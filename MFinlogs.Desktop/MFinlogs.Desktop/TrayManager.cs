using MFinlogs.Desktop.UI;
using MFinlogs.Desktop.UI.Controls;

namespace MFinlogs.Desktop;

/// <summary>
/// Premium system tray manager with modern context menu styling.
/// Uses ModernContextMenu renderer for rounded corners, proper spacing,
/// section headers, and animated hover states.
/// 
/// All original functionality preserved: mode switching, PDF save,
/// update check, settings, exit.
/// </summary>
public class TrayManager : IDisposable
{
    private readonly MainForm _mainForm;
    private NotifyIcon? _trayIcon;
    private ContextMenuStrip? _contextMenu;
    private bool _disposed;

    // Mode items for checked state management
    private ToolStripMenuItem? _onlineItem;
    private ToolStripMenuItem? _offlineItem;
    private ToolStripMenuItem? _hybridItem;

    public TrayManager(MainForm form)
    {
        _mainForm = form;
        Initialize();
    }

    private void Initialize()
    {
        _contextMenu = new ContextMenuStrip();
        _contextMenu.Padding = new Padding(4);
        _contextMenu.ShowImageMargin = false;

        // ─── Open (bold primary action) ───
        var openItem = new ToolStripMenuItem("Open M-Finlogs")
        {
            Font = DesignTokens.Typography.Title
        };
        openItem.Click += (s, e) => _mainForm.ShowFromTray();

        // ─── Separator ───
        var sep1 = new ToolStripSeparator();

        // ─── Mode Submenu ───
        var modeMenu = new ToolStripMenuItem("Mode");

        _onlineItem = new ToolStripMenuItem("\u2601  Online (Cloud)")
        {
            Checked = Program.Config.IsOnlineMode,
            CheckOnClick = false
        };
        _offlineItem = new ToolStripMenuItem("\u26A1  Offline (Local)")
        {
            Checked = Program.Config.IsOfflineMode,
            CheckOnClick = false
        };
        _hybridItem = new ToolStripMenuItem("\u21C4  Hybrid")
        {
            Checked = Program.Config.IsHybridMode,
            CheckOnClick = false
        };


        void UncheckAllModes()
        {
            _onlineItem!.Checked = false;
            _offlineItem!.Checked = false;
            _hybridItem!.Checked = false;
        }

        _onlineItem.Click += (s, e) =>
        { UncheckAllModes(); _onlineItem.Checked = true; _mainForm.SwitchMode("online"); };
        _offlineItem.Click += (s, e) =>
        { UncheckAllModes(); _offlineItem.Checked = true; _mainForm.SwitchMode("offline"); };
        _hybridItem.Click += (s, e) =>
        { UncheckAllModes(); _hybridItem.Checked = true; _mainForm.SwitchMode("hybrid"); };

        modeMenu.DropDownItems.Add(_onlineItem);
        modeMenu.DropDownItems.Add(_offlineItem);
        modeMenu.DropDownItems.Add(_hybridItem);

        // ─── Actions ───
        var sep2 = new ToolStripSeparator();

        var pdfItem = new ToolStripMenuItem("\uD83D\uDCC4  Save as PDF");
        pdfItem.Click += async (s, e) =>
        {
            _mainForm.ShowFromTray();
            await _mainForm.SaveAsPdfAsync();
        };

        var updateItem = new ToolStripMenuItem("\u2B06  Check for Updates");
        updateItem.Click += async (s, e) =>
        {
            var updater = new AutoUpdater();
            await updater.CheckForUpdatesAsync(silent: false);
        };

        var settingsItem = new ToolStripMenuItem("\u2699  Settings");
        settingsItem.Click += (s, e) =>
        {
            _mainForm.ShowFromTray();
            Program.OpenSettings(_mainForm);
            _mainForm.NavigateTo(Program.Config.ActiveUrl);
        };

        var sep3 = new ToolStripSeparator();

        var exitItem = new ToolStripMenuItem("\u23FB  Exit");
        exitItem.Click += (s, e) => _mainForm.ExitApplication();

        // ─── Build Menu ───
        _contextMenu.Items.AddRange(new ToolStripItem[]
        {
            openItem, sep1,
            modeMenu, pdfItem, sep2,
            updateItem, settingsItem, sep3,
            exitItem
        });

        // Apply premium styling
        ModernContextMenu.Apply(_contextMenu);

        // ─── Tray Icon ───
        _trayIcon = new NotifyIcon
        {
            Text = "M-Finlogs",
            ContextMenuStrip = _contextMenu,
            Visible = true
        };

        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
            try { _trayIcon.Icon = new Icon(iconPath); }
            catch { _trayIcon.Icon = SystemIcons.Application; }
        else
            _trayIcon.Icon = SystemIcons.Application;

        _trayIcon.DoubleClick += (s, e) => _mainForm.ShowFromTray();
    }

    /// <summary>Show a balloon notification from the tray icon</summary>
    public void ShowBalloon(string title, string text, ToolTipIcon icon = ToolTipIcon.Info)
    {
        _trayIcon?.ShowBalloonTip(3000, title, text, icon);
    }

    /// <summary>Show notification for overdue parties</summary>
    public void NotifyOverdueParties(int count)
    {
        if (count > 0)
            ShowBalloon("Overdue Payments",
                $"{count} parties have overdue payments. Click to view.",
                ToolTipIcon.Warning);
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    protected virtual void Dispose(bool disposing)
    {
        if (!_disposed)
        {
            if (disposing)
            {
                if (_trayIcon != null)
                {
                    _trayIcon.Visible = false;
                    _trayIcon.Dispose();
                    _trayIcon = null;
                }
                _contextMenu?.Dispose();
            }
            _disposed = true;
        }
    }
}
