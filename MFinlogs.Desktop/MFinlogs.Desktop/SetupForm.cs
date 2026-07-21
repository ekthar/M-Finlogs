using System.Drawing.Drawing2D;
using System.Drawing.Text;
using MFinlogs.Desktop.Config;
using MFinlogs.Desktop.UI;
using MFinlogs.Desktop.UI.Controls;

namespace MFinlogs.Desktop;

/// <summary>
/// Premium onboarding / settings wizard.
/// Transforms the first-run experience into a delightful, modern flow.
/// Uses custom controls (ModernButton, ModernTextBox, ModernRadioCard, ModernCheckBox)
/// with animated transitions, live validation, and elegant visual hierarchy.
/// 
/// Preserves all functionality: URL input, mode selection, auto-start, minimize-to-tray.
/// </summary>
public class SetupForm : Form
{
    // ─── Controls ───
    private ModernTextBox _txtUrl = null!;
    private ModernRadioCard _cardOnline = null!;
    private ModernRadioCard _cardOffline = null!;
    private ModernRadioCard _cardHybrid = null!;
    private ModernCheckBox _chkAutoStart = null!;
    private ModernCheckBox _chkMinimizeToTray = null!;
    private ModernButton _btnStart = null!;
    private Label _lblTitle = null!;
    private Label _lblSubtitle = null!;
    private Label _lblModeHeader = null!;
    private Label _lblOptionsHeader = null!;
    private Panel _contentPanel = null!;
    private Label _btnClose = null!;

    // ─── State ───
    private readonly bool _isFirstRun;
    private float _fadeIn;
    private float _contentSlide;
    private System.Windows.Forms.Timer? _animTimer;

    public bool Completed { get; private set; }


    public SetupForm(bool isFirstRun = true)
    {
        _isFirstRun = isFirstRun;
        DesignTokens.Typography.Initialize();
        InitializeForm();
        BuildLayout();
        StartEntryAnimation();
    }

    private void InitializeForm()
    {
        Text = "M-Finlogs Setup";
        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.CenterScreen;
        Size = new Size(540, 620);
        BackColor = DesignTokens.Colors.Background;
        DoubleBuffered = true;
        ShowInTaskbar = true;
        TopMost = true;
        Opacity = 0;

        // Rounded corners
        Region = CreateFormRegion(540, 620, DesignTokens.Radius.XL);
        Paint += OnFormPaint;

        // Icon
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
            try { Icon = new Icon(iconPath); } catch { }

        // Allow dragging
        MouseDown += (s, e) => { if (e.Button == MouseButtons.Left) DragForm(); };
    }

    private void DragForm()
    {
        // Win32 form drag
        NativeDrag.ReleaseCapture();
        NativeDrag.SendMessage(Handle, 0xA1, 0x2, 0);
    }

    private static class NativeDrag
    {
        [System.Runtime.InteropServices.DllImport("user32.dll")]
        public static extern int SendMessage(IntPtr hWnd, int msg, int wParam, int lParam);
        [System.Runtime.InteropServices.DllImport("user32.dll")]
        public static extern bool ReleaseCapture();
    }


