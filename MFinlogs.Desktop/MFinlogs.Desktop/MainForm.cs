using Microsoft.Web.WebView2.WinForms;
using Microsoft.Web.WebView2.Core;
using MFinlogs.Desktop.Config;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Runtime.InteropServices;

namespace MFinlogs.Desktop;

public partial class MainForm : Form
{
    private WebView2 webView = null!;
    private TrayManager? trayManager;
    private AutoUpdater? autoUpdater;
    private bool isFullscreen = false;
    private FormWindowState previousWindowState;

    // Layout
    private Panel titleBar = null!;
    private Panel webViewContainer = null!;

    // Title bar controls
    private Label titleLabel = null!;
    private Label lblMode = null!;
    private Button btnClose = null!;
    private Button btnMaximize = null!;
    private Button btnMinimize = null!;

    // Drag
    private bool isDragging = false;
    private Point dragStart;

    // Fonts
    private Font appFont = null!;
    private Font appFontBold = null!;
    private Font btnFont = null!;

    public MainForm()
    {
        InitializeComponent();
        LoadFonts();
        SetupForm();
        SetupTitleBar();
        SetupWebViewContainer();
        SetupTrayManager();
        SetupAutoUpdater();
        this.Shown += MainForm_Shown;
    }

    private async void MainForm_Shown(object? sender, EventArgs e)
    {
        this.Shown -= MainForm_Shown;
        await InitializeWebViewAsync();
    }

    private void LoadFonts()
    {
        try
        {
            var pfc = new PrivateFontCollection();
            var regular = Path.Combine(AppContext.BaseDirectory, "Assets", "InterTight-Regular.ttf");
            var bold = Path.Combine(AppContext.BaseDirectory, "Assets", "InterTight-Bold.ttf");
            if (File.Exists(regular)) pfc.AddFontFile(regular);
            if (File.Exists(bold)) pfc.AddFontFile(bold);
            if (pfc.Families.Length > 0)
            {
                appFont = new Font(pfc.Families[0], 8.5f, FontStyle.Regular);
                appFontBold = new Font(pfc.Families[0], 9f, FontStyle.Bold);
                btnFont = new Font("Segoe MDL2 Assets", 10f, FontStyle.Regular);
                return;
            }
        }
        catch { }
        appFont = new Font("Segoe UI", 8.5f, FontStyle.Regular);
        appFontBold = new Font("Segoe UI", 9f, FontStyle.Bold);
        btnFont = new Font("Segoe MDL2 Assets", 10f, FontStyle.Regular);
    }

    private void SetupForm()
    {
        var config = Program.Config;
        this.Text = "M-Finlogs";
        this.MinimumSize = new Size(1024, 600);
        this.StartPosition = FormStartPosition.CenterScreen;
        this.FormBorderStyle = FormBorderStyle.None;
        this.BackColor = GetThemeBackColor();
        this.DoubleBuffered = true;

        if (config.WindowWidth > 0 && config.WindowHeight > 0)
            this.Size = new Size(config.WindowWidth, config.WindowHeight);
        else
            this.Size = new Size(1280, 800);

        if (config.WindowX >= 0 && config.WindowY >= 0)
        {
            this.Location = new Point(config.WindowX, config.WindowY);
            this.StartPosition = FormStartPosition.Manual;
        }
        if (config.IsMaximized)
            this.WindowState = FormWindowState.Maximized;

        this.Resize += MainForm_Resize;
        this.FormClosing += MainForm_FormClosing;
        this.KeyPreview = true;
        this.KeyDown += MainForm_KeyDown;
    }

