using Microsoft.Data.Sqlite;
using Dapper;
using System.Security.Cryptography;
using System.Text;

namespace MFinlogs.Desktop.LocalServer;

/// <summary>
/// SQLite database manager for offline mode.
/// Provides schema initialization, connection management, and data access helpers.
/// </summary>
public static class SqliteDb
{
    private static string _connectionString = "";
    private static string _dbPath = "";

    /// <summary>
    /// Initialize the SQLite database with full schema
    /// </summary>
    public static void Initialize(string dbPath)
    {
        _dbPath = dbPath;
        _connectionString = $"Data Source={dbPath};Cache=Shared";

        Directory.CreateDirectory(Path.GetDirectoryName(dbPath)!);

        using var conn = GetConnection();
        conn.Open();

        // Enable WAL mode for better concurrency
        conn.Execute("PRAGMA journal_mode=WAL;");
        conn.Execute("PRAGMA foreign_keys=ON;");
        conn.Execute("PRAGMA busy_timeout=5000;");

        // Create all tables
        CreateSchema(conn);
        CreateIndexes(conn);
        SeedDefaults(conn);
    }

    /// <summary>
    /// Get a new SQLite connection
    /// </summary>
    public static SqliteConnection GetConnection()
    {
        return new SqliteConnection(_connectionString);
    }

    /// <summary>
    /// Get the database file path
    /// </summary>
    public static string DatabasePath => _dbPath;

