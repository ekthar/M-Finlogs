using System.Drawing.Drawing2D;
using System.Drawing.Text;

namespace MFinlogs.Desktop;

/// <summary>
/// Fullscreen animated splash screen with accounting-themed visuals.
/// - Fills entire primary monitor
/// - Animated logo with circular progress arc
/// - Floating accounting symbols (₹, $, %, ledger lines)
/// - Spring physics on logo, fade-in content, typing effect on subtitle
/// - Smooth fade-out transition when ready
/// </summary>
public class SplashForm : Form
{
    // Animation state
    private float progress = 0f;
    private float logoScale = 0f;
    private float contentOpacity = 0f;
    private float arcAngle = 0f;
    private float fadeOpacity = 1f;
    private bool isFadingOut = false;
    private int typingIndex = 0;
    private float particlePhase = 0f;

    // Status
    private string statusText = "Starting";
    private string subtitle = "Financial Accounting";

    // Particles (accounting symbols floating in background)
    private readonly List<Particle> particles = new();
    private readonly Random rng = new();

    // Timers
    private System.Windows.Forms.Timer animTimer = null!;
    private System.Windows.Forms.Timer fadeTimer = null!;
    private System.Windows.Forms.Timer typingTimer = null!;

    // Fonts
    private Font? titleFont;
    private Font? subtitleFont;
    private Font? statusFont;
    private Font? versionFont;
    private Font? logoFont;
    private Font? particleFont;

    // Colors
    private static readonly Color BgTop = Color.FromArgb(8, 8, 16);
    private static readonly Color BgBottom = Color.FromArgb(16, 16, 32);
    private static readonly Color AccentIndigo = Color.FromArgb(99, 102, 241);
    private static readonly Color AccentViolet = Color.FromArgb(139, 92, 246);
    private static readonly Color TextWhite = Color.FromArgb(250, 250, 255);
    private static readonly Color TextMuted = Color.FromArgb(100, 116, 139);
    private static readonly Color TextDim = Color.FromArgb(40, 50, 70);

