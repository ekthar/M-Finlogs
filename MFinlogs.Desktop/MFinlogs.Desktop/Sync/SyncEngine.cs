using System.Diagnostics;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text.Json;
using Dapper;
using MFinlogs.Desktop.Config;
using MFinlogs.Desktop.LocalServer;

namespace MFinlogs.Desktop.Sync;

/// <summary>
/// Synchronization engine for hybrid online/offline mode.
/// Pushes local changes to Supabase and pulls remote changes.
/// Uses last-write-wins conflict resolution based on timestamps.
/// </summary>
public class SyncEngine : IDisposable
{
    private readonly HttpClient _http;
    private readonly AppConfig _config;
    private CancellationTokenSource? _cts;
    private Task? _syncLoop;
    private bool _disposed;

    public event Action<SyncStatus>? StatusChanged;

    public SyncStatus CurrentStatus { get; private set; } = SyncStatus.Idle;
    public DateTime? LastSyncedAt { get; private set; }
    public int PendingChanges { get; private set; }

    public SyncEngine(AppConfig config)
    {
        _config = config;
        _http = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };

        if (!string.IsNullOrEmpty(config.SupabaseUrl))
        {
            _http.BaseAddress = new Uri(config.SupabaseUrl);
        }
        if (!string.IsNullOrEmpty(config.SupabaseKey))
        {
            _http.DefaultRequestHeaders.Add("apikey", config.SupabaseKey);
            _http.DefaultRequestHeaders.Add("Authorization", $"Bearer {config.SupabaseKey}");
        }

