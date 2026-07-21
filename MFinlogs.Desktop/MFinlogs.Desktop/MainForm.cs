using Microsoft.Web.WebView2.WinForms;
using Microsoft.Web.WebView2.Core;
using MFinlogs.Desktop.Config;
using MFinlogs.Desktop.UI;
using MFinlogs.Desktop.UI.Controls;
using System.Runtime.InteropServices;

namespace MFinlogs.Desktop;

/// <summary>
/// Main application form: native Windows title bar + WebView2 filling 100% client area.
/// Enhanced with: loading overlay, toast notifications, connection indicator,
/// smooth window transitions, and premium visual polish.
/// 
/// All original functionality preserved: WebView2, fullscreen, keyboard shortcuts,
/// tray integration, PDF export, mode switching, deep links.
/// </summary>
public partial class MainForm : Form
{
    private WebView2 _webView = null!;
    private ModernLoadingOverlay _loadingOverlay = null!;
    private ModernToast _toast = null!;
    private TrayManager? _trayManager;
    private AutoUpdater? _autoUpdater;
    private GlobalHotkey? _globalHotkey;
    private bool _webViewReady;

    public MainForm()
    {
        DesignTokens.Typography.Initialize();
        InitializeComponent();
        SetupForm();
        SetupOverlays();
        SetupTrayManager();
        SetupAutoUpdater();
        Shown += MainForm_Shown;
    }

    private async void MainForm_Shown(object? sender, EventArgs e)
    {
        Shown -= MainForm_Shown;
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

        Text = "M-Finlogs";
        FormBorderStyle = FormBorderStyle.Sizable;
        MinimumSize = new Size(1024, 600);
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = DesignTokens.Colors.Background;
        DoubleBuffered = true;

        // Icon
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
            try { Icon = new Icon(iconPath); } catch { }

        // Restore window state
        if (config.WindowWidth > 0 && config.WindowHeight > 0)
            Size = new Size(config.WindowWidth, config.WindowHeight);
        else
            Size = new Size(1280, 800);

        if (config.WindowX >= 0 && config.WindowY >= 0)
        {
            Location = new Point(config.WindowX, config.WindowY);
            StartPosition = FormStartPosition.Manual;
        }
        if (config.IsMaximized)
            WindowState = FormWindowState.Maximized;

        // WebView fills 100% of client area
        _webView = new WebView2 { Dock = DockStyle.Fill, Visible = false };
        Controls.Add(_webView);

        // Events
        FormClosing += MainForm_FormClosing;
        Resize += MainForm_Resize;
        KeyPreview = true;
        KeyDown += MainForm_KeyDown;
    }

    private void SetupOverlays()
    {
        // Loading overlay (shows until WebView is ready)
        _loadingOverlay = new ModernLoadingOverlay();
        Controls.Add(_loadingOverlay);
        _loadingOverlay.BringToFront();
        _loadingOverlay.ShowLoading();

        // Toast notification (top-right corner)
        _toast = new ModernToast
        {
            Anchor = AnchorStyles.Top | AnchorStyles.Right
        };
        Controls.Add(_toast);
        _toast.BringToFront();
        PositionToast();
    }

    private void PositionToast()
    {
        _toast.Location = new Point(
            ClientSize.Width - _toast.Width - 16, 16);
    }

    protected override void OnClientSizeChanged(EventArgs e)
    {
        base.OnClientSizeChanged(e);
        PositionToast();
    }