    private void SetupTitleBar()
    {
        // Title bar: 30px height, clean Windows 11 style
        titleBar = new Panel
        {
            Dock = DockStyle.Top,
            Height = 30,
            BackColor = GetThemeTitleBarColor()
        };
        titleBar.Paint += TitleBar_Paint;

        // === LEFT: App icon + title ===
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        var iconBox = new PictureBox
        {
            Size = new Size(16, 16),
            Location = new Point(10, 7),
            SizeMode = PictureBoxSizeMode.StretchImage,
            BackColor = Color.Transparent
        };
        if (File.Exists(iconPath))
        {
            try { iconBox.Image = new Icon(iconPath, 16, 16).ToBitmap(); } catch { }
        }

        titleLabel = new Label
        {
            Text = "M-Finlogs",
            Font = appFontBold,
            ForeColor = GetThemeTitleForeColor(),
            AutoSize = true,
            Location = new Point(30, 7),
            BackColor = Color.Transparent
        };

        // === RIGHT: Mode label + window buttons ===
        // Window buttons (Windows 11 style): minimize, maximize, close
        int btnW = 46, btnH = 30;

        btnClose = MakeWinButton("\uE8BB", btnW, btnH); // X icon (Segoe MDL2)
        btnClose.Dock = DockStyle.Right;
        btnClose.FlatAppearance.MouseOverBackColor = Color.FromArgb(232, 17, 35);
        btnClose.FlatAppearance.MouseDownBackColor = Color.FromArgb(196, 13, 28);
        btnClose.Click += (s, e) =>
        {
            if (Program.Config.MinimizeToTray) HideToTray();
            else ExitApplication();
        };

        btnMaximize = MakeWinButton("\uE922", btnW, btnH); // Maximize icon
        btnMaximize.Dock = DockStyle.Right;
        btnMaximize.Click += (s, e) => ToggleMaximize();

        btnMinimize = MakeWinButton("\uE921", btnW, btnH); // Minimize icon
        btnMinimize.Dock = DockStyle.Right;
        btnMinimize.Click += (s, e) => this.WindowState = FormWindowState.Minimized;

        // Mode indicator (before buttons)
        lblMode = new Label
        {
            Text = GetModeLabel(),
            Font = appFont,
            ForeColor = GetModeLabelColor(),
            AutoSize = true,
            BackColor = Color.Transparent,
            Padding = new Padding(4, 0, 4, 0)
        };
        var modePanel = new Panel
        {
            Dock = DockStyle.Right,
            Width = 70,
            BackColor = Color.Transparent
        };
        modePanel.Controls.Add(lblMode);
        modePanel.Resize += (s, e) => lblMode.Location = new Point(
            (modePanel.Width - lblMode.Width) / 2, 8);

        // Add controls to title bar
        titleBar.Controls.Add(iconBox);
        titleBar.Controls.Add(titleLabel);
        titleBar.Controls.Add(btnClose);
        titleBar.Controls.Add(btnMaximize);
        titleBar.Controls.Add(btnMinimize);
        titleBar.Controls.Add(modePanel);

        // Drag support on title bar + title label
        titleBar.MouseDown += TitleBar_MouseDown;
        titleBar.MouseMove += TitleBar_MouseMove;
        titleBar.MouseUp += TitleBar_MouseUp;
        titleBar.DoubleClick += (s, e) => ToggleMaximize();
        titleLabel.MouseDown += TitleBar_MouseDown;
        titleLabel.MouseMove += TitleBar_MouseMove;
        titleLabel.MouseUp += TitleBar_MouseUp;
        titleLabel.DoubleClick += (s, e) => ToggleMaximize();

        this.Controls.Add(titleBar);
    }

    /// <summary>
    /// Create a Windows 11-style window button with Segoe MDL2 Assets icon
    /// </summary>
    private Button MakeWinButton(string icon, int w, int h)
    {
        var btn = new Button
        {
            Text = icon,
            Font = btnFont,
            Size = new Size(w, h),
            FlatStyle = FlatStyle.Flat,
            ForeColor = GetThemeTitleForeColor(),
            BackColor = Color.Transparent,
            Cursor = Cursors.Default,
            TabStop = false
        };
        btn.FlatAppearance.BorderSize = 0;
        btn.FlatAppearance.MouseOverBackColor = Color.FromArgb(30, 128, 128, 128);
        btn.FlatAppearance.MouseDownBackColor = Color.FromArgb(50, 128, 128, 128);
        return btn;
    }

    private void TitleBar_Paint(object? sender, PaintEventArgs e)
    {
        // Subtle 1px bottom border
        var sepColor = Program.Config.Theme?.ToLower() switch
        {
            "dark" or "deep blue" or "deepblue" or "warm" => Color.FromArgb(20, 255, 255, 255),
            _ => Color.FromArgb(12, 0, 0, 0)
        };
        using var pen = new Pen(sepColor, 1f);
        e.Graphics.DrawLine(pen, 0, titleBar.Height - 1, titleBar.Width, titleBar.Height - 1);
    }