    private static void CreateSchema(SqliteConnection conn)
    {
        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS users (
                username TEXT PRIMARY KEY,
                password_hash TEXT,
                role TEXT DEFAULT 'accounts',
                created_at TEXT DEFAULT (datetime('now'))
            );
        ");

        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS companies (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                key TEXT UNIQUE,
                created_at TEXT DEFAULT (datetime('now'))
            );
        ");

        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS parties (
                party_id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                normalized_name TEXT,
                type TEXT,
                credit_allowed INTEGER DEFAULT 0,
                company_id TEXT REFERENCES companies(id),
                UNIQUE(normalized_name, company_id)
            );
        ");

        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS transactions (
                txn_id INTEGER PRIMARY KEY AUTOINCREMENT,
                txn_date TEXT,
                bill_no TEXT,
                party_id INTEGER REFERENCES parties(party_id),
                txn_type TEXT,
                payment_mode TEXT,
                financial_year TEXT,
                amount REAL,
                company_id TEXT REFERENCES companies(id)
            );
        ");

        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS daily_cash (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                cash_date TEXT,
                cash_in_hand REAL,
                updated_at TEXT DEFAULT (datetime('now')),
                company_id TEXT,
                UNIQUE(cash_date, company_id)
            );
        ");

        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS app_settings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                setting_key TEXT,
                setting_value TEXT,
                company_id TEXT,
                UNIQUE(setting_key, company_id)
            );
        ");

        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS inventory_items (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                client_row_id TEXT,
                name TEXT NOT NULL,
                cost REAL DEFAULT 0,
                min_stock REAL DEFAULT 0,
                updated_at TEXT DEFAULT (datetime('now')),
                company_id TEXT
            );
        ");

        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS inventory_quantities (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                item_id INTEGER REFERENCES inventory_items(id) ON DELETE CASCADE,
                financial_year TEXT,
                month INTEGER,
                day INTEGER,
                qty REAL,
                company_id TEXT
            );
        ");

        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS inventory_purchases (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                item_id INTEGER REFERENCES inventory_items(id) ON DELETE CASCADE,
                financial_year TEXT,
                month INTEGER,
                day INTEGER,
                qty REAL,
                company_id TEXT
            );
        ");

        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS audit_logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT DEFAULT (datetime('now')),
                username TEXT REFERENCES users(username),
                action TEXT,
                details TEXT,
                company_id TEXT REFERENCES companies(id)
            );
        ");

        // Sync tracking table
        conn.Execute(@"
            CREATE TABLE IF NOT EXISTS sync_status (
                table_name TEXT PRIMARY KEY,
                last_synced_at TEXT,
                pending_changes INTEGER DEFAULT 0
            );
        ");
    }

    private static void CreateIndexes(SqliteConnection conn)
    {
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_txn_date ON transactions(txn_date DESC, txn_id DESC);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_txn_party ON transactions(party_id);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_txn_company ON transactions(company_id);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_txn_fy ON transactions(financial_year, txn_date);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_txn_type ON transactions(txn_type);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_txn_mode ON transactions(payment_mode);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_txn_bill ON transactions(bill_no);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_parties_norm ON parties(normalized_name);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_parties_company ON parties(company_id);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_inv_items_company ON inventory_items(company_id);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_inv_qty_lookup ON inventory_quantities(company_id, financial_year, month, item_id);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_inv_purch_lookup ON inventory_purchases(company_id, financial_year, month, item_id);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_audit_company ON audit_logs(company_id, timestamp DESC);");
        conn.Execute("CREATE INDEX IF NOT EXISTS idx_daily_cash ON daily_cash(cash_date, company_id);");
    }

    private static void SeedDefaults(SqliteConnection conn)
    {
        // Create default company if none exists
        var companyCount = conn.ExecuteScalar<int>("SELECT COUNT(*) FROM companies");
        if (companyCount == 0)
        {
            conn.Execute(@"
                INSERT INTO companies (id, name, key) 
                VALUES ('default', 'Default Company', 'default')
            ");
        }

        // Create admin user if not exists
        var adminCount = conn.ExecuteScalar<int>("SELECT COUNT(*) FROM users WHERE username = 'admin'");
        if (adminCount == 0)
        {
            // Admin with no password (requires first-run setup)
            conn.Execute(@"
                INSERT INTO users (username, password_hash, role) 
                VALUES ('admin', NULL, 'admin')
            ");
        }

        // Seed financial year setting
        var fySetting = conn.ExecuteScalar<string>(
            "SELECT setting_value FROM app_settings WHERE setting_key = 'active_financial_year' AND company_id = 'default'");
        if (string.IsNullOrEmpty(fySetting))
        {
            var currentFy = GetFinancialYear(DateTime.Today);
            conn.Execute(@"
                INSERT OR IGNORE INTO app_settings (setting_key, setting_value, company_id) 
                VALUES ('active_financial_year', @fy, 'default')
            ", new { fy = currentFy });
        }

        // Seed sync status entries
        var tables = new[] { "users", "parties", "transactions", "daily_cash", "inventory_items", "audit_logs" };
        foreach (var table in tables)
        {
            conn.Execute(@"
                INSERT OR IGNORE INTO sync_status (table_name, last_synced_at, pending_changes)
                VALUES (@table, NULL, 0)
            ", new { table });
        }
    }

    #region Helper Methods

    /// <summary>
    /// Get current financial year string (e.g., "2025-2026")
    /// </summary>
    public static string GetFinancialYear(DateTime date)
    {
        int startYear = date.Month >= 4 ? date.Year : date.Year - 1;
        return $"{startYear}-{startYear + 1}";
    }

    /// <summary>
    /// Normalize party name for consistent lookup
    /// </summary>
    public static string NormalizePartyName(string name)
    {
        var cleaned = System.Text.RegularExpressions.Regex.Replace(
            (name ?? "").Trim(), @"\s+", " ");
        return cleaned.ToLower().Replace(" ", "_");
    }

    /// <summary>
    /// Hash password using SHA-256 (compatible with legacy system)
    /// </summary>
    public static string HashPassword(string password)
    {
        using var sha256 = SHA256.Create();
        var bytes = sha256.ComputeHash(Encoding.UTF8.GetBytes(password));
        return Convert.ToHexString(bytes).ToLower();
    }

    /// <summary>
    /// Verify password against stored hash
    /// </summary>
    public static bool VerifyPassword(string? storedHash, string plainPassword)
    {
        if (string.IsNullOrEmpty(storedHash))
            return false;

        var inputHash = HashPassword(plainPassword);
        return string.Equals(storedHash, inputHash, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Get or create party and return party_id
    /// </summary>
    public static int GetOrCreateParty(SqliteConnection conn, string name, string type, bool creditAllowed, string companyId = "default")
    {
        var normalized = NormalizePartyName(name);
        if (string.IsNullOrEmpty(normalized))
            throw new ArgumentException("Party name is required");

        var existing = conn.ExecuteScalar<int?>(
            "SELECT party_id FROM parties WHERE normalized_name = @normalized AND company_id = @companyId",
            new { normalized, companyId });

        if (existing.HasValue)
            return existing.Value;

        conn.Execute(@"
            INSERT INTO parties (name, normalized_name, type, credit_allowed, company_id) 
            VALUES (@name, @normalized, @type, @credit, @companyId)
        ", new { name, normalized, type, credit = creditAllowed ? 1 : 0, companyId });

        return conn.ExecuteScalar<int>("SELECT last_insert_rowid()");
    }

    /// <summary>
    /// Get app setting value
    /// </summary>
    public static string? GetSetting(SqliteConnection conn, string key, string companyId = "default")
    {
        return conn.ExecuteScalar<string?>(
            "SELECT setting_value FROM app_settings WHERE setting_key = @key AND company_id = @companyId",
            new { key, companyId });
    }

    /// <summary>
    /// Set app setting value (upsert)
    /// </summary>
    public static void SetSetting(SqliteConnection conn, string key, string value, string companyId = "default")
    {
        conn.Execute(@"
            INSERT INTO app_settings (setting_key, setting_value, company_id)
            VALUES (@key, @value, @companyId)
            ON CONFLICT(setting_key, company_id) DO UPDATE SET setting_value = @value
        ", new { key, value, companyId });
    }

    /// <summary>
    /// Log an audit action
    /// </summary>
    public static void LogAudit(SqliteConnection conn, string username, string action, string details, string companyId = "default")
    {
        conn.Execute(@"
            INSERT INTO audit_logs (username, action, details, company_id)
            VALUES (@username, @action, @details, @companyId)
        ", new { username, action, details, companyId });
    }

    /// <summary>
    /// Get the selected financial year
    /// </summary>
    public static string GetSelectedFinancialYear(SqliteConnection conn, string companyId = "default")
    {
        var fy = GetSetting(conn, "active_financial_year", companyId);
        return string.IsNullOrEmpty(fy) ? GetFinancialYear(DateTime.Today) : fy;
    }

    /// <summary>
    /// Parse financial year into start and end dates
    /// </summary>
    public static (DateTime start, DateTime end) ParseFinancialYear(string financialYear)
    {
        var parts = financialYear.Split('-');
        if (parts.Length != 2 || !int.TryParse(parts[0], out int startYear) || !int.TryParse(parts[1], out int endYear))
            throw new ArgumentException("Invalid financial year format. Use YYYY-YYYY");

        if (endYear != startYear + 1)
            throw new ArgumentException("Financial year must be consecutive years");

        return (new DateTime(startYear, 4, 1), new DateTime(endYear, 3, 31));
    }

    /// <summary>
    /// Backup the SQLite database to a specified path
    /// </summary>
    public static void Backup(string destinationPath)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(destinationPath)!);

        using var source = GetConnection();
        source.Open();

        using var destination = new SqliteConnection($"Data Source={destinationPath}");
        destination.Open();

        source.BackupDatabase(destination);
    }

    /// <summary>
    /// Restore the SQLite database from a backup file
    /// </summary>
    public static void Restore(string backupPath)
    {
        if (!File.Exists(backupPath))
            throw new FileNotFoundException("Backup file not found", backupPath);

        // Close existing connections by using a new connection string temporarily
        SqliteConnection.ClearAllPools();

        // Copy backup over the existing database
        File.Copy(backupPath, _dbPath, overwrite: true);
    }

    /// <summary>
    /// Auto-backup to configured backup directory
    /// </summary>
    public static string AutoBackup()
    {
        var backupDir = Config.AppConfig.Load().BackupDir;
        Directory.CreateDirectory(backupDir);

        var filename = $"finlogs_auto_{DateTime.Now:yyyyMMdd_HHmmss}.db";
        var backupPath = Path.Combine(backupDir, filename);

        Backup(backupPath);

        // Prune old auto-backups (keep last 10)
        PruneBackups(backupDir, "finlogs_auto_*.db", keepCount: 10);

        return backupPath;
    }

    private static void PruneBackups(string dir, string pattern, int keepCount)
    {
        try
        {
            var files = Directory.GetFiles(dir, pattern)
                .OrderByDescending(File.GetCreationTime)
                .Skip(keepCount)
                .ToArray();

            foreach (var file in files)
            {
                try { File.Delete(file); } catch { }
            }
        }
        catch { }
    }

    #endregion
}
