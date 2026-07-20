using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Dapper;
using System.Text.Json;

namespace MFinlogs.Desktop.LocalServer;

/// <summary>
/// Embedded ASP.NET Minimal API server for offline mode.
/// Mimics the Python FastAPI backend using local SQLite.
/// </summary>
public class LocalApiServer
{
    private readonly int _port;
    private readonly string _dbPath;
    private WebApplication? _app;
    private Task? _runTask;
    private CancellationTokenSource? _cts;
    private string _currentCompany = "default";

    public LocalApiServer(int port, string dbPath)
    {
        _port = port;
        _dbPath = dbPath;
    }

    public void Start()
    {
        _cts = new CancellationTokenSource();

        var builder = WebApplication.CreateBuilder();
        builder.WebHost.UseUrls($"http://localhost:{_port}");
        builder.Services.AddCors();

        _app = builder.Build();

        // CORS - allow all for local use
        _app.UseCors(policy => policy
            .AllowAnyOrigin()
            .AllowAnyMethod()
            .AllowAnyHeader());

        RegisterEndpoints(_app);

        _runTask = _app.RunAsync(_cts.Token);
    }

    public void Stop()
    {
        _cts?.Cancel();
        try { _runTask?.Wait(TimeSpan.FromSeconds(5)); } catch { }
        _app?.DisposeAsync().AsTask().Wait(TimeSpan.FromSeconds(3));
    }


    private void RegisterEndpoints(WebApplication app)
    {
        // === AUTH ===
        app.MapPost("/api/auth/login", HandleLogin);
        app.MapPost("/api/auth/logout", () => Results.Ok(new { status = "Logged out" }));
        app.MapGet("/api/setup/status", HandleSetupStatus);
        app.MapPost("/api/setup/admin", HandleSetupAdmin);

        // === TRANSACTIONS ===
        app.MapGet("/api/transactions", HandleGetTransactions);
        app.MapPost("/api/transactions", HandleAddTransaction);
        app.MapPut("/api/transactions/{id}", HandleEditTransaction);
        app.MapDelete("/api/transactions/{id}", HandleDeleteTransaction);

        // === PARTIES ===
        app.MapGet("/api/parties", HandleGetParties);
        app.MapPost("/api/parties", HandleCreateParty);

        // === DASHBOARD ===
        app.MapGet("/api/dashboard", HandleDashboard);
        app.MapGet("/api/dashboard/trend", HandleDashboardTrend);
        app.MapGet("/api/dashboard/sparkline", HandleSparkline);

        // === REPORTS ===
        app.MapGet("/api/reports/ledger", HandleLedger);
        app.MapGet("/api/reports/day-book", HandleDayBook);
        app.MapGet("/api/reports/outstanding", HandleOutstanding);
        app.MapGet("/api/reports/trial-balance", HandleTrialBalance);
        app.MapGet("/api/reports/profit-loss", HandleProfitLoss);
        app.MapGet("/api/reports/daily-summary", HandleDailySummary);

        // === INVENTORY ===
        app.MapGet("/api/inventory/snapshot", HandleGetInventory);
        app.MapPost("/api/inventory/snapshot", HandleSaveInventory);

        // === CASH ===
        app.MapGet("/api/cash-in-hand", HandleGetCash);
        app.MapPost("/api/cash-in-hand", HandleSetCash);

        // === FINANCIAL YEARS ===
        app.MapGet("/api/financial-years", HandleFinancialYears);
        app.MapPost("/api/financial-years/select", HandleSelectFY);

        // === USERS ===
        app.MapGet("/api/users", HandleGetUsers);
        app.MapPost("/api/users", HandleCreateUser);

        // === AUDIT ===
        app.MapGet("/api/audit", HandleAudit);

        // === NOTIFICATIONS & ACTIVITY ===
        app.MapGet("/api/notifications", HandleNotifications);
        app.MapGet("/api/activity", HandleActivity);

        // === BACKUP ===
        app.MapPost("/api/backup", HandleBackup);
        app.MapPost("/api/restore", HandleRestore);
    }


    #region Auth Endpoints