    /// <summary>
    /// WebView container fills the space BELOW the title bar — prevents overlap
    /// </summary>
    private void SetupWebViewContainer()
    {
        webViewContainer = new Panel
        {
            Dock = DockStyle.Fill,
            Padding = Padding.Empty,
            Margin = Padding.Empty,
            BackColor = GetThemeBackColor()
        };
        this.Controls.Add(webViewContainer);
        // Ensure titleBar stays on top (dock order matters)
        titleBar.BringToFront();
    }


    private async Task InitializeWebViewAsync()
    {
        webView = new WebView2 { Dock = DockStyle.Fill };
        webViewContainer.Controls.Add(webView);

        if (!webView.IsHandleCreated) webView.CreateControl();

        try
        {
            var userDataFolder = Path.Combine(AppConfig.AppDataDir, "WebView2");
            Directory.CreateDirectory(userDataFolder);
            var opts = new CoreWebView2EnvironmentOptions
            {
                AdditionalBrowserArguments = "--disable-features=ServiceWorkerPaymentApps"
            };
            CoreWebView2Environment? env = null;
            for (int i = 1; i <= 3; i++)
            {
                try { env = await CoreWebView2Environment.CreateAsync(null, userDataFolder, opts); break; }
                catch when (i < 3) { await Task.Delay(500 * i); }
            }
            if (env == null) throw new InvalidOperationException("WebView2 environment creation failed.");

            await webView.EnsureCoreWebView2Async(env);

            webView.CoreWebView2.Settings.IsStatusBarEnabled = false;
            webView.CoreWebView2.Settings.AreDefaultContextMenusEnabled = true;
            webView.CoreWebView2.Settings.IsZoomControlEnabled = true;
            webView.CoreWebView2.Settings.AreDevToolsEnabled = true;

            try { await webView.CoreWebView2.Profile.ClearBrowsingDataAsync(CoreWebView2BrowsingDataKinds.ServiceWorkers); } catch { }

            webView.CoreWebView2.NavigationCompleted += CoreWebView2_NavigationCompleted;
            webView.CoreWebView2.NewWindowRequested += (s, ev) => { ev.Handled = true; webView.CoreWebView2.Navigate(ev.Uri); };

            if (Program.Config.IsHybridMode) SetupHybridApiInterception();

            webView.CoreWebView2.Navigate(Program.Config.ActiveUrl);
        }
        catch (COMException comEx) when ((uint)comEx.HResult == 0x8007139F) { ShowWebView2InstallPrompt(); }
        catch (WebView2RuntimeNotFoundException) { ShowWebView2InstallPrompt(); }
        catch (Exception ex)
        {
            MessageBox.Show($"Failed to initialize WebView2:\n{ex.Message}", "M-Finlogs - Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void ShowWebView2InstallPrompt()
    {
        if (MessageBox.Show("WebView2 Runtime is required.\nDownload now?", "M-Finlogs",
            MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes)
        {
            try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
                { FileName = "https://go.microsoft.com/fwlink/p/?LinkId=2124703", UseShellExecute = true }); }
            catch { }
        }
    }

    private async void CoreWebView2_NavigationCompleted(object? sender, CoreWebView2NavigationCompletedEventArgs e)
    {
        if (!e.IsSuccess && (e.WebErrorStatus == CoreWebView2WebErrorStatus.Unknown ||
            e.WebErrorStatus == CoreWebView2WebErrorStatus.ConnectionAborted))
        {
            try
            {
                webView.CoreWebView2.Navigate("about:blank");
                await Task.Delay(400);
                await webView.CoreWebView2.ExecuteScriptAsync(@"(async()=>{
                    if('serviceWorker' in navigator){const r=await navigator.serviceWorker.getRegistrations();for(const s of r)await s.unregister();}
                    if('caches' in window){const n=await caches.keys();for(const k of n)await caches.delete(k);}})();");
                await Task.Delay(200);
                webView.CoreWebView2.Navigate(Program.Config.ActiveUrl);
            }
            catch { }
        }
    }

    private void SetupHybridApiInterception()
    {
        var u = Program.Config.LocalServerUrl;
        webView.CoreWebView2.AddScriptToExecuteOnDocumentCreatedAsync($@"(function(){{
            const L='{u}';const F=window.fetch;
            window.fetch=function(i,o){{let url=(typeof i==='string')?i:i.url;
            if(url.startsWith('/api/')||url.includes('/api/')){{url=L+'/api/'+url.split('/api/')[1];i=(typeof i==='string')?url:new Request(url,i);}}
            return F.call(this,i,o);}};
            const X=XMLHttpRequest.prototype.open;
            XMLHttpRequest.prototype.open=function(m,url,...a){{if(url.startsWith('/api/')||url.includes('/api/'))url=L+'/api/'+url.split('/api/')[1];return X.call(this,m,url,...a);}};
        }})();");
    }

    private void SetupTrayManager() { trayManager = new TrayManager(this); }
    private void SetupAutoUpdater() { autoUpdater = new AutoUpdater(); _ = autoUpdater.CheckForUpdatesAsync(silent: true); }


    #region Keyboard Shortcuts
    private void MainForm_KeyDown(object? sender, KeyEventArgs e)
    {
        switch (e.KeyCode)
        {
            case Keys.F11: ToggleFullscreen(); e.Handled = true; break;
            case Keys.D when e.Control && e.Shift: webView?.CoreWebView2?.OpenDevToolsWindow(); e.Handled = true; break;
            case Keys.Q when e.Control: ExitApplication(); e.Handled = true; break;
            case Keys.P when e.Control: PrintPage(); e.Handled = true; break;
            case Keys.F5: webView?.CoreWebView2?.Reload(); e.Handled = true; break;
        }
    }
    #endregion

    #region Window Management
    private void ToggleFullscreen()
    {
        if (isFullscreen) { titleBar.Visible = true; this.WindowState = previousWindowState; isFullscreen = false; }
        else { previousWindowState = this.WindowState; titleBar.Visible = false; this.WindowState = FormWindowState.Maximized; isFullscreen = true; }
    }
    private void ToggleMaximize()
    {
        this.WindowState = this.WindowState == FormWindowState.Maximized
            ? FormWindowState.Normal : FormWindowState.Maximized;
        // Update maximize button icon
        btnMaximize.Text = this.WindowState == FormWindowState.Maximized ? "\uE923" : "\uE922";
    }
    public void HideToTray() { this.Hide(); trayManager?.ShowBalloon("M-Finlogs", "Minimized to tray."); }
    public void ShowFromTray() { this.Show(); this.WindowState = FormWindowState.Normal; this.BringToFront(); this.Activate(); }
    public void ExitApplication() { SaveWindowState(); Program.Shutdown(); trayManager?.Dispose(); Application.Exit(); }
    private void SaveWindowState()
    {
        var c = Program.Config; c.IsMaximized = this.WindowState == FormWindowState.Maximized;
        if (this.WindowState == FormWindowState.Normal) { c.WindowWidth = this.Width; c.WindowHeight = this.Height; c.WindowX = this.Location.X; c.WindowY = this.Location.Y; }
        c.Save();
    }
    private void MainForm_Resize(object? sender, EventArgs e)
    {
        if (this.WindowState == FormWindowState.Minimized && Program.Config.MinimizeToTray) HideToTray();
        if (btnMaximize != null) btnMaximize.Text = this.WindowState == FormWindowState.Maximized ? "\uE923" : "\uE922";
    }
    private void MainForm_FormClosing(object? sender, FormClosingEventArgs e)
    {
        if (e.CloseReason == CloseReason.UserClosing && Program.Config.MinimizeToTray) { e.Cancel = true; HideToTray(); }
        else { SaveWindowState(); Program.Shutdown(); trayManager?.Dispose(); }
    }
    #endregion

    #region Title Bar Drag
    private void TitleBar_MouseDown(object? sender, MouseEventArgs e) { if (e.Button == MouseButtons.Left) { isDragging = true; dragStart = new Point(e.X, e.Y); } }
    private void TitleBar_MouseMove(object? sender, MouseEventArgs e)
    {
        if (!isDragging) return;
        if (this.WindowState == FormWindowState.Maximized)
        {
            var mx = MousePosition.X; var p = (double)mx / Screen.PrimaryScreen!.WorkingArea.Width;
            this.WindowState = FormWindowState.Normal;
            this.Location = new Point((int)(mx - this.Width * p), MousePosition.Y - dragStart.Y);
        }
        else this.Location = new Point(this.Location.X + e.X - dragStart.X, this.Location.Y + e.Y - dragStart.Y);
    }
    private void TitleBar_MouseUp(object? sender, MouseEventArgs e) { isDragging = false; }
    #endregion

    #region Print & PDF
    private async void PrintPage() { try { if (webView?.CoreWebView2 != null) await webView.CoreWebView2.ExecuteScriptAsync("window.print()"); } catch { } }
    public async Task SaveAsPdfAsync()
    {
        if (webView?.CoreWebView2 == null) return;
        using var dlg = new SaveFileDialog { Filter = "PDF|*.pdf", FileName = $"MFinlogs_{DateTime.Now:yyyyMMdd_HHmmss}.pdf" };
        if (dlg.ShowDialog() == DialogResult.OK) { await webView.CoreWebView2.PrintToPdfAsync(dlg.FileName); MessageBox.Show("PDF saved!", "M-Finlogs"); }
    }
    #endregion


    #region Theme Colors
    private Color GetThemeTitleBarColor() => Program.Config.Theme?.ToLower() switch
    {
        "dark" => Color.FromArgb(32, 32, 32),
        "deep blue" or "deepblue" => Color.FromArgb(15, 23, 42),
        "warm" => Color.FromArgb(55, 38, 29),
        _ => Color.FromArgb(243, 243, 243)
    };
    private Color GetThemeTitleForeColor() => Program.Config.Theme?.ToLower() switch
    {
        "dark" => Color.FromArgb(255, 255, 255),
        "deep blue" or "deepblue" => Color.FromArgb(226, 232, 240),
        "warm" => Color.FromArgb(245, 236, 228),
        _ => Color.FromArgb(23, 23, 23)
    };
    private Color GetThemeBackColor() => Program.Config.Theme?.ToLower() switch
    {
        "dark" => Color.FromArgb(0, 0, 0),
        "deep blue" or "deepblue" => Color.FromArgb(2, 6, 23),
        "warm" => Color.FromArgb(40, 26, 20),
        _ => Color.White
    };
    public void UpdateTheme(string theme)
    {
        Program.Config.Theme = theme; Program.Config.Save();
        this.Invoke(() =>
        {
            titleBar.BackColor = GetThemeTitleBarColor();
            titleLabel.ForeColor = GetThemeTitleForeColor();
            btnClose.ForeColor = GetThemeTitleForeColor();
            btnMaximize.ForeColor = GetThemeTitleForeColor();
            btnMinimize.ForeColor = GetThemeTitleForeColor();
            this.BackColor = GetThemeBackColor();
            titleBar.Invalidate();
        });
    }
    #endregion

    #region Mode
    public void SwitchMode(string mode)
    {
        Program.Config.Mode = mode; Program.Config.Save();
        lblMode.Invoke(() => { lblMode.Text = GetModeLabel(); lblMode.ForeColor = GetModeLabelColor(); });
        webView?.CoreWebView2?.Navigate(Program.Config.ActiveUrl);
    }
    private static string GetModeLabel() => Program.Config.Mode?.ToLower() switch
    {
        "online" => "Online",
        "offline" => "Offline",
        "hybrid" => "Hybrid",
        _ => "Online"
    };
    private static Color GetModeLabelColor() => Program.Config.Mode?.ToLower() switch
    {
        "online" => Color.FromArgb(34, 197, 94),
        "offline" => Color.FromArgb(251, 146, 60),
        "hybrid" => Color.FromArgb(99, 102, 241),
        _ => Color.FromArgb(34, 197, 94)
    };
    public void NavigateTo(string url) { webView?.CoreWebView2?.Navigate(url); }
    #endregion

    #region WndProc Resize Handles
    protected override void WndProc(ref Message m)
    {
        const int WM_NCHITTEST = 0x84;
        if (m.Msg == WM_NCHITTEST)
        {
            base.WndProc(ref m);
            var c = this.PointToClient(Cursor.Position);
            const int g = 5;
            if (c.Y < g) { m.Result = c.X < g ? (IntPtr)13 : c.X > Width - g ? (IntPtr)14 : (IntPtr)12; return; }
            if (c.Y > Height - g) { m.Result = c.X < g ? (IntPtr)16 : c.X > Width - g ? (IntPtr)17 : (IntPtr)15; return; }
            if (c.X < g) { m.Result = (IntPtr)10; return; }
            if (c.X > Width - g) { m.Result = (IntPtr)11; return; }
        }
        base.WndProc(ref m);
    }
    #endregion
}
