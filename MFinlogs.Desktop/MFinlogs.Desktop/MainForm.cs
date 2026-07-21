using Microsoft.Web.WebView2.WinForms;
using Microsoft.Web.WebView2.Core;
using MFinlogs.Desktop.Config;
using System.Drawing.Text;
using System.Runtime.InteropServices;

namespace MFinlogs.Desktop;

/// <summary>
/// Main form: borderless window with WebView2 filling 100% of the space.
/// No custom title bar — the web app's own nav bar acts as the visual header.
/// Window controls (minimize/maximize/close) float as an overlay on the top-right.
/// The top 40px of the WebView acts as the drag region for moving the window.
/// </summary>
public partial class MainForm : Form
{
    private WebView2 webView = null!;
    private TrayManager? trayManager;
    private AutoUpdater? autoUpdater;
    private bool isFullscreen = false;
    private FormWindowState previousWindowState;

    // Overlay window control buttons
    private Panel controlsOverlay = null!;
    private Button btnClose = null!;
    private Button btnMaximize = null!;
    private Button btnMinimize = null!;

    // Drag region (invisible panel at top for window dragging)
    private Panel dragRegion = null!;

    // Drag state
    private bool isDragging = false;
    private Point dragStart;

    // Font
    private Font btnFont = null!;

    public MainForm()
    {
        InitializeComponent();
        btnFont = new Font("Segoe MDL2 Assets", 10f, FontStyle.Regular);
        SetupForm();
        SetupWebView();
        SetupWindowControls();
        SetupDragRegion();
        SetupTrayManager();
        SetupAutoUpdater();
        this.Shown += MainForm_Shown;
    }

    private async void MainForm_Shown(object? sender, EventArgs e)
    {
        this.Shown -= MainForm_Shown;
        await InitializeWebViewAsync();
    }

    private void SetupForm()
    {
        var config = Program.Config;
        this.Text = "M-Finlogs";
        this.MinimumSize = new Size(1024, 600);
        this.StartPosition = FormStartPosition.CenterScreen;
        this.FormBorderStyle = FormBorderStyle.None;
        this.BackColor = Color.White;
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

        // Set icon
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
            try { this.Icon = new Icon(iconPath); } catch { }
    }

    /// <summary>
    /// WebView fills the ENTIRE form — no title bar, no margins.
    /// The web app's own header becomes the visual "title bar".
    /// </summary>
    private void SetupWebView()
    {
        webView = new WebView2
        {
            Dock = DockStyle.Fill
        };
        this.Controls.Add(webView);
    }

    /// <summary>
    /// Floating window control buttons overlaid on top-right corner.
    /// These sit ON TOP of the WebView with a semi-transparent background.
    /// </summary>
    private void SetupWindowControls()
    {
        controlsOverlay = new Panel
        {
            Size = new Size(138, 32),
            BackColor = Color.Transparent,
            Anchor = AnchorStyles.Top | AnchorStyles.Right
        };
        controlsOverlay.Location = new Point(this.ClientSize.Width - 138, 0);

        btnMinimize = MakeWinBtn("\uE921", 0);
        btnMaximize = MakeWinBtn("\uE922", 46);
        btnClose = MakeWinBtn("\uE8BB", 92);
        btnClose.FlatAppearance.MouseOverBackColor = Color.FromArgb(232, 17, 35);
        btnClose.FlatAppearance.MouseDownBackColor = Color.FromArgb(196, 13, 28);

        btnMinimize.Click += (s, e) => this.WindowState = FormWindowState.Minimized;
        btnMaximize.Click += (s, e) => ToggleMaximize();
        btnClose.Click += (s, e) =>
        {
            if (Program.Config.MinimizeToTray) HideToTray();
            else ExitApplication();
        };

        controlsOverlay.Controls.Add(btnMinimize);
        controlsOverlay.Controls.Add(btnMaximize);
        controlsOverlay.Controls.Add(btnClose);

        this.Controls.Add(controlsOverlay);
        controlsOverlay.BringToFront();
    }