    private void BuildLayout()
    {
        // ─── Close Button (top-right) ───
        _btnClose = new Label
        {
            Text = "\u2715",
            Font = DesignTokens.Typography.Body,
            ForeColor = DesignTokens.Colors.TextTertiary,
            AutoSize = true,
            Location = new Point(504, 16),
            Cursor = Cursors.Hand
        };
        _btnClose.Click += (s, e) => HandleClose();
        _btnClose.MouseEnter += (s, e) => _btnClose.ForeColor = DesignTokens.Colors.Danger;
        _btnClose.MouseLeave += (s, e) => _btnClose.ForeColor = DesignTokens.Colors.TextTertiary;
        Controls.Add(_btnClose);

        // ─── Content Panel (scrollable area) ───
        _contentPanel = new Panel
        {
            Location = new Point(40, 44),
            Size = new Size(460, 540),
            BackColor = Color.Transparent,
            AutoScroll = false
        };

        int y = 0;

        // ─── Logo Mark ───
        var logoPanel = new Panel
        {
            Location = new Point(0, y),
            Size = new Size(40, 40),
            BackColor = Color.Transparent
        };
        logoPanel.Paint += (s, e) =>
        {
            var g = e.Graphics;
            ModernRenderer.SetHighQuality(g);
            var logoRect = new RectangleF(0, 0, 36, 36);
            ModernRenderer.FillRoundedRectGradient(g, logoRect, 10,
                DesignTokens.Colors.GradientPrimaryStart,
                DesignTokens.Colors.GradientPrimaryEnd,
                LinearGradientMode.ForwardDiagonal);
            ModernRenderer.DrawTextCentered(g, "M",
                DesignTokens.Typography.Title, Color.White, logoRect);
        };
        _contentPanel.Controls.Add(logoPanel);
        y += 52;

        // ─── Title ───
        _lblTitle = new Label
        {
            Text = _isFirstRun ? "Welcome to M-Finlogs" : "Settings",
            Font = DesignTokens.Typography.H2,
            ForeColor = DesignTokens.Colors.TextPrimary,
            AutoSize = true,
            Location = new Point(0, y)
        };
        _contentPanel.Controls.Add(_lblTitle);
        y += 36;

        // ─── Subtitle ───
        _lblSubtitle = new Label
        {
            Text = _isFirstRun
                ? "Configure your desktop experience in seconds."
                : "Adjust your M-Finlogs desktop configuration.",
            Font = DesignTokens.Typography.Body,
            ForeColor = DesignTokens.Colors.TextTertiary,
            AutoSize = true,
            Location = new Point(0, y)
        };
        _contentPanel.Controls.Add(_lblSubtitle);
        y += 36;


        // ─── URL Input ───
        _txtUrl = new ModernTextBox
        {
            Label = "Web App URL",
            Placeholder = "https://m-finlogs.vercel.app",
            Hint = "Your deployed M-Finlogs web application URL",
            Text = Program.Config.OnlineUrl,
            Location = new Point(0, y),
            Size = new Size(460, 68)
        };
        _contentPanel.Controls.Add(_txtUrl);
        y += 80;

        // ─── Mode Section ───
        _lblModeHeader = new Label
        {
            Text = "Operating Mode",
            Font = DesignTokens.Typography.Label,
            ForeColor = DesignTokens.Colors.TextSecondary,
            AutoSize = true,
            Location = new Point(0, y)
        };
        _contentPanel.Controls.Add(_lblModeHeader);
        y += 24;

        _cardOnline = new ModernRadioCard
        {
            Title = "Online",
            Description = "Use web app directly — data stored in cloud (recommended)",
            IconSymbol = "\u2601", // cloud
            IsSelected = Program.Config.IsOnlineMode || string.IsNullOrEmpty(Program.Config.Mode),
            Location = new Point(0, y),
            Size = new Size(460, 66)
        };
        _cardOnline.Selected += OnModeSelected;
        _contentPanel.Controls.Add(_cardOnline);
        y += 72;

        _cardOffline = new ModernRadioCard
        {
            Title = "Offline",
            Description = "Local database — works without internet connection",
            IconSymbol = "\u26A1", // lightning
            IsSelected = Program.Config.IsOfflineMode,
            Location = new Point(0, y),
            Size = new Size(460, 66)
        };
        _cardOffline.Selected += OnModeSelected;
        _contentPanel.Controls.Add(_cardOffline);
        y += 72;

        _cardHybrid = new ModernRadioCard
        {
            Title = "Hybrid",
            Description = "Web interface + local database for offline resilience",
            IconSymbol = "\u21C4", // arrows
            IsSelected = Program.Config.IsHybridMode,
            Location = new Point(0, y),
            Size = new Size(460, 66)
        };
        _cardHybrid.Selected += OnModeSelected;
        _contentPanel.Controls.Add(_cardHybrid);
        y += 84;


        // ─── Options Section ───
        _lblOptionsHeader = new Label
        {
            Text = "Preferences",
            Font = DesignTokens.Typography.Label,
            ForeColor = DesignTokens.Colors.TextSecondary,
            AutoSize = true,
            Location = new Point(0, y)
        };
        _contentPanel.Controls.Add(_lblOptionsHeader);
        y += 26;

        _chkAutoStart = new ModernCheckBox
        {
            Text = "Launch at Windows startup",
            Checked = Program.Config.AutoStart,
            Location = new Point(0, y),
            Size = new Size(460, 28)
        };
        _contentPanel.Controls.Add(_chkAutoStart);
        y += 34;

        _chkMinimizeToTray = new ModernCheckBox
        {
            Text = "Minimize to system tray when closed",
            Checked = Program.Config.MinimizeToTray,
            Location = new Point(0, y),
            Size = new Size(460, 28)
        };
        _contentPanel.Controls.Add(_chkMinimizeToTray);
        y += 48;

        // ─── Action Button ───
        _btnStart = new ModernButton
        {
            Text = _isFirstRun ? "Get Started" : "Save Settings",
            Variant = ModernButton.ButtonVariant.Primary,
            Size = new Size(460, 48),
            Location = new Point(0, y)
        };
        _btnStart.Click += OnStartClick;
        _contentPanel.Controls.Add(_btnStart);

        Controls.Add(_contentPanel);
        AcceptButton = null; // We handle Enter via KeyDown
        KeyPreview = true;
        KeyDown += (s, e) => { if (e.KeyCode == Keys.Enter) OnStartClick(this, EventArgs.Empty); };
    }

    private void OnModeSelected(object? sender, EventArgs e)
    {
        // Deselect others when one is selected (radio group behavior)
        if (sender == _cardOnline) { _cardOffline.IsSelected = false; _cardHybrid.IsSelected = false; }
        else if (sender == _cardOffline) { _cardOnline.IsSelected = false; _cardHybrid.IsSelected = false; }
        else if (sender == _cardHybrid) { _cardOnline.IsSelected = false; _cardOffline.IsSelected = false; }
    }


