namespace MFinlogs.Desktop;

/// <summary>
/// Manages the system tray icon and context menu
/// </summary>
public class TrayManager : IDisposable
{
    private readonly MainForm mainForm;
    private NotifyIcon? trayIcon;
    private ContextMenuStrip? contextMenu;
    private bool disposed = false;

    public TrayManager(MainForm form)
    {
        mainForm = form;
        Initialize();
    }

    private void Initialize()
    {
        // Create context menu
        contextMenu = new ContextMenuStrip();

        var openItem = new ToolStripMenuItem("&Open M-Finlogs");
        openItem.Font = new Font(openItem.Font, FontStyle.Bold);
        openItem.Click += (s, e) => mainForm.ShowFromTray();

        var separator1 = new ToolStripSeparator();

        // Mode submenu
        var modeMenu = new ToolStripMenuItem("Mode");
        var onlineItem = new ToolStripMenuItem("Online (Cloud DB)")
        {
            Checked = Program.Config.IsOnlineMode,
            CheckOnClick = false
        };
        var offlineItem = new ToolStripMenuItem("Offline (Local DB)")
        {
            Checked = Program.Config.IsOfflineMode,
            CheckOnClick = false
        };
        var hybridItem = new ToolStripMenuItem("Hybrid (Online UI + Local DB)")
        {
            Checked = Program.Config.IsHybridMode,
            CheckOnClick = false
        };

        void UncheckAllModes()
        {
            onlineItem.Checked = false;
            offlineItem.Checked = false;
            hybridItem.Checked = false;
        }

        onlineItem.Click += (s, e) =>
        {
            UncheckAllModes();
            onlineItem.Checked = true;
            mainForm.SwitchMode("online");
        };
        offlineItem.Click += (s, e) =>
        {
            UncheckAllModes();
            offlineItem.Checked = true;
            mainForm.SwitchMode("offline");
        };
        hybridItem.Click += (s, e) =>
        {
            UncheckAllModes();
            hybridItem.Checked = true;
            mainForm.SwitchMode("hybrid");
        };

        modeMenu.DropDownItems.Add(onlineItem);
        modeMenu.DropDownItems.Add(offlineItem);
        modeMenu.DropDownItems.Add(hybridItem);

        var separator2 = new ToolStripSeparator();

        var updateItem = new ToolStripMenuItem("Check for &Updates");
        updateItem.Click += async (s, e) =>
        {
            var updater = new AutoUpdater();
            await updater.CheckForUpdatesAsync(silent: false);
        };

        var pdfItem = new ToolStripMenuItem("Save Page as &PDF");
        pdfItem.Click += async (s, e) =>
        {
            mainForm.ShowFromTray();
            await mainForm.SaveAsPdfAsync();
        };

        var separator3 = new ToolStripSeparator();

        var exitItem = new ToolStripMenuItem("E&xit");
        exitItem.Click += (s, e) => mainForm.ExitApplication();

        contextMenu.Items.AddRange(new ToolStripItem[]
        {
            openItem,
            separator1,
            modeMenu,
            pdfItem,
            separator2,
            updateItem,
            separator3,
            exitItem
        });

        // Create tray icon
        trayIcon = new NotifyIcon
        {
            Text = "M-Finlogs",
            ContextMenuStrip = contextMenu,
            Visible = true
        };

        // Set icon
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
        {
            try
            {
                trayIcon.Icon = new Icon(iconPath);
            }
            catch
            {
                trayIcon.Icon = SystemIcons.Application;
            }
        }
        else
        {
            trayIcon.Icon = SystemIcons.Application;
        }

        // Double-click to restore
        trayIcon.DoubleClick += (s, e) => mainForm.ShowFromTray();
    }

    /// <summary>
    /// Show a balloon notification from the tray icon
    /// </summary>
    public void ShowBalloon(string title, string text, ToolTipIcon icon = ToolTipIcon.Info)
    {
        trayIcon?.ShowBalloonTip(3000, title, text, icon);
    }

    /// <summary>
    /// Show notification for overdue parties
    /// </summary>
    public void NotifyOverdueParties(int count)
    {
        if (count > 0)
        {
            ShowBalloon(
                "Overdue Payments",
                $"{count} parties have overdue payments. Click to view.",
                ToolTipIcon.Warning);
        }
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    protected virtual void Dispose(bool disposing)
    {
        if (!disposed)
        {
            if (disposing)
            {
                if (trayIcon != null)
                {
                    trayIcon.Visible = false;
                    trayIcon.Dispose();
                    trayIcon = null;
                }
                contextMenu?.Dispose();
            }
            disposed = true;
        }
    }
}