    private Button MakeWinBtn(string icon, int x)
    {
        var btn = new Button
        {
            Text = icon,
            Font = btnFont,
            Size = new Size(46, 32),
            Location = new Point(x, 0),
            FlatStyle = FlatStyle.Flat,
            ForeColor = Color.FromArgb(23, 23, 23),
            BackColor = Color.Transparent,
            Cursor = Cursors.Default,
            TabStop = false
        };
        btn.FlatAppearance.BorderSize = 0;
        btn.FlatAppearance.MouseOverBackColor = Color.FromArgb(40, 0, 0, 0);
        btn.FlatAppearance.MouseDownBackColor = Color.FromArgb(60, 0, 0, 0);
        return btn;
    }

    /// <summary>
    /// Invisible drag region at the top of the form (height=6px).
    /// The actual window drag is handled by WndProc hit testing.
    /// </summary>
    private void SetupDragRegion()
    {
        // We use WndProc HTCAPTION for dragging instead of a panel
        // This allows dragging anywhere on the top 40px that isn't a button
    }

    private async Task InitializeWebViewAsync()
    {
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
        if (isFullscreen) { controlsOverlay.Visible = true; this.WindowState = previousWindowState; isFullscreen = false; }
        else { previousWindowState = this.WindowState; controlsOverlay.Visible = false; this.WindowState = FormWindowState.Maximized; isFullscreen = true; }
    }
    private void ToggleMaximize()
    {
        this.WindowState = this.WindowState == FormWindowState.Maximized
            ? FormWindowState.Normal : FormWindowState.Maximized;
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
        // Reposition controls overlay on resize
        if (controlsOverlay != null) controlsOverlay.Location = new Point(this.ClientSize.Width - 138, 0);
    }
    private void MainForm_FormClosing(object? sender, FormClosingEventArgs e)
    {
        if (e.CloseReason == CloseReason.UserClosing && Program.Config.MinimizeToTray) { e.Cancel = true; HideToTray(); }
        else { SaveWindowState(); Program.Shutdown(); trayManager?.Dispose(); }
    }
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

    #region Mode & Theme
    public void SwitchMode(string mode)
    {
        Program.Config.Mode = mode; Program.Config.Save();
        webView?.CoreWebView2?.Navigate(Program.Config.ActiveUrl);
    }
    public void UpdateTheme(string theme) { Program.Config.Theme = theme; Program.Config.Save(); }
    public void NavigateTo(string url) { webView?.CoreWebView2?.Navigate(url); }
    #endregion

    #region WndProc — Resize handles + drag region
    protected override void WndProc(ref Message m)
    {
        const int WM_NCHITTEST = 0x84;
        const int HTCAPTION = 2;

        if (m.Msg == WM_NCHITTEST)
        {
            base.WndProc(ref m);
            var cursor = this.PointToClient(Cursor.Position);
            const int resizeGrip = 5;
            const int dragHeight = 40; // Top 40px = drag region (matches web app nav height)

            // Resize edges
            if (cursor.Y < resizeGrip)
            {
                m.Result = cursor.X < resizeGrip ? (IntPtr)13 : cursor.X > Width - resizeGrip ? (IntPtr)14 : (IntPtr)12;
                return;
            }
            if (cursor.Y > Height - resizeGrip)
            {
                m.Result = cursor.X < resizeGrip ? (IntPtr)16 : cursor.X > Width - resizeGrip ? (IntPtr)17 : (IntPtr)15;
                return;
            }
            if (cursor.X < resizeGrip) { m.Result = (IntPtr)10; return; }
            if (cursor.X > Width - resizeGrip) { m.Result = (IntPtr)11; return; }

            // Drag region: top 40px, but NOT over the window control buttons (right 138px)
            if (cursor.Y < dragHeight && cursor.X < (this.ClientSize.Width - 138))
            {
                m.Result = (IntPtr)HTCAPTION;
                return;
            }
        }

        base.WndProc(ref m);
    }
    #endregion
}
