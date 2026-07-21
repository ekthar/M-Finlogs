using Microsoft.Web.WebView2.WinForms;
using Microsoft.Web.WebView2.Core;
using MFinlogs.Desktop.Config;
using System.Runtime.InteropServices;

namespace MFinlogs.Desktop;

/// <summary>
/// Main form: native Windows title bar + WebView2 filling 100% client area.
/// No custom chrome = no overlap with web app navigation.
/// </summary>
public partial class MainForm : Form
{
    private WebView2 webView = null!;
    private TrayManager? trayManager;
    private AutoUpdater? autoUpdater;
    private GlobalHotkey? globalHotkey;

    public MainForm()
    {
        InitializeComponent();
        SetupForm();
        SetupTrayManager();
        SetupAutoUpdater();
        this.Shown += MainForm_Shown;
    }

    private async void MainForm_Shown(object? sender, EventArgs e)
    {
        this.Shown -= MainForm_Shown;
        RegisterGlobalHotkey();
        ProtocolHandler.Register();
        await InitializeWebViewAsync();

        // Handle --url argument (deep link)
        var args = Environment.GetCommandLineArgs();
        var urlIdx = Array.IndexOf(args, "--url");
        if (urlIdx >= 0 && urlIdx + 1 < args.Length)
            HandleDeepLink(args[urlIdx + 1]);
    }

    private void SetupForm()
    {
        var config = Program.Config;

        // Native Windows title bar — zero overlap, OS handles min/max/close
        this.Text = "M-Finlogs";
        this.FormBorderStyle = FormBorderStyle.Sizable;
        this.MinimumSize = new Size(1024, 600);
        this.StartPosition = FormStartPosition.CenterScreen;
        this.BackColor = Color.White;
        this.DoubleBuffered = true;

        // Icon
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
            try { this.Icon = new Icon(iconPath); } catch { }

        // Restore window state
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

        // WebView fills 100% of client area
        webView = new WebView2 { Dock = DockStyle.Fill };
        this.Controls.Add(webView);

        // Events
        this.FormClosing += MainForm_FormClosing;
        this.Resize += MainForm_Resize;
        this.KeyPreview = true;
        this.KeyDown += MainForm_KeyDown;
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

    #region Setup & Integrations
    private void SetupTrayManager() { trayManager = new TrayManager(this); }
    private void SetupAutoUpdater() { autoUpdater = new AutoUpdater(); _ = autoUpdater.CheckForUpdatesAsync(silent: true); }
    private void RegisterGlobalHotkey() { globalHotkey = new GlobalHotkey(this); }

    public void HandleDeepLink(string url)
    {
        var path = ProtocolHandler.ParseUrl(url);
        if (path != null)
        {
            NavigateTo(Program.Config.OnlineUrl.TrimEnd('/') + path);
            ShowFromTray();
        }
    }
    #endregion

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
            // Ctrl+, = Open Settings
            case Keys.Oemcomma when e.Control:
                Program.OpenSettings(this);
                // Reload URL in case it changed
                webView?.CoreWebView2?.Navigate(Program.Config.ActiveUrl);
                e.Handled = true;
                break;
        }
    }
    #endregion

    #region Window Management
    private bool isFullscreen = false;
    private FormWindowState prevState;
    private FormBorderStyle prevBorder;

    private void ToggleFullscreen()
    {
        if (isFullscreen)
        {
            this.FormBorderStyle = prevBorder;
            this.WindowState = prevState;
            isFullscreen = false;
        }
        else
        {
            prevState = this.WindowState;
            prevBorder = this.FormBorderStyle;
            this.FormBorderStyle = FormBorderStyle.None;
            this.WindowState = FormWindowState.Maximized;
            isFullscreen = true;
        }
    }

    public void HideToTray()
    {
        this.Hide();
        trayManager?.ShowBalloon("M-Finlogs", "Minimized to system tray.");
    }

    public void ShowFromTray()
    {
        this.Show();
        this.WindowState = FormWindowState.Normal;
        this.BringToFront();
        this.Activate();
    }

    public void ExitApplication()
    {
        SaveWindowState();
        Program.Shutdown();
        globalHotkey?.Dispose();
        trayManager?.Dispose();
        Application.Exit();
    }

    private void SaveWindowState()
    {
        var c = Program.Config;
        c.IsMaximized = this.WindowState == FormWindowState.Maximized;
        if (this.WindowState == FormWindowState.Normal)
        {
            c.WindowWidth = this.Width;
            c.WindowHeight = this.Height;
            c.WindowX = this.Location.X;
            c.WindowY = this.Location.Y;
        }
        c.Save();
    }

    private void MainForm_Resize(object? sender, EventArgs e)
    {
        if (this.WindowState == FormWindowState.Minimized && Program.Config.MinimizeToTray)
            HideToTray();
    }

    private void MainForm_FormClosing(object? sender, FormClosingEventArgs e)
    {
        if (e.CloseReason == CloseReason.UserClosing && Program.Config.MinimizeToTray)
        {
            e.Cancel = true;
            HideToTray();
        }
        else
        {
            SaveWindowState();
            Program.Shutdown();
            globalHotkey?.Dispose();
            trayManager?.Dispose();
        }
    }
    #endregion

    #region Print & PDF
    private async void PrintPage()
    {
        try { if (webView?.CoreWebView2 != null) await webView.CoreWebView2.ExecuteScriptAsync("window.print()"); }
        catch { }
    }

    public async Task SaveAsPdfAsync()
    {
        if (webView?.CoreWebView2 == null) return;
        using var dlg = new SaveFileDialog { Filter = "PDF files (*.pdf)|*.pdf", FileName = $"MFinlogs_{DateTime.Now:yyyyMMdd_HHmmss}.pdf" };
        if (dlg.ShowDialog() == DialogResult.OK)
        {
            await webView.CoreWebView2.PrintToPdfAsync(dlg.FileName);
            MessageBox.Show("PDF saved!", "M-Finlogs", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }
    #endregion

    #region Public API
    public void SwitchMode(string mode)
    {
        Program.Config.Mode = mode;
        Program.Config.Save();
        webView?.CoreWebView2?.Navigate(Program.Config.ActiveUrl);
    }

    public void NavigateTo(string url) { webView?.CoreWebView2?.Navigate(url); }
    #endregion

    #region WndProc — Global hotkey + single-instance activation
    protected override void WndProc(ref Message m)
    {
        if (globalHotkey != null && globalHotkey.IsOurHotkey(m))
        {
            ShowFromTray();
            return;
        }
        if (m.Msg == NativeMethods.ActivateMessage)
        {
            ShowFromTray();
            return;
        }
        base.WndProc(ref m);
    }
    #endregion
}