    private IResult HandleLogin(HttpContext ctx)
    {
        var body = ReadBody<LoginRequest>(ctx);
        if (body == null) return Results.BadRequest(new { detail = "Invalid request" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var user = conn.QueryFirstOrDefault<dynamic>(
            "SELECT username, password_hash, role FROM users WHERE username = @username",
            new { username = body.Username });

        if (user == null)
            return Results.Json(new { detail = "Invalid Credentials" }, statusCode: 401);

        string? storedHash = user.password_hash;
        if (!SqliteDb.VerifyPassword(storedHash, body.Password))
            return Results.Json(new { detail = "Invalid Credentials" }, statusCode: 401);

        SqliteDb.LogAudit(conn, body.Username, "Login", "User logged in", _currentCompany);

        return Results.Ok(new
        {
            status = "Success",
            username = (string)user.username,
            role = (string)user.role,
            access_token = "local_token_" + Guid.NewGuid().ToString("N"),
            token_type = "bearer"
        });
    }

    private IResult HandleSetupStatus()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();
        var hash = conn.ExecuteScalar<string?>(
            "SELECT password_hash FROM users WHERE username = 'admin'");
        return Results.Ok(new { needs_setup = string.IsNullOrEmpty(hash) });
    }

    private IResult HandleSetupAdmin(HttpContext ctx)
    {
        var body = ReadBody<SetupRequest>(ctx);
        if (body == null || string.IsNullOrEmpty(body.Password) || body.Password.Length < 8)
            return Results.BadRequest(new { detail = "Password must be at least 8 characters" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();
        var hash = SqliteDb.HashPassword(body.Password);
        conn.Execute("UPDATE users SET password_hash = @hash WHERE username = 'admin'", new { hash });

        return Results.Ok(new { status = "Setup complete" });
    }

    #endregion


    #region Transaction Endpoints

    private IResult HandleGetTransactions(HttpContext ctx)
    {
        int page = int.TryParse(ctx.Request.Query["page"], out var p) ? p : 1;
        int limit = int.TryParse(ctx.Request.Query["limit"], out var l) ? l : 50;
        string? fromDate = ctx.Request.Query["from_date"];
        string? toDate = ctx.Request.Query["to_date"];

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var fy = SqliteDb.GetSelectedFinancialYear(conn, _currentCompany);
        var (fyStart, fyEnd) = SqliteDb.ParseFinancialYear(fy);

        var where = "WHERE t.txn_date >= @fyStart AND t.txn_date <= @fyEnd AND t.company_id = @company";
        var parms = new DynamicParameters();
        parms.Add("fyStart", fyStart.ToString("yyyy-MM-dd"));
        parms.Add("fyEnd", fyEnd.ToString("yyyy-MM-dd"));
        parms.Add("company", _currentCompany);

        if (!string.IsNullOrEmpty(fromDate)) { where += " AND t.txn_date >= @from"; parms.Add("from", fromDate); }
        if (!string.IsNullOrEmpty(toDate)) { where += " AND t.txn_date <= @to"; parms.Add("to", toDate); }

        var total = conn.ExecuteScalar<int>($"SELECT COUNT(*) FROM transactions t {where}", parms);
        int offset = (page - 1) * limit;
        parms.Add("offset", offset);
        parms.Add("limit", limit);

        var rows = conn.Query<dynamic>($@"
            SELECT t.txn_id as id, t.txn_date as date, t.bill_no, p.name as party,
                   t.txn_type as type, t.payment_mode as mode, t.amount
            FROM transactions t
            JOIN parties p ON t.party_id = p.party_id
            {where}
            ORDER BY t.txn_date DESC, t.txn_id DESC
            LIMIT @limit OFFSET @offset
        ", parms).ToList();

        return Results.Ok(new
        {
            transactions = rows,
            page,
            limit,
            total,
            total_pages = (total + limit - 1) / limit
        });
    }

    private IResult HandleAddTransaction(HttpContext ctx)
    {
        var body = ReadBody<TransactionRequest>(ctx);
        if (body == null) return Results.BadRequest(new { detail = "Invalid request" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var partyId = SqliteDb.GetOrCreateParty(conn, body.Party, "Customer", true, _currentCompany);
        var txnDate = string.IsNullOrEmpty(body.Date) ? DateTime.Today : DateTime.Parse(body.Date);
        var txnFy = SqliteDb.GetFinancialYear(txnDate);

        conn.Execute(@"
            INSERT INTO transactions (txn_date, bill_no, party_id, txn_type, payment_mode, financial_year, amount, company_id)
            VALUES (@date, @bill, @partyId, @type, @mode, @fy, @amount, @company)
        ", new { date = txnDate.ToString("yyyy-MM-dd"), bill = body.BillNo ?? "", partyId, type = body.TxnType, mode = body.Mode, fy = txnFy, amount = body.Amount, company = _currentCompany });

        return Results.Ok(new { status = "Transaction Added" });
    }

    private IResult HandleEditTransaction(int id, HttpContext ctx)
    {
        var body = ReadBody<EditTransactionRequest>(ctx);
        if (body == null) return Results.BadRequest(new { detail = "Invalid request" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var valid = new[] { "txn_date", "bill_no", "txn_type", "payment_mode", "amount", "party" };
        if (!valid.Contains(body.Field))
            return Results.BadRequest(new { detail = "Invalid field" });

        if (body.Field == "party")
        {
            var normalized = SqliteDb.NormalizePartyName(body.NewValue);
            var partyId = conn.ExecuteScalar<int?>(
                "SELECT party_id FROM parties WHERE normalized_name = @n AND company_id = @c",
                new { n = normalized, c = _currentCompany });
            if (!partyId.HasValue) return Results.NotFound(new { detail = "Party not found" });
            conn.Execute("UPDATE transactions SET party_id = @pid WHERE txn_id = @id", new { pid = partyId.Value, id });
        }
        else
        {
            conn.Execute($"UPDATE transactions SET {body.Field} = @val WHERE txn_id = @id",
                new { val = body.NewValue, id });
        }

        return Results.Ok(new { status = "Updated Successfully" });
    }

    private IResult HandleDeleteTransaction(int id)
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();
        conn.Execute("DELETE FROM transactions WHERE txn_id = @id", new { id });
        return Results.Ok(new { status = "Deleted Successfully" });
    }

    #endregion


    #region Party Endpoints

    private IResult HandleGetParties()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();
        var parties = conn.Query<dynamic>(
            "SELECT name, type FROM parties WHERE company_id = @c ORDER BY name",
            new { c = _currentCompany }).ToList();
        return Results.Ok(parties);
    }

    private IResult HandleCreateParty(HttpContext ctx)
    {
        var body = ReadBody<PartyRequest>(ctx);
        if (body == null) return Results.BadRequest(new { detail = "Invalid request" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        if (body.Type == "Bank")
        {
            var bankCount = conn.ExecuteScalar<int>(
                "SELECT COUNT(*) FROM parties WHERE type = 'Bank' AND company_id = @c",
                new { c = _currentCompany });
            if (bankCount > 0)
                return Results.BadRequest(new { detail = "Only one Bank account is allowed." });
        }

        SqliteDb.GetOrCreateParty(conn, body.Name, body.Type, body.Credit, _currentCompany);
        return Results.Ok(new { status = "Party Created" });
    }

    #endregion

    #region Dashboard Endpoints

    private IResult HandleDashboard()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var fy = SqliteDb.GetSelectedFinancialYear(conn, _currentCompany);
        var (fyStart, fyEnd) = SqliteDb.ParseFinancialYear(fy);
        var today = DateTime.Today.ToString("yyyy-MM-dd");
        var monthStart = new DateTime(DateTime.Today.Year, DateTime.Today.Month, 1).ToString("yyyy-MM-dd");

        var salesToday = conn.ExecuteScalar<double>(@"
            SELECT COALESCE(SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type='Sale Return' THEN -amount ELSE 0 END), 0)
            FROM transactions WHERE txn_date = @today AND company_id = @c",
            new { today, c = _currentCompany });

        var salesMonth = conn.ExecuteScalar<double>(@"
            SELECT COALESCE(SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type='Sale Return' THEN -amount ELSE 0 END), 0)
            FROM transactions WHERE txn_date >= @monthStart AND txn_date <= @today AND company_id = @c",
            new { monthStart, today, c = _currentCompany });

        var cashBal = conn.ExecuteScalar<double>(@"
            SELECT COALESCE(SUM(CASE WHEN payment_mode='Cash' AND txn_type IN ('Sale','Receipt') THEN amount ELSE 0 END), 0)
             - COALESCE(SUM(CASE WHEN payment_mode='Cash' AND txn_type IN ('Expense','Sale Return') THEN amount ELSE 0 END), 0)
            FROM transactions WHERE txn_date >= @s AND txn_date <= @e AND company_id = @c",
            new { s = fyStart.ToString("yyyy-MM-dd"), e = fyEnd.ToString("yyyy-MM-dd"), c = _currentCompany });

        var bankBal = conn.ExecuteScalar<double>(@"
            SELECT COALESCE(SUM(CASE WHEN payment_mode IN ('Bank','UPI') AND txn_type IN ('Sale','Receipt') THEN amount ELSE 0 END), 0)
             - COALESCE(SUM(CASE WHEN payment_mode IN ('Bank','UPI') AND txn_type IN ('Expense','Sale Return') THEN amount ELSE 0 END), 0)
            FROM transactions WHERE txn_date >= @s AND txn_date <= @e AND company_id = @c",
            new { s = fyStart.ToString("yyyy-MM-dd"), e = fyEnd.ToString("yyyy-MM-dd"), c = _currentCompany });

        var receivables = conn.ExecuteScalar<double>(@"
            SELECT COALESCE(SUM(CASE WHEN t.txn_type='Sale' THEN t.amount ELSE 0 END), 0)
             - COALESCE(SUM(CASE WHEN t.txn_type IN ('Receipt','Sale Return') THEN t.amount ELSE 0 END), 0)
            FROM transactions t JOIN parties p ON t.party_id=p.party_id
            WHERE p.type='Credit Customer' AND t.txn_date >= @s AND t.txn_date <= @e AND t.company_id = @c",
            new { s = fyStart.ToString("yyyy-MM-dd"), e = fyEnd.ToString("yyyy-MM-dd"), c = _currentCompany });

        return Results.Ok(new
        {
            sales_today = salesToday,
            sales_month = salesMonth,
            cash_balance = cashBal,
            bank_balance = bankBal,
            receivables
        });
    }

    private IResult HandleDashboardTrend()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var rows = conn.Query<dynamic>(@"
            SELECT txn_date as date,
                   SUM(CASE WHEN txn_type='Sale' THEN amount ELSE 0 END) as sales,
                   SUM(CASE WHEN txn_type='Expense' THEN amount ELSE 0 END) as expenses
            FROM transactions
            WHERE txn_date >= date('now', '-30 days') AND company_id = @c
            GROUP BY txn_date ORDER BY txn_date",
            new { c = _currentCompany }).ToList();

        return Results.Ok(rows);
    }

    private IResult HandleSparkline()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var rows = conn.Query<double>(@"
            SELECT COALESCE(SUM(CASE WHEN txn_type='Sale' THEN amount ELSE 0 END), 0)
            FROM transactions
            WHERE txn_date >= date('now', '-7 days') AND company_id = @c
            GROUP BY txn_date ORDER BY txn_date",
            new { c = _currentCompany }).ToList();

        return Results.Ok(rows);
    }

    #endregion


    #region Report Endpoints

    private IResult HandleLedger(HttpContext ctx)
    {
        string? party = ctx.Request.Query["party"];
        string? start = ctx.Request.Query["start"];
        string? end = ctx.Request.Query["end"];

        if (string.IsNullOrEmpty(party))
            return Results.BadRequest(new { detail = "Party parameter required" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var normalized = SqliteDb.NormalizePartyName(party);
        var partyRow = conn.QueryFirstOrDefault<dynamic>(
            "SELECT party_id FROM parties WHERE normalized_name = @n AND company_id = @c",
            new { n = normalized, c = _currentCompany });

        if (partyRow == null)
            return Results.Ok(new { data = Array.Empty<object>(), opening_balance = 0.0 });

        int partyId = (int)(long)partyRow.party_id;
        var fy = SqliteDb.GetSelectedFinancialYear(conn, _currentCompany);

        var where = "WHERE t.party_id = @pid";
        var parms = new DynamicParameters();
        parms.Add("pid", partyId);

        if (!string.IsNullOrEmpty(start)) { where += " AND t.txn_date >= @start"; parms.Add("start", start); }
        if (!string.IsNullOrEmpty(end)) { where += " AND t.txn_date <= @end"; parms.Add("end", end); }

        // Opening balance
        double openingBalance = 0;
        if (!string.IsNullOrEmpty(start))
        {
            openingBalance = conn.ExecuteScalar<double>(@"
                SELECT COALESCE(SUM(CASE WHEN UPPER(txn_type)='SALE' THEN amount
                    WHEN UPPER(txn_type) IN ('RECEIPT','SALE RETURN') THEN -amount ELSE 0 END), 0)
                FROM transactions WHERE party_id = @pid AND txn_date < @start",
                new { pid = partyId, start });
        }

        var txns = conn.Query<dynamic>($@"
            SELECT t.txn_id as id, t.txn_date as date, t.bill_no, t.txn_type as type,
                   t.payment_mode as mode, t.amount
            FROM transactions t {where}
            ORDER BY t.txn_date, t.txn_id", parms).ToList();

        double balance = openingBalance;
        var ledger = new List<object>();
        foreach (var txn in txns)
        {
            string ttype = ((string)(txn.type ?? "")).Trim().ToLower();
            double amt = (double)txn.amount;
            if (ttype == "sale") balance += amt;
            else if (ttype == "receipt" || ttype == "sale return") balance -= amt;

            ledger.Add(new
            {
                id = (long)txn.id,
                date = (string)txn.date,
                bill_no = (string)(txn.bill_no ?? ""),
                type = (string)txn.type,
                mode = (string)txn.mode,
                amount = amt,
                balance,
                financial_year = fy
            });
        }

        return Results.Ok(new { data = ledger, opening_balance = openingBalance, financial_year = fy });
    }

    private IResult HandleDayBook(HttpContext ctx)
    {
        string date = ctx.Request.Query["date"].FirstOrDefault() ?? DateTime.Today.ToString("yyyy-MM-dd");

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var rows = conn.Query<dynamic>(@"
            SELECT t.txn_id as id, t.txn_date as date, t.bill_no, p.name as party,
                   t.txn_type as type, t.payment_mode as mode, t.amount
            FROM transactions t JOIN parties p ON t.party_id = p.party_id
            WHERE t.txn_date = @date AND t.company_id = @c
            ORDER BY t.txn_id DESC",
            new { date, c = _currentCompany }).ToList();

        return Results.Ok(rows);
    }

    private IResult HandleOutstanding()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var fy = SqliteDb.GetSelectedFinancialYear(conn, _currentCompany);
        var (fyStart, fyEnd) = SqliteDb.ParseFinancialYear(fy);

        var rows = conn.Query<dynamic>(@"
            SELECT p.name,
                SUM(CASE WHEN UPPER(t.txn_type)='SALE' THEN t.amount ELSE 0 END) as sales,
                SUM(CASE WHEN UPPER(t.txn_type) IN ('RECEIPT','SALE RETURN') THEN t.amount ELSE 0 END) as credits,
                MAX(CASE WHEN UPPER(t.txn_type)='RECEIPT' THEN t.txn_date END) as last_receipt_date
            FROM parties p
            LEFT JOIN transactions t ON p.party_id = t.party_id
            WHERE p.type = 'Credit Customer' AND p.company_id = @c
              AND t.txn_date >= @s AND t.txn_date <= @e
            GROUP BY p.name",
            new { c = _currentCompany, s = fyStart.ToString("yyyy-MM-dd"), e = fyEnd.ToString("yyyy-MM-dd") }).ToList();

        var outstanding = new List<object>();
        double totalOutstanding = 0;

        foreach (var r in rows)
        {
            double sales = (double)(r.sales ?? 0);
            double credits = (double)(r.credits ?? 0);
            double balance = sales - credits;
            if (balance <= 0) continue;

            string? lastReceipt = r.last_receipt_date;
            int daysUnpaid = 0;
            if (!string.IsNullOrEmpty(lastReceipt) && DateTime.TryParse(lastReceipt, out var lrd))
                daysUnpaid = (DateTime.Today - lrd).Days;

            string riskLevel = daysUnpaid >= 30 ? "critical" : daysUnpaid >= 15 ? "high" : "normal";

            outstanding.Add(new
            {
                party = (string)r.name,
                balance,
                last_receipt_date = lastReceipt,
                days_unpaid = daysUnpaid,
                risk_level = riskLevel
            });
            totalOutstanding += balance;
        }

        return Results.Ok(new { data = outstanding, total = totalOutstanding });
    }

    private IResult HandleTrialBalance()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var fy = SqliteDb.GetSelectedFinancialYear(conn, _currentCompany);
        var (fyStart, fyEnd) = SqliteDb.ParseFinancialYear(fy);
        var s = fyStart.ToString("yyyy-MM-dd");
        var e = fyEnd.ToString("yyyy-MM-dd");

        var cashIn = conn.ExecuteScalar<double>(@"SELECT COALESCE(SUM(amount),0) FROM transactions
            WHERE payment_mode='Cash' AND txn_type IN ('Sale','Receipt') AND txn_date>=@s AND txn_date<=@e AND company_id=@c", new { s, e, c = _currentCompany });
        var cashOut = conn.ExecuteScalar<double>(@"SELECT COALESCE(SUM(amount),0) FROM transactions
            WHERE payment_mode='Cash' AND txn_type IN ('Expense','Sale Return') AND txn_date>=@s AND txn_date<=@e AND company_id=@c", new { s, e, c = _currentCompany });
        var bankIn = conn.ExecuteScalar<double>(@"SELECT COALESCE(SUM(amount),0) FROM transactions
            WHERE payment_mode IN ('Bank','UPI') AND txn_type IN ('Sale','Receipt') AND txn_date>=@s AND txn_date<=@e AND company_id=@c", new { s, e, c = _currentCompany });
        var bankOut = conn.ExecuteScalar<double>(@"SELECT COALESCE(SUM(amount),0) FROM transactions
            WHERE payment_mode IN ('Bank','UPI') AND txn_type IN ('Expense','Sale Return') AND txn_date>=@s AND txn_date<=@e AND company_id=@c", new { s, e, c = _currentCompany });

        var debtors = conn.ExecuteScalar<double>(@"
            SELECT COALESCE(SUM(CASE WHEN t.txn_type='Sale' THEN t.amount ELSE 0 END),0)
             - COALESCE(SUM(CASE WHEN t.txn_type IN ('Receipt','Sale Return') THEN t.amount ELSE 0 END),0)
            FROM transactions t JOIN parties p ON t.party_id=p.party_id
            WHERE p.type='Credit Customer' AND t.txn_date>=@s AND t.txn_date<=@e AND t.company_id=@c", new { s, e, c = _currentCompany });

        var totalSales = conn.ExecuteScalar<double>(@"SELECT COALESCE(SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type='Sale Return' THEN -amount ELSE 0 END),0)
            FROM transactions WHERE txn_date>=@s AND txn_date<=@e AND company_id=@c", new { s, e, c = _currentCompany });
        var totalExpenses = conn.ExecuteScalar<double>(@"SELECT COALESCE(SUM(amount),0) FROM transactions
            WHERE txn_type='Expense' AND txn_date>=@s AND txn_date<=@e AND company_id=@c", new { s, e, c = _currentCompany });

        double cashBal = cashIn - cashOut, bankBal = bankIn - bankOut;

        return Results.Ok(new object[]
        {
            new { account = "Cash Account", debit = cashBal > 0 ? cashBal : 0, credit = cashBal < 0 ? -cashBal : 0 },
            new { account = "Bank Account", debit = bankBal > 0 ? bankBal : 0, credit = bankBal < 0 ? -bankBal : 0 },
            new { account = "Sundry Debtors", debit = debtors > 0 ? debtors : 0, credit = debtors < 0 ? -debtors : 0 },
            new { account = "Sales Account", debit = 0.0, credit = totalSales },
            new { account = "Expense Account", debit = totalExpenses, credit = 0.0 }
        });
    }

    private IResult HandleProfitLoss()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var fy = SqliteDb.GetSelectedFinancialYear(conn, _currentCompany);
        var (fyStart, fyEnd) = SqliteDb.ParseFinancialYear(fy);
        var s = fyStart.ToString("yyyy-MM-dd");
        var e = fyEnd.ToString("yyyy-MM-dd");

        var sales = conn.ExecuteScalar<double>(@"SELECT COALESCE(SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type='Sale Return' THEN -amount ELSE 0 END),0)
            FROM transactions WHERE txn_date>=@s AND txn_date<=@e AND company_id=@c", new { s, e, c = _currentCompany });
        var expenses = conn.ExecuteScalar<double>(@"SELECT COALESCE(SUM(amount),0) FROM transactions
            WHERE txn_type='Expense' AND txn_date>=@s AND txn_date<=@e AND company_id=@c", new { s, e, c = _currentCompany });

        return Results.Ok(new { sales, expenses, net_profit = sales - expenses });
    }

    private IResult HandleDailySummary(HttpContext ctx)
    {
        string? start = ctx.Request.Query["start"];
        string? end = ctx.Request.Query["end"];

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var endDate = string.IsNullOrEmpty(end) ? DateTime.Today : DateTime.Parse(end);
        var startDate = string.IsNullOrEmpty(start) ? endDate.AddDays(-29) : DateTime.Parse(start);

        var rows = conn.Query<dynamic>(@"
            SELECT txn_date as date,
                SUM(CASE WHEN txn_type='Sale' THEN amount WHEN txn_type='Sale Return' THEN -amount ELSE 0 END) as total_sales,
                SUM(CASE WHEN payment_mode='Cash' AND txn_type IN ('Sale','Receipt') THEN amount ELSE 0 END) as cash_in,
                SUM(CASE WHEN payment_mode='Cash' AND txn_type IN ('Expense','Sale Return') THEN amount ELSE 0 END) as cash_expense,
                SUM(CASE WHEN payment_mode IN ('Bank','UPI') AND txn_type IN ('Sale','Receipt') THEN amount ELSE 0 END) as bank
            FROM transactions
            WHERE txn_date >= @s AND txn_date <= @e AND company_id = @c
            GROUP BY txn_date ORDER BY txn_date DESC",
            new { s = startDate.ToString("yyyy-MM-dd"), e = endDate.ToString("yyyy-MM-dd"), c = _currentCompany }).ToList();

        return Results.Ok(rows);
    }

    #endregion


    #region Inventory Endpoints

    private IResult HandleGetInventory(HttpContext ctx)
    {
        string? financialYear = ctx.Request.Query["financial_year"];
        int month = int.TryParse(ctx.Request.Query["month"], out var m) ? m : DateTime.Today.Month;

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var fy = string.IsNullOrEmpty(financialYear)
            ? SqliteDb.GetSelectedFinancialYear(conn, _currentCompany)
            : financialYear;

        var items = conn.Query<dynamic>(@"
            SELECT id, client_row_id, name, cost, min_stock
            FROM inventory_items WHERE company_id = @c ORDER BY id",
            new { c = _currentCompany }).ToList();

        var rows = new List<object>();
        foreach (var item in items)
        {
            int itemId = (int)(long)item.id;
            var qtys = conn.Query<dynamic>(@"
                SELECT day, qty FROM inventory_quantities
                WHERE item_id=@id AND financial_year=@fy AND month=@m AND company_id=@c",
                new { id = itemId, fy, m = month, c = _currentCompany }).ToList();

            var purchases = conn.Query<dynamic>(@"
                SELECT day, qty FROM inventory_purchases
                WHERE item_id=@id AND financial_year=@fy AND month=@m AND company_id=@c",
                new { id = itemId, fy, m = month, c = _currentCompany }).ToList();

            var qtyArr = new double[31];
            var purchArr = new double[31];
            foreach (var q in qtys) { int d = (int)(long)q.day; if (d >= 1 && d <= 31) qtyArr[d - 1] = (double)q.qty; }
            foreach (var p in purchases) { int d = (int)(long)p.day; if (d >= 1 && d <= 31) purchArr[d - 1] = (double)p.qty; }

            rows.Add(new
            {
                id = (string)(item.client_row_id ?? $"inv-{itemId}"),
                name = (string)item.name,
                cost = (double)(item.cost ?? 0),
                min_stock = (double)(item.min_stock ?? 0),
                qty = qtyArr,
                purchase_qty = purchArr
            });
        }

        return Results.Ok(new { found = rows.Count > 0, financial_year = fy, month, rows });
    }

    private IResult HandleSaveInventory(HttpContext ctx)
    {
        var body = ReadBody<InventorySnapshotRequest>(ctx);
        if (body == null) return Results.BadRequest(new { detail = "Invalid request" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        using var transaction = conn.BeginTransaction();

        foreach (var row in body.Rows ?? new List<InventoryRow>())
        {
            if (string.IsNullOrWhiteSpace(row.Name)) continue;

            var rowId = string.IsNullOrEmpty(row.Id) ? $"inv-{Guid.NewGuid():N}" : row.Id;

            // Upsert item
            var existing = conn.ExecuteScalar<int?>(
                "SELECT id FROM inventory_items WHERE client_row_id=@rid AND company_id=@c",
                new { rid = rowId, c = _currentCompany }, transaction);

            int itemId;
            if (existing.HasValue)
            {
                itemId = existing.Value;
                conn.Execute(@"UPDATE inventory_items SET name=@name, cost=@cost, min_stock=@ms, updated_at=datetime('now')
                    WHERE id=@id", new { name = row.Name, cost = row.Cost, ms = row.MinStock, id = itemId }, transaction);
            }
            else
            {
                conn.Execute(@"INSERT INTO inventory_items (client_row_id, name, cost, min_stock, company_id)
                    VALUES (@rid, @name, @cost, @ms, @c)",
                    new { rid = rowId, name = row.Name, cost = row.Cost, ms = row.MinStock, c = _currentCompany }, transaction);
                itemId = (int)conn.ExecuteScalar<long>("SELECT last_insert_rowid()", transaction: transaction);
            }

            // Clear and rewrite quantities
            conn.Execute("DELETE FROM inventory_quantities WHERE item_id=@id AND financial_year=@fy AND month=@m AND company_id=@c",
                new { id = itemId, fy = body.FinancialYear, m = body.Month, c = _currentCompany }, transaction);
            conn.Execute("DELETE FROM inventory_purchases WHERE item_id=@id AND financial_year=@fy AND month=@m AND company_id=@c",
                new { id = itemId, fy = body.FinancialYear, m = body.Month, c = _currentCompany }, transaction);

            for (int i = 0; i < (row.Qty?.Count ?? 0); i++)
            {
                conn.Execute(@"INSERT INTO inventory_quantities (item_id, financial_year, month, day, qty, company_id)
                    VALUES (@id, @fy, @m, @d, @q, @c)",
                    new { id = itemId, fy = body.FinancialYear, m = body.Month, d = i + 1, q = row.Qty![i], c = _currentCompany }, transaction);
            }
            for (int i = 0; i < (row.PurchaseQty?.Count ?? 0); i++)
            {
                if (row.PurchaseQty![i] > 0)
                    conn.Execute(@"INSERT INTO inventory_purchases (item_id, financial_year, month, day, qty, company_id)
                        VALUES (@id, @fy, @m, @d, @q, @c)",
                        new { id = itemId, fy = body.FinancialYear, m = body.Month, d = i + 1, q = row.PurchaseQty[i], c = _currentCompany }, transaction);
            }
        }

        transaction.Commit();
        return Results.Ok(new { status = "Saved" });
    }

    #endregion


    #region Cash, Financial Years, Users, Audit, Notifications

    private IResult HandleGetCash()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();
        var val = conn.ExecuteScalar<double?>(
            "SELECT CAST(setting_value AS REAL) FROM app_settings WHERE setting_key='opening_cash_seed' AND company_id=@c",
            new { c = _currentCompany });
        return Results.Ok(new { opening_cash = val ?? 0.0 });
    }

    private IResult HandleSetCash(HttpContext ctx)
    {
        var body = ReadBody<CashRequest>(ctx);
        if (body == null) return Results.BadRequest(new { detail = "Invalid request" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        if (!string.IsNullOrEmpty(body.Date))
        {
            conn.Execute(@"INSERT INTO daily_cash (cash_date, cash_in_hand, company_id)
                VALUES (@date, @amount, @c)
                ON CONFLICT(cash_date, company_id) DO UPDATE SET cash_in_hand=@amount, updated_at=datetime('now')",
                new { date = body.Date, amount = body.CashInHand, c = _currentCompany });
        }
        else
        {
            SqliteDb.SetSetting(conn, "opening_cash_seed", body.CashInHand.ToString(), _currentCompany);
        }
        return Results.Ok(new { status = "Saved" });
    }

    private IResult HandleFinancialYears()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var selected = SqliteDb.GetSelectedFinancialYear(conn, _currentCompany);
        var years = conn.Query<string>(@"
            SELECT DISTINCT financial_year FROM transactions
            WHERE financial_year IS NOT NULL AND company_id=@c
            ORDER BY financial_year DESC", new { c = _currentCompany }).ToList();

        var currentFy = SqliteDb.GetFinancialYear(DateTime.Today);
        var merged = new List<string>();
        foreach (var y in new[] { selected, currentFy }.Concat(years))
        {
            if (!string.IsNullOrEmpty(y) && !merged.Contains(y)) merged.Add(y);
        }
        merged.Sort(); merged.Reverse();

        return Results.Ok(new { years = merged, selected });
    }

    private IResult HandleSelectFY(HttpContext ctx)
    {
        var body = ReadBody<SelectFYRequest>(ctx);
        if (body == null || string.IsNullOrEmpty(body.Year))
            return Results.BadRequest(new { detail = "Financial year required" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();
        SqliteDb.SetSetting(conn, "active_financial_year", body.Year, _currentCompany);
        return Results.Ok(new { status = "Selected", financial_year = body.Year });
    }

    private IResult HandleGetUsers()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();
        var users = conn.Query<dynamic>("SELECT username, role FROM users").ToList();
        return Results.Ok(users);
    }

    private IResult HandleCreateUser(HttpContext ctx)
    {
        var body = ReadBody<CreateUserRequest>(ctx);
        if (body == null) return Results.BadRequest(new { detail = "Invalid request" });

        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var exists = conn.ExecuteScalar<int>("SELECT COUNT(*) FROM users WHERE username=@u", new { u = body.Username });
        if (exists > 0) return Results.BadRequest(new { detail = "User already exists" });

        var hash = SqliteDb.HashPassword(body.Password);
        conn.Execute("INSERT INTO users (username, password_hash, role) VALUES (@u, @h, @r)",
            new { u = body.Username, h = hash, r = body.Role });
        return Results.Ok(new { status = "User Created" });
    }

    private IResult HandleAudit()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();
        var logs = conn.Query<dynamic>(@"
            SELECT timestamp, username, action, details FROM audit_logs
            WHERE company_id=@c ORDER BY timestamp DESC LIMIT 200",
            new { c = _currentCompany }).ToList();
        return Results.Ok(logs);
    }

    private IResult HandleNotifications()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        // Get overdue parties count
        var overdueCount = conn.ExecuteScalar<int>(@"
            SELECT COUNT(*) FROM (
                SELECT p.name,
                    SUM(CASE WHEN t.txn_type='Sale' THEN t.amount ELSE 0 END)
                    - SUM(CASE WHEN t.txn_type IN ('Receipt','Sale Return') THEN t.amount ELSE 0 END) as balance
                FROM parties p JOIN transactions t ON p.party_id=t.party_id
                WHERE p.type='Credit Customer' AND p.company_id=@c
                GROUP BY p.name HAVING balance > 0
            )", new { c = _currentCompany });

        var notifications = new List<object>();
        if (overdueCount > 0)
        {
            notifications.Add(new { type = "warning", message = $"{overdueCount} parties have outstanding balances", count = overdueCount });
        }

        return Results.Ok(notifications);
    }

    private IResult HandleActivity()
    {
        using var conn = SqliteDb.GetConnection();
        conn.Open();

        var activity = conn.Query<dynamic>(@"
            SELECT t.txn_date as date, p.name as party, t.txn_type as type, t.amount
            FROM transactions t JOIN parties p ON t.party_id=p.party_id
            WHERE t.company_id=@c
            ORDER BY t.txn_date DESC, t.txn_id DESC LIMIT 20",
            new { c = _currentCompany }).ToList();

        return Results.Ok(activity);
    }

    #endregion


    #region Backup/Restore

    private IResult HandleBackup()
    {
        try
        {
            var path = SqliteDb.AutoBackup();
            return Results.Ok(new { status = "Backup Successful", path });
        }
        catch (Exception ex)
        {
            return Results.Json(new { detail = $"Backup failed: {ex.Message}" }, statusCode: 500);
        }
    }

    private IResult HandleRestore(HttpContext ctx)
    {
        var body = ReadBody<RestoreRequest>(ctx);
        if (body == null || string.IsNullOrEmpty(body.Path))
            return Results.BadRequest(new { detail = "Backup path required" });

        try
        {
            SqliteDb.Restore(body.Path);
            return Results.Ok(new { status = "Restore Successful" });
        }
        catch (Exception ex)
        {
            return Results.Json(new { detail = $"Restore failed: {ex.Message}" }, statusCode: 500);
        }
    }

    #endregion

    #region Helpers

    private static T? ReadBody<T>(HttpContext ctx) where T : class
    {
        try
        {
            using var reader = new StreamReader(ctx.Request.Body);
            var json = reader.ReadToEndAsync().Result;
            return JsonSerializer.Deserialize<T>(json, new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true
            });
        }
        catch { return null; }
    }

    #endregion

    #region Request Models

    private record LoginRequest(string Username, string Password);
    private record SetupRequest(string Password);
    private record TransactionRequest(string Date, string? BillNo, string Party, string TxnType, string Mode, double Amount);
    private record EditTransactionRequest(string Field, string NewValue);
    private record PartyRequest(string Name, string Type, bool Credit);
    private record CashRequest(string? Date, double CashInHand);
    private record SelectFYRequest(string Year);
    private record CreateUserRequest(string Username, string Password, string Role);
    private record RestoreRequest(string Path);

    private class InventorySnapshotRequest
    {
        public string FinancialYear { get; set; } = "";
        public int Month { get; set; }
        public List<InventoryRow>? Rows { get; set; }
    }

    private class InventoryRow
    {
        public string? Id { get; set; }
        public string Name { get; set; } = "";
        public List<double>? Qty { get; set; }
        public List<double>? PurchaseQty { get; set; }
        public double MinStock { get; set; }
        public double Cost { get; set; }
    }

    #endregion
}