    // ═══════════════════════════════════════════════════════════════
    // ENTRY ANIMATION
    // ═══════════════════════════════════════════════════════════════

    private void StartEntryAnimation()
    {
        _animTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _animTimer.Tick += (s, e) =>
        {
            bool dirty = false;

            // Fade in
            if (_fadeIn < 1f)
            {
                _fadeIn += 0.05f;
                if (_fadeIn > 1f) _fadeIn = 1f;
                Opacity = _fadeIn;
                dirty = true;
            }

            // Content slide up
            if (_contentSlide < 1f)
            {
                _contentSlide += (1f - _contentSlide) * 0.08f;
                if (_contentSlide > 0.99f) _contentSlide = 1f;
                int offset = (int)((1f - _contentSlide) * 20);
                _contentPanel.Location = new Point(40, 44 + offset);
                dirty = true;
            }

            if (!dirty)
                _animTimer?.Stop();
        };
        _animTimer.Start();
    }

    // ═══════════════════════════════════════════════════════════════
    // VALIDATION & SAVE
    // ═══════════════════════════════════════════════════════════════

    private void OnStartClick(object? sender, EventArgs e)
    {
        var url = _txtUrl.Text.Trim();

        // Validate URL
        if (string.IsNullOrWhiteSpace(url))
        {
            _txtUrl.ErrorText = "Please enter a URL to continue.";
            _txtUrl.ShakeError();
            return;
        }

        if (!url.StartsWith("http://") && !url.StartsWith("https://"))
        {
            url = "https://" + url;
            _txtUrl.Text = url;
        }

        if (!Uri.TryCreate(url, UriKind.Absolute, out _))
        {
            _txtUrl.ErrorText = "Please enter a valid URL.";
            _txtUrl.ShakeError();
            return;
        }

        _txtUrl.ClearError();

        // Show loading state
        _btnStart.IsLoading = true;

        // Save config
        var config = Program.Config;
        config.OnlineUrl = url.TrimEnd('/');
        config.Mode = _cardOnline.IsSelected ? "online"
            : _cardOffline.IsSelected ? "offline" : "hybrid";
        config.AutoStart = _chkAutoStart.Checked;
        config.MinimizeToTray = _chkMinimizeToTray.Checked;
        config.Save();

        // Handle auto-start registry
        AutoUpdater.SetAutoStart(config.AutoStart);

        // Brief delay for visual feedback, then close
        var closeTimer = new System.Windows.Forms.Timer { Interval = 400 };
        closeTimer.Tick += (s2, e2) =>
        {
            closeTimer.Stop();
            closeTimer.Dispose();
            Completed = true;
            FadeOutAndClose();
        };
        closeTimer.Start();
    }

    private void FadeOutAndClose()
    {
        var fadeOut = new System.Windows.Forms.Timer { Interval = 16 };
        fadeOut.Tick += (s, e) =>
        {
            _fadeIn -= 0.06f;
            if (_fadeIn <= 0)
            {
                fadeOut.Stop();
                fadeOut.Dispose();
                Close();
            }
            else
            {
                Opacity = _fadeIn;
            }
        };
        fadeOut.Start();
    }

    private void HandleClose()
    {
        if (_isFirstRun)
        {
            _txtUrl.ErrorText = "Please enter a URL to continue.";
            _txtUrl.ShakeError();
        }
        else
        {
            FadeOutAndClose();
        }
    }


    // ═══════════════════════════════════════════════════════════════
    // CUSTOM PAINTING — Accent bar, border, shadow
    // ═══════════════════════════════════════════════════════════════

    private void OnFormPaint(object? sender, PaintEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        // Top accent gradient bar (3px)
        using var accentBrush = new LinearGradientBrush(
            new Rectangle(0, 0, Width, 3),
            DesignTokens.Colors.GradientPrimaryStart,
            DesignTokens.Colors.GradientAuroraMid,
            LinearGradientMode.Horizontal);
        g.FillRectangle(accentBrush, 0, 0, Width, 3);

        // Border
        var borderRect = new RectangleF(0, 0, Width - 1, Height - 1);
        ModernRenderer.DrawRoundedRect(g, borderRect, DesignTokens.Radius.XL,
            DesignTokens.Colors.Stroke);
    }

    // ═══════════════════════════════════════════════════════════════
    // FORM HELPERS
    // ═══════════════════════════════════════════════════════════════

    private Region CreateFormRegion(int w, int h, int radius)
    {
        using var path = ModernRenderer.CreateRoundedRect(
            new RectangleF(0, 0, w, h), radius);
        return new Region(path);
    }

    // Drop shadow via class style
    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ClassStyle |= 0x00020000; // CS_DROPSHADOW
            return cp;
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _animTimer?.Stop();
            _animTimer?.Dispose();
        }
        base.Dispose(disposing);
    }
}
