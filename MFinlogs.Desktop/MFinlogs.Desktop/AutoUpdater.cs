using System.Diagnostics;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text.Json.Serialization;

namespace MFinlogs.Desktop;

/// <summary>
/// Checks GitHub releases for new versions and prompts the user to update
/// </summary>
public class AutoUpdater
{
    private const string GitHubApiUrl = "https://api.github.com/repos/ekthar/M-Finlogs/releases/latest";
    private static readonly HttpClient httpClient = new();

    static AutoUpdater()
    {
        httpClient.DefaultRequestHeaders.UserAgent.ParseAdd("MFinlogs-Desktop/1.0");
        httpClient.Timeout = TimeSpan.FromSeconds(10);
    }

    /// <summary>
    /// Check GitHub releases for updates
    /// </summary>
    /// <param name="silent">If true, only show dialog when update is available</param>
    public async Task CheckForUpdatesAsync(bool silent = true)
    {
        try
        {
            var response = await httpClient.GetAsync(GitHubApiUrl);

            if (!response.IsSuccessStatusCode)
            {
                if (!silent)
                {
                    MessageBox.Show(
                        "Unable to check for updates. Please check your internet connection.",
                        "Update Check",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Information);
                }
                return;
            }

            var release = await response.Content.ReadFromJsonAsync<GitHubRelease>();
            if (release == null)
            {
                if (!silent)
                    ShowUpToDate();
                return;
            }

            var currentVersion = GetCurrentVersion();
            var latestVersion = ParseVersion(release.TagName);

            if (latestVersion > currentVersion)
            {
                var result = MessageBox.Show(
                    $"A new version of M-Finlogs is available!\n\n" +
                    $"Current: v{currentVersion}\n" +
                    $"Latest: v{latestVersion}\n\n" +
                    $"{release.Name}\n\n" +
                    $"Would you like to download the update?",
                    "Update Available",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Information);

                if (result == DialogResult.Yes)
                {
                    // Open download page in default browser
                    var downloadUrl = release.HtmlUrl ?? $"https://github.com/ekthar/M-Finlogs/releases/latest";
                    Process.Start(new ProcessStartInfo(downloadUrl) { UseShellExecute = true });
                }
            }
            else if (!silent)
            {
                ShowUpToDate();
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"Update check failed: {ex.Message}");
            if (!silent)
            {
                MessageBox.Show(
                    $"Failed to check for updates:\n{ex.Message}",
                    "Update Check",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
            }
        }
    }

    private static void ShowUpToDate()
    {
        MessageBox.Show(
            "You are running the latest version of M-Finlogs.",
            "Update Check",
            MessageBoxButtons.OK,
            MessageBoxIcon.Information);
    }

    private static Version GetCurrentVersion()
    {
        var assembly = typeof(AutoUpdater).Assembly;
        return assembly.GetName().Version ?? new Version(1, 0, 0);
    }

    private static Version ParseVersion(string? tagName)
    {
        if (string.IsNullOrWhiteSpace(tagName))
            return new Version(0, 0, 0);

        // Strip leading 'v' if present
        var versionStr = tagName.TrimStart('v', 'V');

        return Version.TryParse(versionStr, out var version)
            ? version
            : new Version(0, 0, 0);
    }

    /// <summary>
    /// Set or remove the application from Windows startup
    /// </summary>
    public static void SetAutoStart(bool enable)
    {
        try
        {
            var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(
                @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run", true);

            if (key == null) return;

            if (enable)
            {
                var exePath = Process.GetCurrentProcess().MainModule?.FileName;
                if (!string.IsNullOrEmpty(exePath))
                {
                    key.SetValue("MFinlogs", $"\"{exePath}\" --minimized");
                }
            }
            else
            {
                key.DeleteValue("MFinlogs", false);
            }

            key.Close();
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"AutoStart registry error: {ex.Message}");
        }
    }
}

/// <summary>
/// GitHub Release API response model
/// </summary>
internal class GitHubRelease
{
    [JsonPropertyName("tag_name")]
    public string? TagName { get; set; }

    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("html_url")]
    public string? HtmlUrl { get; set; }

    [JsonPropertyName("body")]
    public string? Body { get; set; }

    [JsonPropertyName("published_at")]
    public string? PublishedAt { get; set; }

    [JsonPropertyName("assets")]
    public List<GitHubAsset>? Assets { get; set; }
}

internal class GitHubAsset
{
    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("browser_download_url")]
    public string? BrowserDownloadUrl { get; set; }

    [JsonPropertyName("size")]
    public long Size { get; set; }
}