    public SplashForm()
    {
        LoadFonts();
        InitializeForm();
        GenerateParticles();
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
                var fam = pfc.Families[0];
                titleFont = new Font(fam, 42f, FontStyle.Bold);
                subtitleFont = new Font(pfc.Families.Length > 1 ? pfc.Families[1] : fam, 14f);
                statusFont = new Font(pfc.Families.Length > 1 ? pfc.Families[1] : fam, 10f);
                versionFont = new Font(pfc.Families.Length > 1 ? pfc.Families[1] : fam, 9f);
                logoFont = new Font(fam, 38f, FontStyle.Bold);
                particleFont = new Font(pfc.Families.Length > 1 ? pfc.Families[1] : fam, 14f);
                return;
            }
        }
        catch { }
        titleFont = new Font("Segoe UI", 42f, FontStyle.Bold);
        subtitleFont = new Font("Segoe UI", 14f);
        statusFont = new Font("Segoe UI", 10f);
        versionFont = new Font("Segoe UI", 9f);
        logoFont = new Font("Segoe UI", 38f, FontStyle.Bold);
        particleFont = new Font("Segoe UI", 14f);
    }

    private void InitializeForm()
    {
        this.FormBorderStyle = FormBorderStyle.None;
        this.WindowState = FormWindowState.Maximized;
        this.StartPosition = FormStartPosition.Manual;
        this.Location = Point.Empty;
        this.Size = Screen.PrimaryScreen!.Bounds.Size;
        this.BackColor = BgTop;
        this.DoubleBuffered = true;
        this.ShowInTaskbar = false;
        this.TopMost = true;

        // Set icon
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
            try { this.Icon = new Icon(iconPath); } catch { }
    }

    private void GenerateParticles()
    {
        string[] symbols = { "₹", "$", "%", "∑", "△", "◇", "≡", "⊞", "◈", "⟐" };
        for (int i = 0; i < 35; i++)
        {
            particles.Add(new Particle
            {
                Symbol = symbols[rng.Next(symbols.Length)],
                X = rng.Next(this.Width),
                Y = rng.Next(this.Height),
                Speed = 0.3f + (float)rng.NextDouble() * 0.7f,
                Opacity = 0.03f + (float)rng.NextDouble() * 0.06f,
                Size = 12f + (float)rng.NextDouble() * 10f,
                Drift = (float)(rng.NextDouble() - 0.5) * 0.4f
            });
        }
    }

    private void StartAnimations()
    {
        // Main animation loop (~60fps)
        animTimer = new System.Windows.Forms.Timer { Interval = 16 };
        animTimer.Tick += (s, e) =>
        {
            // Logo spring in
            logoScale += (1f - logoScale) * 0.06f;

            // Content fade in (delayed)
            if (logoScale > 0.8f && contentOpacity < 1f)
                contentOpacity = Math.Min(1f, contentOpacity + 0.025f);

            // Progress arc rotation
            arcAngle += 3f;
            if (arcAngle >= 360f) arcAngle -= 360f;

            // Progress fill (ease out)
            if (progress < 0.85f && !isFadingOut)
                progress += 0.003f;

            // Particle drift
            particlePhase += 0.01f;
            foreach (var p in particles)
            {
                p.Y -= p.Speed;
                p.X += p.Drift + (float)Math.Sin(particlePhase + p.X * 0.01) * 0.2f;
                if (p.Y < -30) { p.Y = this.Height + 30; p.X = rng.Next(this.Width); }
                if (p.X < -30) p.X = this.Width + 30;
                if (p.X > this.Width + 30) p.X = -30;
            }

            this.Invalidate();
        };
        animTimer.Start();

        // Typing effect for subtitle
        typingTimer = new System.Windows.Forms.Timer { Interval = 60 };
        typingTimer.Tick += (s, e) =>
        {
            if (typingIndex < subtitle.Length)
                typingIndex++;
            else
                typingTimer.Stop();
        };
        // Start typing after a short delay
        Task.Delay(800).ContinueWith(_ => this.Invoke(() => typingTimer.Start()));
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.HighQuality;
        g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;

        int cx = this.Width / 2;
        int cy = this.Height / 2 - 40;

        // === BACKGROUND ===
        using var bgBrush = new LinearGradientBrush(
            this.ClientRectangle, BgTop, BgBottom, LinearGradientMode.Vertical);
        g.FillRectangle(bgBrush, this.ClientRectangle);

        // Ambient glow orbs
        DrawGlow(g, cx - 300, cy - 200, 500, AccentIndigo, 8);
        DrawGlow(g, cx + 200, cy + 150, 400, AccentViolet, 6);

        // === PARTICLES (floating accounting symbols) ===
        foreach (var p in particles)
        {
            using var pBrush = new SolidBrush(Color.FromArgb((int)(p.Opacity * 255), TextMuted));
            using var pFont = new Font(particleFont!.FontFamily, p.Size);
            g.DrawString(p.Symbol, pFont, pBrush, p.X, p.Y);
        }

        // === LOGO CIRCLE ===
        int logoSize = (int)(100 * logoScale);
        int logoX = cx - logoSize / 2;
        int logoY = cy - 100 - logoSize / 2;

        if (logoSize > 2)
        {
            // Outer glow
            using var glowBrush = new SolidBrush(Color.FromArgb((int)(20 * logoScale), AccentIndigo));
            g.FillEllipse(glowBrush, logoX - 12, logoY - 12, logoSize + 24, logoSize + 24);

            // Gradient circle
            using var logoBrush = new LinearGradientBrush(
                new Rectangle(logoX, logoY, logoSize, logoSize),
                AccentIndigo, AccentViolet, LinearGradientMode.ForwardDiagonal);
            g.FillEllipse(logoBrush, logoX, logoY, logoSize, logoSize);

            // "M" letter
            using var mBrush = new SolidBrush(Color.FromArgb((int)(255 * logoScale), 255, 255, 255));
            var sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
            using var mFont = new Font(logoFont!.FontFamily, logoSize * 0.36f, FontStyle.Bold);
            g.DrawString("M", mFont, mBrush, new RectangleF(logoX, logoY, logoSize, logoSize), sf);

            // === PROGRESS ARC (around logo) ===
            int arcSize = logoSize + 28;
            int arcX = cx - arcSize / 2;
            int arcY = cy - 100 - arcSize / 2;
            float sweepAngle = progress * 360f;

            using var arcPen = new Pen(Color.FromArgb(30, 255, 255, 255), 2f);
            g.DrawEllipse(arcPen, arcX, arcY, arcSize, arcSize);

            using var progressPen = new Pen(AccentIndigo, 2.5f)
            {
                StartCap = LineCap.Round,
                EndCap = LineCap.Round
            };
            if (sweepAngle > 1)
                g.DrawArc(progressPen, arcX, arcY, arcSize, arcSize, arcAngle, sweepAngle);
        }

        // === TITLE ===
        int alpha = (int)(255 * contentOpacity);
        using var titleBrush = new SolidBrush(Color.FromArgb(alpha, TextWhite));
        var titleSf = new StringFormat { Alignment = StringAlignment.Center };
        g.DrawString("M-Finlogs", titleFont!, titleBrush, cx, cy + 10, titleSf);

        // === SUBTITLE (typing effect) ===
        string visibleSubtitle = subtitle.Substring(0, Math.Min(typingIndex, subtitle.Length));
        using var subBrush = new SolidBrush(Color.FromArgb((int)(alpha * 0.7f), TextMuted));
        g.DrawString(visibleSubtitle, subtitleFont!, subBrush, cx, cy + 65, titleSf);

        // === STATUS ===
        int dots = ((int)(arcAngle / 30f)) % 4;
        string statusDisplay = statusText + new string('.', dots);
        using var statusBrush = new SolidBrush(Color.FromArgb((int)(alpha * 0.5f), TextMuted));
        g.DrawString(statusDisplay, statusFont!, statusBrush, cx, cy + 120, titleSf);

        // === VERSION (bottom center) ===
        string version = $"v{typeof(SplashForm).Assembly.GetName().Version?.ToString(3) ?? "1.0.0"}";
        using var verBrush = new SolidBrush(Color.FromArgb(40, 255, 255, 255));
        g.DrawString(version, versionFont!, verBrush, cx, this.Height - 40, titleSf);

        // === FADE OVERLAY ===
        if (isFadingOut && fadeOpacity < 1f)
        {
            // no overlay needed — we're reducing form opacity instead
        }
    }

    private void DrawGlow(Graphics g, int x, int y, int size, Color color, int alpha)
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

    /// <summary>
    /// Update the status text
    /// </summary>
    public void UpdateStatus(string text)
    {
        if (this.InvokeRequired)
            this.Invoke(() => statusText = text);
        else
            statusText = text;
    }

    /// <summary>
    /// Set progress value (0.0 to 1.0)
    /// </summary>
    public void SetProgress(float value)
    {
        progress = Math.Clamp(value, 0f, 1f);
    }

    /// <summary>
    /// Smoothly fade out and close
    /// </summary>
    public void FadeOutAndClose()
    {
        isFadingOut = true;
        progress = 1f;

        fadeTimer = new System.Windows.Forms.Timer { Interval = 16 };
        fadeTimer.Tick += (s, e) =>
        {
            fadeOpacity -= 0.05f;
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

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        animTimer?.Stop();
        animTimer?.Dispose();
        typingTimer?.Stop();
        typingTimer?.Dispose();
        fadeTimer?.Stop();
        fadeTimer?.Dispose();
        base.OnFormClosing(e);
    }

    /// <summary>
    /// Floating particle data
    /// </summary>
    private class Particle
    {
        public string Symbol { get; set; } = "";
        public float X { get; set; }
        public float Y { get; set; }
        public float Speed { get; set; }
        public float Opacity { get; set; }
        public float Size { get; set; }
        public float Drift { get; set; }
    }
}