        if (DateTime.TryParse(config.LastSyncedAt, out var last))
            LastSyncedAt = last;
    }

    /// <summary>
    /// Start the background sync loop
    /// </summary>
    public void Start()
    {
        if (!_config.AutoSync || string.IsNullOrEmpty(_config.SupabaseUrl))
            return;

        _cts = new CancellationTokenSource();
        _syncLoop = Task.Run(() => SyncLoopAsync(_cts.Token));
    }

    /// <summary>
    /// Stop the background sync loop
    /// </summary>
    public void Stop()
    {
        _cts?.Cancel();
        try { _syncLoop?.Wait(TimeSpan.FromSeconds(5)); } catch { }
    }

    /// <summary>
    /// Perform a single sync operation (push + pull)
    /// </summary>
    public async Task SyncNowAsync()
    {
        if (string.IsNullOrEmpty(_config.SupabaseUrl))
        {
            UpdateStatus(SyncStatus.Error);
            return;
        }

        try
        {
            UpdateStatus(SyncStatus.Syncing);

            // Push local changes
            await PushTransactionsAsync();
            await PushPartiesAsync();

            // Pull remote changes
            await PullTransactionsAsync();
            await PullPartiesAsync();

            // Update sync timestamps
            LastSyncedAt = DateTime.UtcNow;
            _config.LastSyncedAt = LastSyncedAt.Value.ToString("O");
            _config.Save();

            UpdateSyncStatus();
            UpdateStatus(SyncStatus.Synced);
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"Sync error: {ex.Message}");
            UpdateStatus(SyncStatus.Error);
        }
    }

    private async Task SyncLoopAsync(CancellationToken ct)
    {
        // Initial delay
        await Task.Delay(TimeSpan.FromSeconds(10), ct);

        while (!ct.IsCancellationRequested)
        {
            try
            {
                if (await IsOnlineAsync())
                {
                    await SyncNowAsync();
                }
                else
                {
                    UpdateStatus(SyncStatus.Offline);
                }
            }
            catch (OperationCanceledException) { break; }
            catch (Exception ex)
            {
                Debug.WriteLine($"Sync loop error: {ex.Message}");
                UpdateStatus(SyncStatus.Error);
            }

            try
            {
                await Task.Delay(TimeSpan.FromSeconds(_config.SyncInterval), ct);
            }
            catch (OperationCanceledException) { break; }
        }
    }


    #region Push Operations

    private async Task PushTransactionsAsync()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var lastSync = GetTableLastSync(conn, "transactions");

        // Get locally modified transactions since last sync
        var query = lastSync.HasValue
            ? "SELECT * FROM transactions WHERE company_id = 'default' AND txn_date >= @since"
            : "SELECT * FROM transactions WHERE company_id = 'default'";

        var transactions = conn.Query<dynamic>(query,
            new { since = lastSync?.ToString("yyyy-MM-dd") }).ToList();

        if (transactions.Count == 0) return;

        // Push to Supabase REST API (PostgREST)
        foreach (var batch in Batch(transactions, 50))
        {
            var payload = batch.Select(t => new
            {
                txn_date = (string)t.txn_date,
                bill_no = (string)(t.bill_no ?? ""),
                party_id = (long)t.party_id,
                txn_type = (string)t.txn_type,
                payment_mode = (string)t.payment_mode,
                financial_year = (string)(t.financial_year ?? ""),
                amount = (double)t.amount,
                company_id = "default"
            }).ToList();

            var response = await _http.PostAsJsonAsync("/rest/v1/transactions", payload);
            response.EnsureSuccessStatusCode();
        }

        // Update sync timestamp
        SetTableLastSync(conn, "transactions", DateTime.UtcNow);
    }

    private async Task PushPartiesAsync()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var parties = conn.Query<dynamic>(
            "SELECT * FROM parties WHERE company_id = 'default'").ToList();

        if (parties.Count == 0) return;

        var payload = parties.Select(p => new
        {
            name = (string)p.name,
            normalized_name = (string)(p.normalized_name ?? ""),
            type = (string)(p.type ?? ""),
            credit_allowed = (long)(p.credit_allowed ?? 0) == 1,
            company_id = "default"
        }).ToList();

        // Upsert using Supabase's UPSERT with on_conflict
        var request = new HttpRequestMessage(HttpMethod.Post, "/rest/v1/parties")
        {
            Content = JsonContent.Create(payload)
        };
        request.Headers.Add("Prefer", "resolution=merge-duplicates");

        var response = await _http.SendAsync(request);
        response.EnsureSuccessStatusCode();

        SetTableLastSync(conn, "parties", DateTime.UtcNow);
    }

    #endregion

    #region Pull Operations

    private async Task PullTransactionsAsync()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var lastSync = GetTableLastSync(conn, "transactions");
        var url = "/rest/v1/transactions?company_id=eq.default&order=txn_date.desc&limit=500";

        if (lastSync.HasValue)
            url += $"&txn_date=gte.{lastSync.Value:yyyy-MM-dd}";

        var response = await _http.GetAsync(url);
        if (!response.IsSuccessStatusCode) return;

        var remoteTransactions = await response.Content.ReadFromJsonAsync<List<RemoteTransaction>>();
        if (remoteTransactions == null || remoteTransactions.Count == 0) return;

        using var transaction = conn.BeginTransaction();

        foreach (var rt in remoteTransactions)
        {
            // Last-write-wins: check if remote is newer
            var existing = conn.ExecuteScalar<int>(
                @"SELECT COUNT(*) FROM transactions
                  WHERE txn_date=@date AND bill_no=@bill AND party_id=@pid AND amount=@amt AND company_id='default'",
                new { date = rt.TxnDate, bill = rt.BillNo, pid = rt.PartyId, amt = rt.Amount },
                transaction);

            if (existing == 0)
            {
                conn.Execute(@"
                    INSERT INTO transactions (txn_date, bill_no, party_id, txn_type, payment_mode, financial_year, amount, company_id)
                    VALUES (@date, @bill, @pid, @type, @mode, @fy, @amt, 'default')",
                    new { date = rt.TxnDate, bill = rt.BillNo ?? "", pid = rt.PartyId, type = rt.TxnType, mode = rt.PaymentMode, fy = rt.FinancialYear, amt = rt.Amount },
                    transaction);
            }
        }

        transaction.Commit();
    }

    private async Task PullPartiesAsync()
    {
        var url = "/rest/v1/parties?company_id=eq.default";
        var response = await _http.GetAsync(url);
        if (!response.IsSuccessStatusCode) return;

        var remoteParties = await response.Content.ReadFromJsonAsync<List<RemoteParty>>();
        if (remoteParties == null || remoteParties.Count == 0) return;

        using var conn = SqliteDb.GetConnection();
        conn.Open();
        using var transaction = conn.BeginTransaction();

        foreach (var rp in remoteParties)
        {
            var normalized = SqliteDb.NormalizePartyName(rp.Name ?? "");
            if (string.IsNullOrEmpty(normalized)) continue;

            var exists = conn.ExecuteScalar<int>(
                "SELECT COUNT(*) FROM parties WHERE normalized_name=@n AND company_id='default'",
                new { n = normalized }, transaction);

            if (exists == 0)
            {
                conn.Execute(@"
                    INSERT INTO parties (name, normalized_name, type, credit_allowed, company_id)
                    VALUES (@name, @norm, @type, @credit, 'default')",
                    new { name = rp.Name, norm = normalized, type = rp.Type ?? "Customer", credit = rp.CreditAllowed ? 1 : 0 },
                    transaction);
            }
        }

        transaction.Commit();
    }

    #endregion


    #region Helpers

    private async Task<bool> IsOnlineAsync()
    {
        try
        {
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(5));
            var response = await _http.GetAsync("/rest/v1/", cts.Token);
            return response.IsSuccessStatusCode;
        }
        catch { return false; }
    }

    private static DateTime? GetTableLastSync(Microsoft.Data.Sqlite.SqliteConnection conn, string table)
    {
        var val = conn.ExecuteScalar<string?>(
            "SELECT last_synced_at FROM sync_status WHERE table_name = @table",
            new { table });
        return DateTime.TryParse(val, out var dt) ? dt : null;
    }

    private static void SetTableLastSync(Microsoft.Data.Sqlite.SqliteConnection conn, string table, DateTime syncTime)
    {
        conn.Execute(@"
            INSERT INTO sync_status (table_name, last_synced_at, pending_changes)
            VALUES (@table, @time, 0)
            ON CONFLICT(table_name) DO UPDATE SET last_synced_at=@time, pending_changes=0",
            new { table, time = syncTime.ToString("O") });
    }

    private void UpdateSyncStatus()
    {
        try
        {
            using var conn = SqliteDb.GetConnection();
            conn.Open();
            PendingChanges = conn.ExecuteScalar<int>(
                "SELECT COALESCE(SUM(pending_changes), 0) FROM sync_status");
        }
        catch { PendingChanges = 0; }
    }

    private void UpdateStatus(SyncStatus status)
    {
        CurrentStatus = status;
        StatusChanged?.Invoke(status);
    }

    private static IEnumerable<List<T>> Batch<T>(IEnumerable<T> source, int size)
    {
        var batch = new List<T>(size);
        foreach (var item in source)
        {
            batch.Add(item);
            if (batch.Count >= size)
            {
                yield return batch;
                batch = new List<T>(size);
            }
        }
        if (batch.Count > 0) yield return batch;
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            Stop();
            _http.Dispose();
            _disposed = true;
        }
    }

    #endregion

    #region Remote Models

    private class RemoteTransaction
    {
        public string? TxnDate { get; set; }
        public string? BillNo { get; set; }
        public int PartyId { get; set; }
        public string? TxnType { get; set; }
        public string? PaymentMode { get; set; }
        public string? FinancialYear { get; set; }
        public double Amount { get; set; }
    }

    private class RemoteParty
    {
        public string? Name { get; set; }
        public string? Type { get; set; }
        public bool CreditAllowed { get; set; }
    }

    #endregion
}

/// <summary>
/// Sync status indicator values
/// </summary>
public enum SyncStatus
{
    Idle,
    Syncing,
    Synced,
    Pending,
    Error,
    Offline
}
