using Microsoft.Web.WebView2.WinForms;
using Microsoft.Web.WebView2.Core;
using MFinlogs.Desktop.Config;

namespace MFinlogs.Desktop;

/// <summary>
/// Main application window with WebView2, custom title bar, and keyboard shortcuts
/// </summary>
public partial class MainForm : Form
{
    private WebView2 webView = null!;
    private TrayManager? trayManager;
    private AutoUpdater? autoUpdater;
    private bool isFullscreen = false;
    private FormWindowState previousWindowState;
    private FormBorderStyle previousBorderStyle;

    // Custom title bar controls
    private Panel titleBar = null!;
    private Label titleLabel = null!;
    private PictureBox titleIcon = null!;
    private Button btnMinimize = null!;
    private Button btnMaximize = null!;
    private Button btnClose = null!;
    private Label lblMode = null!;

    // Drag support
    private bool isDragging = false;
    private Point dragStart;

    public MainForm()
    {
        InitializeComponent();
        SetupForm();
        SetupTitleBar();
        SetupWebView();
        SetupTrayManager();
        SetupAutoUpdater();
    }

    private void SetupForm()
    {
        var config = Program.Config;

        this.Text = "M-Finlogs";
        this.MinimumSize = new Size(1024, 600);
        this.StartPosition = FormStartPosition.CenterScreen;
        this.FormBorderStyle = FormBorderStyle.None; // Custom title bar
        this.BackColor = GetThemeBackColor();

        // Restore window position/size
        if (config.WindowWidth > 0 && config.WindowHeight > 0)
        {
            this.Size = new Size(config.WindowWidth, config.WindowHeight);
        }
        else
        {
            this.Size = new Size(1280, 800);
        }

        if (config.WindowX >= 0 && config.WindowY >= 0)
        {
            this.Location = new Point(config.WindowX, config.WindowY);
            this.StartPosition = FormStartPosition.Manual;
        }

        if (config.IsMaximized)
        {
            this.WindowState = FormWindowState.Maximized;
        }

        // Handle window state changes
        this.Resize += MainForm_Resize;
        this.FormClosing += MainForm_FormClosing;
        this.KeyPreview = true;
        this.KeyDown += MainForm_KeyDown;
    }

