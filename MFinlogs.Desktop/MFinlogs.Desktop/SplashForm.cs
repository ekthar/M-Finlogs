using System.Drawing.Drawing2D;
using System.Drawing.Text;

namespace MFinlogs.Desktop;

/// <summary>
/// Professional splash screen with smooth animations, glass morphism effect,
/// animated progress indicator, and fade-out transition.
/// Inspired by modern app launchers (Figma, Linear, Arc Browser).
/// </summary>
public class SplashForm : Form
{
    private Label lblTitle;
    private Label lblSubtitle;
    private Label lblStatus;
    private Label lblVersion;
    private Panel logoPanel;
    private System.Windows.Forms.Timer animTimer;
    private System.Windows.Forms.Timer fadeTimer;

    // Animation state
    private float progress = 0f;
    private float logoScale = 0.8f;
    private float logoTargetScale = 1.0f;
    private float contentOpacity = 0f;
    private float dotPhase = 0f;
    private bool isFadingOut = false;
    private float fadeOpacity = 1.0f;

    // Colors
    private static readonly Color BgDark = Color.FromArgb(10, 10, 14);
    private static readonly Color BgGradientEnd = Color.FromArgb(18, 18, 28);
    private static readonly Color AccentBlue = Color.FromArgb(99, 102, 241);
    private static readonly Color AccentViolet = Color.FromArgb(139, 92, 246);
    private static readonly Color TextPrimary = Color.FromArgb(250, 250, 255);
    private static readonly Color TextSecondary = Color.FromArgb(148, 163, 184);
    private static readonly Color TextMuted = Color.FromArgb(71, 85, 105);

    // Font
    private Font? titleFont;
    private Font? subtitleFont;
    private Font? statusFont;
    private Font? versionFont;

    public SplashForm()
    {
        LoadFonts();
        InitializeComponents();
        StartAnimations();
    }

    private void LoadFonts()
    {
        try
        {
            var pfc = new PrivateFontCollection();
            var bold = Path.Combine(AppContext.BaseDirectory, "Assets", "InterTight-Bold.ttf");
            var regular = Path.Combine(AppContext.BaseDirectory, "Assets", "InterTight-Regular.ttf");
            if (File.Exists(bold)) pfc.AddFontFile(bold);
            if (File.Exists(regular)) pfc.AddFontFile(regular);
            if (pfc.Families.Length > 0)
            {
                titleFont = new Font(pfc.Families[0], 32f, FontStyle.Bold);
                subtitleFont = new Font(pfc.Families.Length > 1 ? pfc.Families[1] : pfc.Families[0], 11f, FontStyle.Regular);
                statusFont = new Font(pfc.Families.Length > 1 ? pfc.Families[1] : pfc.Families[0], 9f, FontStyle.Regular);
                versionFont = new Font(pfc.Families.Length > 1 ? pfc.Families[1] : pfc.Families[0], 8f, FontStyle.Regular);
                return;
            }
        }
        catch { }

        titleFont = new Font("Segoe UI", 32f, FontStyle.Bold);
        subtitleFont = new Font("Segoe UI", 11f, FontStyle.Regular);
        statusFont = new Font("Segoe UI", 9f, FontStyle.Regular);
        versionFont = new Font("Segoe UI", 8f, FontStyle.Regular);
    }

