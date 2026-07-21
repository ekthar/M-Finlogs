using Microsoft.Win32;
using System.Diagnostics;

namespace MFinlogs.Desktop;

/// <summary>
/// Handles the mfinlogs:// protocol for deep linking into the app.
/// </summary>
public static class ProtocolHandler
{
    private const string PROTOCOL = "mfinlogs";

    public static void Register()
    {
        try
        {
            var exePath = Process.GetCurrentProcess().MainModule?.FileName;
            if (string.IsNullOrEmpty(exePath)) return;

            using var key = Registry.CurrentUser.CreateSubKey($@"SOFTWARE\Classes\{PROTOCOL}");
            if (key == null) return;

            key.SetValue("", "URL:M-Finlogs");
            key.SetValue("URL Protocol", "");

            using var iconKey = key.CreateSubKey("DefaultIcon");
            iconKey?.SetValue("", $"\"{exePath}\",0");

            using var commandKey = key.CreateSubKey(@"shell\open\command");
            commandKey?.SetValue("", $"\"{exePath}\" --url \"%1\"");
        }
        catch { }
    }

    public static bool IsRegistered()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey($@"SOFTWARE\Classes\{PROTOCOL}");
            return key != null;
        }
        catch { return false; }
    }

    public static string? ParseUrl(string protocolUrl)
    {
        if (string.IsNullOrWhiteSpace(protocolUrl)) return null;
        var trimmed = protocolUrl.Trim().TrimEnd('/');
        if (trimmed.StartsWith($"{PROTOCOL}://", StringComparison.OrdinalIgnoreCase))
        {
            var path = trimmed.Substring($"{PROTOCOL}://".Length);
            if (!path.StartsWith('/')) path = "/" + path;
            return path;
        }
        return null;
    }
}