    private void SetupTitleBar()
    {
        var themeColor = GetThemeTitleBarColor();

        // Title bar panel
        titleBar = new Panel
        {
            Dock = DockStyle.Top,
            Height = 36,
            BackColor = themeColor,
            Padding = new Padding(8, 0, 0, 0)
        };

        // App icon
        titleIcon = new PictureBox
        {
            Size = new Size(20, 20),
            Location = new Point(10, 8),
            SizeMode = PictureBoxSizeMode.StretchImage
        };

        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
        {
            try
            {
                titleIcon.Image = new Icon(iconPath, 20, 20).ToBitmap();
            }
            catch { }
        }

        // Title text
        titleLabel = new Label
        {
            Text = "M-Finlogs",
            ForeColor = GetThemeTitleForeColor(),
            Font = new Font("Segoe UI", 10f, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(36, 9),
            BackColor = Color.Transparent
        };

        // Mode indicator
        lblMode = new Label
        {
            Text = Program.Config.IsOnlineMode ? "● Online" : "● Offline",
            ForeColor = Program.Config.IsOnlineMode ? Color.FromArgb(34, 197, 94) : Color.FromArgb(251, 146, 60),
            Font = new Font("Segoe UI", 8f, FontStyle.Regular),
            AutoSize = true,
            Location = new Point(130, 11),
            BackColor = Color.Transparent
        };

        // Window control buttons
        var buttonWidth = 46;
        var buttonHeight = 36;

        btnClose = CreateTitleButton("✕", buttonWidth, buttonHeight);
        btnClose.FlatAppearance.MouseOverBackColor = Color.FromArgb(232, 17, 35);
        btnClose.Click += (s, e) =>
        {
            if (Program.Config.MinimizeToTray)
            {
                HideToTray();
            }
            else
            {
                ExitApplication();
            }
        };

        btnMaximize = CreateTitleButton("□", buttonWidth, buttonHeight);
        btnMaximize.Click += (s, e) => ToggleMaximize();

        btnMinimize = CreateTitleButton("─", buttonWidth, buttonHeight);
        btnMinimize.Click += (s, e) => this.WindowState = FormWindowState.Minimized;

        // Layout buttons from right
        btnClose.Dock = DockStyle.Right;
        btnMaximize.Dock = DockStyle.Right;
        btnMinimize.Dock = DockStyle.Right;

        titleBar.Controls.Add(titleIcon);
        titleBar.Controls.Add(titleLabel);
        titleBar.Controls.Add(lblMode);
        titleBar.Controls.Add(btnClose);
        titleBar.Controls.Add(btnMaximize);
        titleBar.Controls.Add(btnMinimize);

        // Drag support for title bar
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

    private Button CreateTitleButton(string text, int width, int height)
    {
        return new Button
        {
            Text = text,
            Size = new Size(width, height),
            FlatStyle = FlatStyle.Flat,
            ForeColor = GetThemeTitleForeColor(),
            BackColor = Color.Transparent,
            Font = new Font("Segoe UI", 10f),
            TabStop = false,
            FlatAppearance =
            {
                BorderSize = 0,
                MouseOverBackColor = Color.FromArgb(50, 255, 255, 255)
            }
        };
    }

    private async void SetupWebView()
    {
        webView = new WebView2
        {
            Dock = DockStyle.Fill
        };

        this.Controls.Add(webView);
        webView.BringToFront();
        titleBar.BringToFront();

        try
        {
            // Configure WebView2 environment
            var userDataFolder = Path.Combine(AppConfig.AppDataDir, "WebView2");
            var env = await CoreWebView2Environment.CreateAsync(null, userDataFolder);
            await webView.EnsureCoreWebView2Async(env);

            // Configure settings
            webView.CoreWebView2.Settings.IsStatusBarEnabled = false;
            webView.CoreWebView2.Settings.AreDefaultContextMenusEnabled = true;
            webView.CoreWebView2.Settings.IsZoomControlEnabled = true;
            webView.CoreWebView2.Settings.AreDevToolsEnabled = true;

            // Handle navigation events
            webView.CoreWebView2.NavigationStarting += CoreWebView2_NavigationStarting;
            webView.CoreWebView2.NavigationCompleted += CoreWebView2_NavigationCompleted;
            webView.CoreWebView2.NewWindowRequested += CoreWebView2_NewWindowRequested;

            // Navigate to the appropriate URL
            webView.CoreWebView2.Navigate(Program.Config.ActiveUrl);
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                $"Failed to initialize WebView2:\n{ex.Message}\n\nPlease ensure WebView2 Runtime is installed.",
                "M-Finlogs - Error",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }

    private void SetupTrayManager()
    {
        trayManager = new TrayManager(this);
    }

    private void SetupAutoUpdater()
    {
        autoUpdater = new AutoUpdater();
        _ = autoUpdater.CheckForUpdatesAsync(silent: true);
    }

    #region WebView2 Events

    private void CoreWebView2_NavigationStarting(object? sender, CoreWebView2NavigationStartingEventArgs e)
    {
        // Allow all navigations within the app
    }

    private void CoreWebView2_NavigationCompleted(object? sender, CoreWebView2NavigationCompletedEventArgs e)
    {
        if (!e.IsSuccess)
        {
            System.Diagnostics.Debug.WriteLine($"Navigation failed: {e.WebErrorStatus}");
        }
    }

    private void CoreWebView2_NewWindowRequested(object? sender, CoreWebView2NewWindowRequestedEventArgs e)
    {
        // Open links in the same window
        e.Handled = true;
        webView.CoreWebView2.Navigate(e.Uri);
    }

    #endregion

    #region Keyboard Shortcuts

    private void MainForm_KeyDown(object? sender, KeyEventArgs e)
    {
        switch (e.KeyCode)
        {
            case Keys.F11:
                ToggleFullscreen();
                e.Handled = true;
                break;

            case Keys.D when e.Control && e.Shift:
                // Ctrl+Shift+D = DevTools
                webView?.CoreWebView2?.OpenDevToolsWindow();
                e.Handled = true;
                break;

            case Keys.Q when e.Control:
                // Ctrl+Q = Quit
                ExitApplication();
                e.Handled = true;
                break;

            case Keys.P when e.Control:
                // Ctrl+P = Print
                PrintPage();
                e.Handled = true;
                break;

            case Keys.F5:
                // F5 = Refresh
                webView?.CoreWebView2?.Reload();
                e.Handled = true;
                break;
        }
    }

    #endregion

    #region Window Management

    private void ToggleFullscreen()
    {
        if (isFullscreen)
        {
            // Restore
            this.FormBorderStyle = FormBorderStyle.None;
            this.WindowState = previousWindowState;
            titleBar.Visible = true;
            isFullscreen = false;
        }
        else
        {
            // Fullscreen
            previousWindowState = this.WindowState;
            previousBorderStyle = this.FormBorderStyle;
            titleBar.Visible = false;
            this.WindowState = FormWindowState.Normal;
            this.FormBorderStyle = FormBorderStyle.None;
            this.WindowState = FormWindowState.Maximized;
            isFullscreen = true;
        }
    }

    private void ToggleMaximize()
    {
        if (this.WindowState == FormWindowState.Maximized)
        {
            this.WindowState = FormWindowState.Normal;
            btnMaximize.Text = "□";
        }
        else
        {
            this.WindowState = FormWindowState.Maximized;
            btnMaximize.Text = "❐";
        }
    }

    public void HideToTray()
    {
        this.Hide();
        trayManager?.ShowBalloon("M-Finlogs", "Application minimized to system tray.");
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
        trayManager?.Dispose();
        Application.Exit();
    }

    private void SaveWindowState()
    {
        var config = Program.Config;
        config.IsMaximized = this.WindowState == FormWindowState.Maximized;

        if (this.WindowState == FormWindowState.Normal)
        {
            config.WindowWidth = this.Width;
            config.WindowHeight = this.Height;
            config.WindowX = this.Location.X;
            config.WindowY = this.Location.Y;
        }

        config.Save();
    }

    private void MainForm_Resize(object? sender, EventArgs e)
    {
        if (this.WindowState == FormWindowState.Minimized && Program.Config.MinimizeToTray)
        {
            HideToTray();
        }

        // Update maximize button text
        if (btnMaximize != null)
        {
            btnMaximize.Text = this.WindowState == FormWindowState.Maximized ? "❐" : "□";
        }
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
            trayManager?.Dispose();
        }
    }

    #endregion

    #region Title Bar Drag

    private void TitleBar_MouseDown(object? sender, MouseEventArgs e)
    {
        if (e.Button == MouseButtons.Left)
        {
            isDragging = true;
            dragStart = new Point(e.X, e.Y);
        }
    }

    private void TitleBar_MouseMove(object? sender, MouseEventArgs e)
    {
        if (isDragging)
        {
            if (this.WindowState == FormWindowState.Maximized)
            {
                // Restore before dragging
                var mouseX = MousePosition.X;
                var proportion = (double)mouseX / Screen.PrimaryScreen!.WorkingArea.Width;
                this.WindowState = FormWindowState.Normal;
                this.Location = new Point(
                    (int)(mouseX - this.Width * proportion),
                    MousePosition.Y - dragStart.Y);
                btnMaximize.Text = "□";
            }
            else
            {
                this.Location = new Point(
                    this.Location.X + e.X - dragStart.X,
                    this.Location.Y + e.Y - dragStart.Y);
            }
        }
    }

    private void TitleBar_MouseUp(object? sender, MouseEventArgs e)
    {
        isDragging = false;
    }

    #endregion

    #region Print Support

    private async void PrintPage()
    {
        try
        {
            if (webView?.CoreWebView2 != null)
            {
                // Use WebView2 built-in print dialog
                await webView.CoreWebView2.ExecuteScriptAsync("window.print()");
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"Print error: {ex.Message}");
        }
    }

    /// <summary>
    /// Save page as PDF
    /// </summary>
    public async Task SaveAsPdfAsync()
    {
        try
        {
            if (webView?.CoreWebView2 == null) return;

            using var saveDialog = new SaveFileDialog
            {
                Filter = "PDF files (*.pdf)|*.pdf",
                DefaultExt = "pdf",
                FileName = $"MFinlogs_Report_{DateTime.Now:yyyyMMdd_HHmmss}.pdf"
            };

            if (saveDialog.ShowDialog() == DialogResult.OK)
            {
                await webView.CoreWebView2.PrintToPdfAsync(saveDialog.FileName);
                MessageBox.Show("PDF saved successfully!", "M-Finlogs", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }
        catch (Exception ex)
        {
            MessageBox.Show($"Failed to save PDF: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    #endregion

    #region Theme Support

    private Color GetThemeTitleBarColor()
    {
        return Program.Config.Theme?.ToLower() switch
        {
            "dark" => Color.FromArgb(30, 30, 30),
            "deep blue" or "deepblue" => Color.FromArgb(15, 23, 42),
            "warm" => Color.FromArgb(69, 47, 36),
            _ => Color.FromArgb(249, 250, 251) // light
        };
    }

    private Color GetThemeTitleForeColor()
    {
        return Program.Config.Theme?.ToLower() switch
        {
            "dark" => Color.FromArgb(229, 231, 235),
            "deep blue" or "deepblue" => Color.FromArgb(203, 213, 225),
            "warm" => Color.FromArgb(245, 236, 228),
            _ => Color.FromArgb(17, 24, 39) // light
        };
    }

    private Color GetThemeBackColor()
    {
        return Program.Config.Theme?.ToLower() switch
        {
            "dark" => Color.FromArgb(17, 17, 17),
            "deep blue" or "deepblue" => Color.FromArgb(2, 6, 23),
            "warm" => Color.FromArgb(44, 30, 23),
            _ => Color.White // light
        };
    }

    /// <summary>
    /// Update theme from web app notification
    /// </summary>
    public void UpdateTheme(string theme)
    {
        Program.Config.Theme = theme;
        Program.Config.Save();

        this.Invoke(() =>
        {
            titleBar.BackColor = GetThemeTitleBarColor();
            titleLabel.ForeColor = GetThemeTitleForeColor();
            btnClose.ForeColor = GetThemeTitleForeColor();
            btnMaximize.ForeColor = GetThemeTitleForeColor();
            btnMinimize.ForeColor = GetThemeTitleForeColor();
            this.BackColor = GetThemeBackColor();
        });
    }

    #endregion

    #region Public Methods

    /// <summary>
    /// Switch between online and offline mode
    /// </summary>
    public void SwitchMode(string mode)
    {
        Program.Config.Mode = mode;
        Program.Config.Save();

        lblMode.Invoke(() =>
        {
            lblMode.Text = mode == "online" ? "● Online" : "● Offline";
            lblMode.ForeColor = mode == "online"
                ? Color.FromArgb(34, 197, 94)
                : Color.FromArgb(251, 146, 60);
        });

        // Reload to new URL
        webView?.CoreWebView2?.Navigate(Program.Config.ActiveUrl);
    }

    /// <summary>
    /// Navigate WebView to a specific URL
    /// </summary>
    public void NavigateTo(string url)
    {
        webView?.CoreWebView2?.Navigate(url);
    }

    #endregion

    // Override WndProc for window resize handles (borderless form)
    protected override void WndProc(ref Message m)
    {
        const int WM_NCHITTEST = 0x84;
        const int HTLEFT = 10;
        const int HTRIGHT = 11;
        const int HTTOP = 12;
        const int HTTOPLEFT = 13;
        const int HTTOPRIGHT = 14;
        const int HTBOTTOM = 15;
        const int HTBOTTOMLEFT = 16;
        const int HTBOTTOMRIGHT = 17;

        if (m.Msg == WM_NCHITTEST)
        {
            base.WndProc(ref m);

            var cursor = this.PointToClient(Cursor.Position);
            const int grip = 8;

            if (cursor.Y < grip)
            {
                if (cursor.X < grip) m.Result = (IntPtr)HTTOPLEFT;
                else if (cursor.X > this.Width - grip) m.Result = (IntPtr)HTTOPRIGHT;
                else m.Result = (IntPtr)HTTOP;
            }
            else if (cursor.Y > this.Height - grip)
            {
                if (cursor.X < grip) m.Result = (IntPtr)HTBOTTOMLEFT;
                else if (cursor.X > this.Width - grip) m.Result = (IntPtr)HTBOTTOMRIGHT;
                else m.Result = (IntPtr)HTBOTTOM;
            }
            else if (cursor.X < grip) m.Result = (IntPtr)HTLEFT;
            else if (cursor.X > this.Width - grip) m.Result = (IntPtr)HTRIGHT;

            return;
        }

        base.WndProc(ref m);
    }
}
