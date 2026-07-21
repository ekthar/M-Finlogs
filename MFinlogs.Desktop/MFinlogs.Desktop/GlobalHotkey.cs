using System.Runtime.InteropServices;

namespace MFinlogs.Desktop;

/// <summary>
/// Registers a system-wide global hotkey (Ctrl+Shift+F) that brings
/// M-Finlogs to the foreground from anywhere in Windows.
/// </summary>
public class GlobalHotkey : IDisposable
{
    private readonly Form ownerForm;
    private readonly int hotkeyId;
    private bool registered = false;
    private bool disposed = false;

    // Hotkey: Ctrl+Shift+F
    private const int MOD_CONTROL = 0x0002;
    private const int MOD_SHIFT = 0x0004;
    private const int VK_F = 0x46;
    public const int WM_HOTKEY = 0x0312;

    [DllImport("user32.dll")]
    private static extern bool RegisterHotKey(IntPtr hWnd, int id, int fsModifiers, int vk);

    [DllImport("user32.dll")]
    private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    public GlobalHotkey(Form owner)
    {
        ownerForm = owner;
        hotkeyId = owner.GetHashCode(); // Unique ID per form
        Register();
    }

    private void Register()
    {
        try
        {
            registered = RegisterHotKey(ownerForm.Handle, hotkeyId, MOD_CONTROL | MOD_SHIFT, VK_F);
            if (!registered)
                System.Diagnostics.Debug.WriteLine("Global hotkey Ctrl+Shift+F registration failed (may be in use by another app).");
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Hotkey registration error: {ex.Message}");
        }
    }

    /// <summary>
    /// Call this from the form's WndProc when WM_HOTKEY is received
    /// </summary>
    public bool IsOurHotkey(Message m)
    {
        return m.Msg == WM_HOTKEY && (int)m.WParam == hotkeyId;
    }

    public void Dispose()
    {
        if (!disposed)
        {
            if (registered)
            {
                UnregisterHotKey(ownerForm.Handle, hotkeyId);
                registered = false;
            }
            disposed = true;
        }
        GC.SuppressFinalize(this);
    }
}