    private void InitializeComponents()
    {
        this.SuspendLayout();

        this.FormBorderStyle = FormBorderStyle.None;
        this.StartPosition = FormStartPosition.CenterScreen;
        this.Size = new Size(520, 340);
        this.ShowInTaskbar = false;
        this.TopMost = true;
        this.BackColor = BgDark;
        this.DoubleBuffered = true;

        // Rounded corners (Windows 11 style, 12px radius)
        this.Region = CreateRoundedRegion(520, 340, 12);

        // Custom paint for background effects
        this.Paint += SplashForm_Paint;

        // Logo area (custom painted circle with icon)
        logoPanel = new Panel
        {
            Size = new Size(64, 64),
            Location = new Point((520 - 64) / 2, 60),
            BackColor = Color.Transparent
        };
        logoPanel.Paint += LogoPanel_Paint;

        // Title
        lblTitle = new Label
        {
            Text = "M-Finlogs",
            Font = titleFont,
            ForeColor = TextPrimary,
            AutoSize = false,
            Size = new Size(520, 48),
            Location = new Point(0, 135),
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.Transparent
        };

        // Subtitle
        lblSubtitle = new Label
        {
            Text = "Financial Accounting",
            Font = subtitleFont,
            ForeColor = TextSecondary,
            AutoSize = false,
            Size = new Size(520, 22),
            Location = new Point(0, 185),
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.Transparent
        };

        // Status (with animated dots)
        lblStatus = new Label
        {
            Text = "Starting",
            Font = statusFont,
            ForeColor = TextMuted,
            AutoSize = false,
            Size = new Size(520, 18),
            Location = new Point(0, 270),
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.Transparent
        };

        // Version
        lblVersion = new Label
        {
            Text = $"v{typeof(SplashForm).Assembly.GetName().Version?.ToString(3) ?? "1.0.0"}",
            Font = versionFont,
            ForeColor = Color.FromArgb(50, 255, 255, 255),
            AutoSize = false,
            Size = new Size(520, 14),
            Location = new Point(0, 310),
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.Transparent
        };

        this.Controls.Add(logoPanel);
        this.Controls.Add(lblTitle);
        this.Controls.Add(lblSubtitle);
        this.Controls.Add(lblStatus);
        this.Controls.Add(lblVersion);

        this.ResumeLayout(false);
    }

    private void SplashForm_Paint(object? sender, PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.HighQuality;

        // Background gradient
        using var bgBrush = new LinearGradientBrush(
            this.ClientRectangle, BgDark, BgGradientEnd,
            LinearGradientMode.ForwardDiagonal);
        g.FillRectangle(bgBrush, this.ClientRectangle);

        // Glow orbs (ambient light effect)
        DrawGlowOrb(g, -40, -40, 220, AccentBlue, 12);
        DrawGlowOrb(g, 380, 220, 180, AccentViolet, 8);
        DrawGlowOrb(g, 200, 300, 140, AccentBlue, 6);

        // Progress track (bottom, thin line)
        int trackY = 248;
        int trackX = 100;
        int trackW = 320;
        using var trackBrush = new SolidBrush(Color.FromArgb(20, 255, 255, 255));
        g.FillRectangle(trackBrush, trackX, trackY, trackW, 2);

        // Progress fill (animated gradient)
        if (progress > 0)
        {
            int fillW = (int)(trackW * Math.Min(progress, 1.0f));
            using var progressBrush = new LinearGradientBrush(
                new Rectangle(trackX, trackY, Math.Max(fillW, 1), 2),
                AccentBlue, AccentViolet,
                LinearGradientMode.Horizontal);
            g.FillRectangle(progressBrush, trackX, trackY, fillW, 2);

            // Glow at the tip
            if (fillW > 4)
            {
                using var tipBrush = new SolidBrush(Color.FromArgb(60, 139, 92, 246));
                g.FillEllipse(tipBrush, trackX + fillW - 4, trackY - 3, 8, 8);
            }
        }

        // Subtle border (1px)
        using var borderPen = new Pen(Color.FromArgb(15, 255, 255, 255), 1f);
        var borderRect = new Rectangle(0, 0, this.Width - 1, this.Height - 1);
        using var borderPath = CreateRoundedPath(borderRect, 12);
        g.DrawPath(borderPen, borderPath);
    }

