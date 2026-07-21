using Microsoft.Win32;
using System.Diagnostics;

namespace MFinlogs.Desktop;

/// <summary>
/// Handles the mfinlogs:// protocol for deep linking into the app.
/// Usage: mfinlogs://dashboard, mfinlogs://daily-entry, mfinlogs://reports/ledger
/// </summary>
public static class ProtocolHandler
{
    private const string PROTOCOL = "mfinlogs";
    private const string PROTOCOL_DESCRIPTION = "M-Finlogs Desktop App";

    /// <summary>
    /// Register mfinlogs:// protocol handler in the Windows registry (current user).
    /// Called once during first run or from settings.
    /// </summary>
    public static void Register()
    {
        try
        {
            var exePath = Process.GetCurrentProcess().MainModule?.FileName;
            if (string.IsNullOrEmpty(exePath)) return;

            using var key = Registry.CurrentUser.CreateSubKey($@"SOFTWARE\Classes\{PROTOCOL}");
            if (key == null) return;

            key.SetValue("", $"URL:{PROTOCOL_DESCRIPTION}");
            key.SetValue("URL Protocol", "");

            using var iconKey = key.CreateSubKey("DefaultIcon");
            iconKey?.SetValue("", $"\"{exePath}\",0");

            using var commandKey = key.CreateSubKey(@"shell\open\command");
            commandKey?.SetValue("", $"\"{exePath}\" --url \"%1\"");

            Debug.WriteLine($"Registered protocol handler: {PROTOCOL}://");
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"Protocol registration failed: {ex.Message}");
        }
    }

    /// <summary>
    /// Unregister the protocol handler
    /// </summary>
    public static void Unregister()
    {
        try
        {
            Registry.CurrentUser.DeleteSubKeyTree($@"SOFTWARE\Classes\{PROTOCOL}", false);
        }
        catch { }
    }

    /// <summary>
    /// Check if the protocol handler is registered
    /// </summary>
    public static bool IsRegistered()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey($@"SOFTWARE\Classes\{PROTOCOL}");
            return key != null;
        }
        catch { return false; }
    }

    /// <summary>
    /// Parse a mfinlogs:// URL into a web app path.
    /// Example: "mfinlogs://reports/ledger?party=ABC" → "/reports/ledger?party=ABC"
    /// </summary>
    public static string? ParseUrl(string protocolUrl)
    {
        if (string.IsNullOrWhiteSpace(protocolUrl))
            return null;

        // Strip the protocol prefix
        var trimmed = protocolUrl.Trim().TrimEnd('/');

        if (trimmed.StartsWith($"{PROTOCOL}://", StringComparison.OrdinalIgnoreCase))
        {
            var path = trimmed.Substring($"{PROTOCOL}://".Length);
            // Ensure it starts with /
            if (!path.StartsWith('/'))
                path = "/" + path;
            return path;
        }

        return null;
    }
}
