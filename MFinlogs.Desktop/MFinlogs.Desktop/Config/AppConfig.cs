using System.Text.Json;
using System.Text.Json.Serialization;

namespace MFinlogs.Desktop.Config;

/// <summary>
/// Application configuration model - serialized to/from appsettings.json
/// </summary>
public class AppConfig
{
    [JsonPropertyName("mode")]
    public string Mode { get; set; } = "online"; // "online" or "offline"

    [JsonPropertyName("onlineUrl")]
    public string OnlineUrl { get; set; } = "https://finlogs.netlify.app";

    [JsonPropertyName("localPort")]
    public int LocalPort { get; set; } = 8080;

    [JsonPropertyName("supabaseUrl")]
    public string SupabaseUrl { get; set; } = "";

    [JsonPropertyName("supabaseKey")]
    public string SupabaseKey { get; set; } = "";

    [JsonPropertyName("autoSync")]
    public bool AutoSync { get; set; } = true;

    [JsonPropertyName("syncInterval")]
    public int SyncInterval { get; set; } = 300; // seconds

    [JsonPropertyName("autoStart")]
    public bool AutoStart { get; set; } = false;

    [JsonPropertyName("minimizeToTray")]
    public bool MinimizeToTray { get; set; } = true;

    [JsonPropertyName("theme")]
    public string Theme { get; set; } = "light";

    [JsonPropertyName("lastSyncedAt")]
    public string? LastSyncedAt { get; set; }

    [JsonPropertyName("windowWidth")]
    public int WindowWidth { get; set; } = 1280;

    [JsonPropertyName("windowHeight")]
    public int WindowHeight { get; set; } = 800;

    [JsonPropertyName("windowX")]
    public int WindowX { get; set; } = -1;

    [JsonPropertyName("windowY")]
    public int WindowY { get; set; } = -1;

    [JsonPropertyName("isMaximized")]
    public bool IsMaximized { get; set; } = false;

    // Computed properties
    [JsonIgnore]
    public bool IsOnlineMode => Mode?.ToLower() == "online";

    [JsonIgnore]
    public bool IsOfflineMode => Mode?.ToLower() == "offline";

    [JsonIgnore]
    public string LocalServerUrl => $"http://localhost:{LocalPort}";

    [JsonIgnore]
    public string ActiveUrl => IsOnlineMode ? OnlineUrl : LocalServerUrl;

    [JsonIgnore]
    public string DatabasePath => Path.Combine(AppDataDir, "finlogs.db");

    [JsonIgnore]
    public string BackupDir => Path.Combine(AppDataDir, "backups");

    [JsonIgnore]
    public static string AppDataDir
    {
        get
        {
            var dir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "MFinlogs");
            Directory.CreateDirectory(dir);
            return dir;
        }
    }

    private static readonly string ConfigFilePath = Path.Combine(AppDataDir, "appsettings.json");

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    /// <summary>
    /// Load configuration from disk, or create default if not exists
    /// </summary>
    public static AppConfig Load()
    {
        try
        {
            // Try user-specific config first
            if (File.Exists(ConfigFilePath))
            {
                var json = File.ReadAllText(ConfigFilePath);
                return JsonSerializer.Deserialize<AppConfig>(json, JsonOptions) ?? new AppConfig();
            }

            // Fall back to app-local config
            var localPath = Path.Combine(AppContext.BaseDirectory, "appsettings.json");
            if (File.Exists(localPath))
            {
                var json = File.ReadAllText(localPath);
                var config = JsonSerializer.Deserialize<AppConfig>(json, JsonOptions) ?? new AppConfig();
                // Save to user directory for future edits
                config.Save();
                return config;
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Config load error: {ex.Message}");
        }

        var defaultConfig = new AppConfig();
        defaultConfig.Save();
        return defaultConfig;
    }

    /// <summary>
    /// Save configuration to disk
    /// </summary>
    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(ConfigFilePath)!);
            var json = JsonSerializer.Serialize(this, JsonOptions);
            File.WriteAllText(ConfigFilePath, json);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Config save error: {ex.Message}");
        }
    }
}