    private void LogoPanel_Paint(object? sender, PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.HighQuality;

        int size = (int)(56 * logoScale);
        int offset = (64 - size) / 2;

        // Outer glow
        using var glowBrush = new SolidBrush(Color.FromArgb((int)(20 * contentOpacity), 99, 102, 241));
        g.FillEllipse(glowBrush, offset - 4, offset - 4, size + 8, size + 8);

        // Circle background with gradient
        using var circleBrush = new LinearGradientBrush(
            new Rectangle(offset, offset, size, size),
            AccentBlue, AccentViolet,
            LinearGradientMode.ForwardDiagonal);
        g.FillEllipse(circleBrush, offset, offset, size, size);

        // "M" letter in the center
        using var textBrush = new SolidBrush(Color.FromArgb((int)(255 * contentOpacity), 255, 255, 255));
        using var textFont = new Font("Segoe UI", size * 0.38f, FontStyle.Bold);
        var sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
        g.DrawString("M", textFont, textBrush, new RectangleF(offset, offset, size, size), sf);
    }

    private void DrawGlowOrb(Graphics g, int x, int y, int size, Color color, int alpha)
    {
        using var path = new GraphicsPath();
        path.AddEllipse(x, y, size, size);
        using var brush = new PathGradientBrush(path)
        {
            CenterColor = Color.FromArgb(alpha, color),
            SurroundColors = new[] { Color.FromArgb(0, color) }
        };
        g.FillEllipse(brush, x, y, size, size);
    }

    private void StartAnimations()
    {
        animTimer = new System.Windows.Forms.Timer { Interval = 16 }; // ~60fps
        animTimer.Tick += (s, e) =>
        {
            // Logo spring animation
            logoScale += (logoTargetScale - logoScale) * 0.08f;

            // Content fade in
            if (contentOpacity < 1f)
                contentOpacity = Math.Min(1f, contentOpacity + 0.04f);

            // Progress animation (ease)
            if (progress < 0.85f)
                progress += 0.005f;

            // Dot animation phase
            dotPhase += 0.05f;

            // Update status dots
            int dots = ((int)(dotPhase * 2)) % 4;
            var baseStatus = lblStatus.Text.TrimEnd('.', ' ');
            if (!isFadingOut)
            {
                lblStatus.Text = baseStatus + new string('.', dots);
            }

            // Repaint
            logoPanel.Invalidate();
            this.Invalidate();
        };
        animTimer.Start();
    }

    /// <summary>
    /// Update status text displayed on splash screen
    /// </summary>
    public void UpdateStatus(string status)
    {
        if (this.InvokeRequired)
            this.Invoke(() => UpdateStatus(status));
        else
            lblStatus.Text = status;
    }

    /// <summary>
    /// Set progress (0.0 to 1.0)
    /// </summary>
    public void SetProgress(float value)
    {
        progress = Math.Clamp(value, 0f, 1f);
    }

    /// <summary>
    /// Fade out and close the splash screen smoothly
    /// </summary>
    public void FadeOutAndClose()
    {
        isFadingOut = true;
        progress = 1.0f;
        this.Invalidate();

        fadeTimer = new System.Windows.Forms.Timer { Interval = 16 };
        fadeTimer.Tick += (s, e) =>
        {
            fadeOpacity -= 0.06f;
            this.Opacity = Math.Max(0, fadeOpacity);

            if (fadeOpacity <= 0)
            {
                fadeTimer.Stop();
                fadeTimer.Dispose();
                this.Close();
            }
        };
        fadeTimer.Start();
    }

    private Region CreateRoundedRegion(int w, int h, int r)
    {
        using var path = CreateRoundedPath(new Rectangle(0, 0, w, h), r);
        return new Region(path);
    }

    private static GraphicsPath CreateRoundedPath(Rectangle rect, int radius)
    {
        var path = new GraphicsPath();
        int d = radius * 2;
        path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        path.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        path.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        path.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        path.CloseFigure();
        return path;
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        animTimer?.Stop();
        animTimer?.Dispose();
        fadeTimer?.Stop();
        fadeTimer?.Dispose();
        base.OnFormClosing(e);
    }

    // Drop shadow effect (Windows 11)
    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ClassStyle |= 0x00020000; // CS_DROPSHADOW
            return cp;
        }
    }
}