    private async Task InitializeWebViewAsync()
    {
        if (!_webView.IsHandleCreated) _webView.CreateControl();

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

            await _webView.EnsureCoreWebView2Async(env);

            _webView.CoreWebView2.Settings.IsStatusBarEnabled = false;
            _webView.CoreWebView2.Settings.AreDefaultContextMenusEnabled = true;
            _webView.CoreWebView2.Settings.IsZoomControlEnabled = true;
            _webView.CoreWebView2.Settings.AreDevToolsEnabled = true;

            try { await _webView.CoreWebView2.Profile.ClearBrowsingDataAsync(
                CoreWebView2BrowsingDataKinds.ServiceWorkers); } catch { }

            _webView.CoreWebView2.NavigationCompleted += CoreWebView2_NavigationCompleted;
            _webView.CoreWebView2.NewWindowRequested += (s, ev) =>
            { ev.Handled = true; _webView.CoreWebView2.Navigate(ev.Uri); };

            if (Program.Config.IsHybridMode)
                SetupHybridApiInterception();

            _webView.CoreWebView2.Navigate(Program.Config.ActiveUrl);
        }
        catch (COMException comEx) when ((uint)comEx.HResult == 0x8007139F)
        { ShowWebView2InstallPrompt(); }
        catch (WebView2RuntimeNotFoundException)
        { ShowWebView2InstallPrompt(); }
        catch (Exception ex)
        {
            _loadingOverlay.HideLoading();
            _toast.Show($"WebView2 error: {ex.Message}", ModernToast.ToastType.Error);
        }
    }

    private void ShowWebView2InstallPrompt()
    {
        _loadingOverlay.HideLoading();
        if (MessageBox.Show("WebView2 Runtime is required.\nDownload now?", "M-Finlogs",
            MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes)
        {
            try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
                { FileName = "https://go.microsoft.com/fwlink/p/?LinkId=2124703", UseShellExecute = true }); }
            catch { }
        }
    }

    private async void CoreWebView2_NavigationCompleted(object? sender,
        CoreWebView2NavigationCompletedEventArgs e)
    {
        if (e.IsSuccess)
        {
            // Successfully loaded — reveal WebView with smooth transition
            if (!_webViewReady)
            {
                _webViewReady = true;
                _webView.Visible = true;
                _loadingOverlay.HideLoading();
            }
        }
        else if (e.WebErrorStatus == CoreWebView2WebErrorStatus.Unknown ||
            e.WebErrorStatus == CoreWebView2WebErrorStatus.ConnectionAborted)
        {
            try
            {
                _webView.CoreWebView2.Navigate("about:blank");
                await Task.Delay(400);
                await _webView.CoreWebView2.ExecuteScriptAsync(@"(async()=>{
                    if('serviceWorker' in navigator){const r=await navigator.serviceWorker.getRegistrations();for(const s of r)await s.unregister();}
                    if('caches' in window){const n=await caches.keys();for(const k of n)await caches.delete(k);}})();");
                await Task.Delay(200);
                _webView.CoreWebView2.Navigate(Program.Config.ActiveUrl);
            }
            catch { }
        }
    }


    private void SetupHybridApiInterception()
    {
        var u = Program.Config.LocalServerUrl;
        _webView.CoreWebView2.AddScriptToExecuteOnDocumentCreatedAsync($@"(function(){{
            const L='{u}';const F=window.fetch;
            window.fetch=function(i,o){{let url=(typeof i==='string')?i:i.url;
            if(url.startsWith('/api/')||url.includes('/api/')){{url=L+'/api/'+url.split('/api/')[1];i=(typeof i==='string')?url:new Request(url,i);}}
            return F.call(this,i,o);}};
            const X=XMLHttpRequest.prototype.open;
            XMLHttpRequest.prototype.open=function(m,url,...a){{if(url.startsWith('/api/')||url.includes('/api/'))url=L+'/api/'+url.split('/api/')[1];return X.call(this,m,url,...a);}};
        }})();");
    }

    #region Setup & Integrations
    private void SetupTrayManager() { _trayManager = new TrayManager(this); }
    private void SetupAutoUpdater()
    { _autoUpdater = new AutoUpdater(); _ = _autoUpdater.CheckForUpdatesAsync(silent: true); }
    private void RegisterGlobalHotkey() { _globalHotkey = new GlobalHotkey(this); }

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
            case Keys.F11:
                ToggleFullscreen(); e.Handled = true; break;
            case Keys.D when e.Control && e.Shift:
                _webView?.CoreWebView2?.OpenDevToolsWindow(); e.Handled = true; break;
            case Keys.Q when e.Control:
                ExitApplication(); e.Handled = true; break;
            case Keys.P when e.Control:
                PrintPage(); e.Handled = true; break;
            case Keys.F5:
                _webView?.CoreWebView2?.Reload(); e.Handled = true; break;
            case Keys.Oemcomma when e.Control:
                Program.OpenSettings(this);
                _webView?.CoreWebView2?.Navigate(Program.Config.ActiveUrl);
                e.Handled = true;
                break;
        }
    }
    #endregion

    #region Window Management
    private bool _isFullscreen;
    private FormWindowState _prevState;
    private FormBorderStyle _prevBorder;

    private void ToggleFullscreen()
    {
        if (_isFullscreen)
        {
            FormBorderStyle = _prevBorder;
            WindowState = _prevState;
            _isFullscreen = false;
        }
        else
        {
            _prevState = WindowState;
            _prevBorder = FormBorderStyle;
            FormBorderStyle = FormBorderStyle.None;
            WindowState = FormWindowState.Maximized;
            _isFullscreen = true;
        }
    }

    public void HideToTray()
    {
        Hide();
        _trayManager?.ShowBalloon("M-Finlogs", "Minimized to system tray.");
    }

    public void ShowFromTray()
    {
        Show();
        WindowState = FormWindowState.Normal;
        BringToFront();
        Activate();
    }

    public void ExitApplication()
    {
        SaveWindowState();
        Program.Shutdown();
        _globalHotkey?.Dispose();
        _trayManager?.Dispose();
        Application.Exit();
    }


    private void SaveWindowState()
    {
        var c = Program.Config;
        c.IsMaximized = WindowState == FormWindowState.Maximized;
        if (WindowState == FormWindowState.Normal)
        {
            c.WindowWidth = Width;
            c.WindowHeight = Height;
            c.WindowX = Location.X;
            c.WindowY = Location.Y;
        }
        c.Save();
    }

    private void MainForm_Resize(object? sender, EventArgs e)
    {
        if (WindowState == FormWindowState.Minimized && Program.Config.MinimizeToTray)
            HideToTray();
        PositionToast();
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
            _globalHotkey?.Dispose();
            _trayManager?.Dispose();
        }
    }
    #endregion

    #region Print & PDF
    private async void PrintPage()
    {
        try { if (_webView?.CoreWebView2 != null)
            await _webView.CoreWebView2.ExecuteScriptAsync("window.print()"); }
        catch { }
    }

    public async Task SaveAsPdfAsync()
    {
        if (_webView?.CoreWebView2 == null) return;
        using var dlg = new SaveFileDialog
        {
            Filter = "PDF files (*.pdf)|*.pdf",
            FileName = $"MFinlogs_{DateTime.Now:yyyyMMdd_HHmmss}.pdf"
        };
        if (dlg.ShowDialog() == DialogResult.OK)
        {
            await _webView.CoreWebView2.PrintToPdfAsync(dlg.FileName);
            _toast.Show("PDF saved successfully!", ModernToast.ToastType.Success);
        }
    }
    #endregion

    #region Public API
    public void SwitchMode(string mode)
    {
        Program.Config.Mode = mode;
        Program.Config.Save();
        _webView?.CoreWebView2?.Navigate(Program.Config.ActiveUrl);
        _toast.Show($"Switched to {mode} mode", ModernToast.ToastType.Info);
    }

    public void NavigateTo(string url) { _webView?.CoreWebView2?.Navigate(url); }

    /// <summary>Show a toast notification on the main form</summary>
    public void ShowToast(string message, ModernToast.ToastType type = ModernToast.ToastType.Info)
    {
        if (InvokeRequired)
            Invoke(() => _toast.Show(message, type));
        else
            _toast.Show(message, type);
    }
    #endregion

    #region WndProc — Global hotkey + single-instance activation
    protected override void WndProc(ref Message m)
    {
        if (_globalHotkey != null && _globalHotkey.IsOurHotkey(m))
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
