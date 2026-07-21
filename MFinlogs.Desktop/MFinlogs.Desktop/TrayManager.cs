using System.Runtime.InteropServices;

namespace MFinlogs.Desktop;

/// <summary>
/// Professional system tray manager with:
/// - Context menu with quick actions and jump-list style entries
/// - Windows native toast-style balloon notifications
/// - Badge-style tooltip showing connection status
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
        contextMenu = new ContextMenuStrip
        {
            BackColor = Color.FromArgb(250, 250, 250),
            ShowImageMargin = true,
            Renderer = new ModernToolStripRenderer()
        };

        // === QUICK ACTIONS (jump-list style) ===
        var headerLabel = new ToolStripLabel("M-Finlogs")
        {
            Font = new Font("Segoe UI", 9f, FontStyle.Bold),
            ForeColor = Color.FromArgb(23, 23, 23),
            Padding = new Padding(4, 6, 0, 4)
        };
        contextMenu.Items.Add(headerLabel);
        contextMenu.Items.Add(new ToolStripSeparator());

        // Open app
        var openItem = new ToolStripMenuItem("&Open M-Finlogs", null, (s, e) => mainForm.ShowFromTray())
        {
            Font = new Font("Segoe UI", 9f, FontStyle.Bold)
        };
        contextMenu.Items.Add(openItem);

        // Quick navigation items
        var dashboardItem = new ToolStripMenuItem("Dashboard", null,
            (s, e) => { mainForm.ShowFromTray(); mainForm.NavigateTo(Program.Config.OnlineUrl + "/"); });
        var dailyEntryItem = new ToolStripMenuItem("New Entry", null,
            (s, e) => { mainForm.ShowFromTray(); mainForm.NavigateTo(Program.Config.OnlineUrl + "/daily-entry"); });
        var reportsItem = new ToolStripMenuItem("Reports", null,
            (s, e) => { mainForm.ShowFromTray(); mainForm.NavigateTo(Program.Config.OnlineUrl + "/reports/ledger"); });
        var partiesItem = new ToolStripMenuItem("Parties", null,
            (s, e) => { mainForm.ShowFromTray(); mainForm.NavigateTo(Program.Config.OnlineUrl + "/parties"); });

        contextMenu.Items.Add(new ToolStripSeparator());
        contextMenu.Items.Add(dashboardItem);
        contextMenu.Items.Add(dailyEntryItem);
        contextMenu.Items.Add(reportsItem);
        contextMenu.Items.Add(partiesItem);

        // === MODE ===
        contextMenu.Items.Add(new ToolStripSeparator());
        var modeMenu = new ToolStripMenuItem("Mode");

        var onlineItem = new ToolStripMenuItem("Online (Cloud)")
        { Checked = Program.Config.IsOnlineMode, CheckOnClick = false };
        var offlineItem = new ToolStripMenuItem("Offline (Local)")
        { Checked = Program.Config.IsOfflineMode, CheckOnClick = false };
        var hybridItem = new ToolStripMenuItem("Hybrid (Cloud UI + Local DB)")
        { Checked = Program.Config.IsHybridMode, CheckOnClick = false };

        void UncheckAll() { onlineItem.Checked = offlineItem.Checked = hybridItem.Checked = false; }

        onlineItem.Click += (s, e) => { UncheckAll(); onlineItem.Checked = true; mainForm.SwitchMode("online"); };
        offlineItem.Click += (s, e) => { UncheckAll(); offlineItem.Checked = true; mainForm.SwitchMode("offline"); };
        hybridItem.Click += (s, e) => { UncheckAll(); hybridItem.Checked = true; mainForm.SwitchMode("hybrid"); };

        modeMenu.DropDownItems.AddRange(new ToolStripItem[] { onlineItem, offlineItem, hybridItem });
        contextMenu.Items.Add(modeMenu);

        // === TOOLS ===
        contextMenu.Items.Add(new ToolStripSeparator());

        var pdfItem = new ToolStripMenuItem("Save Page as PDF", null, async (s, e) =>
        {
            mainForm.ShowFromTray();
            await mainForm.SaveAsPdfAsync();
        });
        contextMenu.Items.Add(pdfItem);

        var updateItem = new ToolStripMenuItem("Check for Updates", null, async (s, e) =>
        {
            var updater = new AutoUpdater();
            await updater.CheckForUpdatesAsync(silent: false);
        });
        contextMenu.Items.Add(updateItem);

        // === EXIT ===
        contextMenu.Items.Add(new ToolStripSeparator());
        var exitItem = new ToolStripMenuItem("Exit", null, (s, e) => mainForm.ExitApplication());
        contextMenu.Items.Add(exitItem);

        // === TRAY ICON ===
        trayIcon = new NotifyIcon
        {
            Text = GetTooltipText(),
            ContextMenuStrip = contextMenu,
            Visible = true
        };

        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        try { trayIcon.Icon = File.Exists(iconPath) ? new Icon(iconPath) : SystemIcons.Application; }
        catch { trayIcon.Icon = SystemIcons.Application; }

        trayIcon.DoubleClick += (s, e) => mainForm.ShowFromTray();
        trayIcon.MouseClick += (s, e) =>
        {
            if (e.Button == MouseButtons.Left)
                mainForm.ShowFromTray();
        };
    }

    private string GetTooltipText()
    {
        var mode = Program.Config.Mode?.ToLower() switch
        {
            "offline" => "Offline",
            "hybrid" => "Hybrid",
            _ => "Online"
        };
        return $"M-Finlogs — {mode}";
    }

    /// <summary>
    /// Show a Windows toast-style balloon notification
    /// </summary>
    public void ShowBalloon(string title, string text, ToolTipIcon icon = ToolTipIcon.None)
    {
        trayIcon?.ShowBalloonTip(4000, title, text, icon);
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
                $"{count} {(count == 1 ? "party has" : "parties have")} outstanding balance.",
                ToolTipIcon.Warning);
        }
    }

    /// <summary>
    /// Show sync status notification
    /// </summary>
    public void NotifySyncComplete(int changes)
    {
        if (changes > 0)
            ShowBalloon("Sync Complete", $"{changes} records synchronized.");
    }

    /// <summary>
    /// Update tooltip text (e.g., when mode changes)
    /// </summary>
    public void RefreshTooltip()
    {
        if (trayIcon != null)
            trayIcon.Text = GetTooltipText();
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

/// <summary>
/// Modern flat renderer for the context menu (Windows 11 style)
/// </summary>
internal class ModernToolStripRenderer : ToolStripProfessionalRenderer
{
    public ModernToolStripRenderer() : base(new ModernColorTable()) { }

    protected override void OnRenderMenuItemBackground(ToolStripItemRenderEventArgs e)
    {
        if (e.Item.Selected && e.Item.Enabled)
        {
            using var brush = new SolidBrush(Color.FromArgb(235, 235, 240));
            var rect = new Rectangle(4, 1, e.Item.Width - 8, e.Item.Height - 2);
            using var path = CreateRoundedRect(rect, 4);
            e.Graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.HighQuality;
            e.Graphics.FillPath(brush, path);
        }
    }

    protected override void OnRenderSeparator(ToolStripSeparatorRenderEventArgs e)
    {
        using var pen = new Pen(Color.FromArgb(230, 230, 230), 1f);
        int y = e.Item.Height / 2;
        e.Graphics.DrawLine(pen, 8, y, e.Item.Width - 8, y);
    }

    private static System.Drawing.Drawing2D.GraphicsPath CreateRoundedRect(Rectangle rect, int radius)
    {
        var path = new System.Drawing.Drawing2D.GraphicsPath();
        int d = radius * 2;
        path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        path.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        path.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        path.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        path.CloseFigure();
        return path;
    }
}

internal class ModernColorTable : ProfessionalColorTable
{
    public override Color MenuBorder => Color.FromArgb(220, 220, 220);
    public override Color MenuItemBorder => Color.Transparent;
    public override Color MenuItemSelected => Color.FromArgb(235, 235, 240);
    public override Color MenuStripGradientBegin => Color.FromArgb(250, 250, 250);
    public override Color MenuStripGradientEnd => Color.FromArgb(250, 250, 250);
    public override Color ToolStripDropDownBackground => Color.FromArgb(250, 250, 250);
    public override Color ImageMarginGradientBegin => Color.FromArgb(250, 250, 250);
    public override Color ImageMarginGradientMiddle => Color.FromArgb(250, 250, 250);
    public override Color ImageMarginGradientEnd => Color.FromArgb(250, 250, 250);
    public override Color SeparatorDark => Color.FromArgb(230, 230, 230);
    public override Color SeparatorLight => Color.Transparent;
}
