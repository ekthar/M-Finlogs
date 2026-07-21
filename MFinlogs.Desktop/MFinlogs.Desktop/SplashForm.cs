using System.Drawing.Drawing2D;
using System.Drawing.Text;
using MFinlogs.Desktop.UI;

namespace MFinlogs.Desktop;

/// <summary>
/// Premium fullscreen animated splash screen.
/// Visual language: Aurora mesh gradients, glass morphism, layered depth,
/// cinematic logo reveal, spring physics, smooth progress, particle field.
/// 
/// Inspired by: Linear splash, Arc Browser intro, Apple boot, Raycast launch.
/// 
/// Preserves all existing API: UpdateStatus(), SetProgress(), FadeOutAndClose()
/// </summary>
public class SplashForm : Form
{
    // ═══ Animation State ═══
    private float _progress;
    private float _logoScale;
    private float _logoGlow;
    private float _contentOpacity;
    private float _arcRotation;
    private float _fadeOpacity = 1f;
    private float _meshPhase;
    private float _particlePhase;
    private float _progressBarWidth;
    private float _statusOpacity;
    private float _ringPulse;
    private bool _isFadingOut;
    private int _typingIndex;

    // ═══ Content ═══
    private string _statusText = "Starting";
    private readonly string _subtitle = "Financial Intelligence";
    private readonly string _version;

    // ═══ Particles ═══
    private readonly List<Particle> _particles = new();
    private readonly Random _rng = new();

    // ═══ Aurora orbs (mesh gradient simulation) ═══
    private readonly List<AuroraOrb> _orbs = new();

    // ═══ Timers ═══
    private System.Windows.Forms.Timer _animTimer = null!;
    private System.Windows.Forms.Timer? _fadeTimer;
    private System.Windows.Forms.Timer? _typingTimer;

    // ═══ Fonts ═══
    private Font? _titleFont;
    private Font? _subtitleFont;
    private Font? _statusFont;
    private Font? _versionFont;
    private Font? _logoFont;
    private Font? _particleFont;


    public SplashForm()
    {
        _version = $"v{typeof(SplashForm).Assembly.GetName().Version?.ToString(3) ?? "1.0.0"}";
        DesignTokens.Typography.Initialize();
        LoadFonts();
        InitializeForm();
        GenerateAuroraOrbs();
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
                var boldFam = pfc.Families[0];
                var regFam = pfc.Families.Length > 1 ? pfc.Families[1] : boldFam;
                _titleFont = new Font(boldFam, 48f, FontStyle.Bold);
                _subtitleFont = new Font(regFam, 13f);
                _statusFont = new Font(regFam, 10.5f);
                _versionFont = new Font(regFam, 9f);
                _logoFont = new Font(boldFam, 40f, FontStyle.Bold);
                _particleFont = new Font(regFam, 13f);
                return;
            }
        }
        catch { }
        _titleFont = new Font("Segoe UI", 48f, FontStyle.Bold);
        _subtitleFont = new Font("Segoe UI", 13f);
        _statusFont = new Font("Segoe UI", 10.5f);
        _versionFont = new Font("Segoe UI", 9f);
        _logoFont = new Font("Segoe UI", 40f, FontStyle.Bold);
        _particleFont = new Font("Segoe UI", 13f);
    }

    private void InitializeForm()
    {
        FormBorderStyle = FormBorderStyle.None;
        WindowState = FormWindowState.Maximized;
        StartPosition = FormStartPosition.Manual;
        Location = Point.Empty;
        Size = Screen.PrimaryScreen!.Bounds.Size;
        BackColor = Color.FromArgb(6, 7, 14);
        DoubleBuffered = true;
        ShowInTaskbar = false;
        TopMost = true;

        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
            try { Icon = new Icon(iconPath); } catch { }
    }


    private void GenerateAuroraOrbs()
    {
        // Large ambient light orbs that drift slowly — creates aurora mesh effect
        _orbs.Add(new AuroraOrb
        {
            X = Width * 0.3f, Y = Height * 0.3f,
            Radius = Width * 0.35f,
            Color = DesignTokens.Colors.GradientAuroraStart,
            Alpha = 10, SpeedX = 0.15f, SpeedY = 0.08f, Phase = 0f
        });
        _orbs.Add(new AuroraOrb
        {
            X = Width * 0.7f, Y = Height * 0.5f,
            Radius = Width * 0.3f,
            Color = DesignTokens.Colors.GradientAuroraMid,
            Alpha = 8, SpeedX = -0.1f, SpeedY = 0.12f, Phase = 1.2f
        });
        _orbs.Add(new AuroraOrb
        {
            X = Width * 0.5f, Y = Height * 0.7f,
            Radius = Width * 0.25f,
            Color = DesignTokens.Colors.GradientAuroraEnd,
            Alpha = 6, SpeedX = 0.08f, SpeedY = -0.06f, Phase = 2.5f
        });
        _orbs.Add(new AuroraOrb
        {
            X = Width * 0.2f, Y = Height * 0.8f,
            Radius = Width * 0.2f,
            Color = DesignTokens.Colors.GradientCoolStart,
            Alpha = 5, SpeedX = 0.12f, SpeedY = 0.05f, Phase = 3.8f
        });
    }

    private void GenerateParticles()
    {
        string[] symbols = { "₹", "$", "%", "∑", "△", "◇", "≡", "⊞", "◈", "⟐", "∞", "◎" };
        int count = DesignTokens.Effects.ParticleCount;
        for (int i = 0; i < count; i++)
        {
            _particles.Add(new Particle
            {
                Symbol = symbols[_rng.Next(symbols.Length)],
                X = _rng.Next(Width),
                Y = _rng.Next(Height),
                Speed = DesignTokens.Effects.ParticleMinSpeed +
                    (float)_rng.NextDouble() * (DesignTokens.Effects.ParticleMaxSpeed - DesignTokens.Effects.ParticleMinSpeed),
                Opacity = DesignTokens.Effects.ParticleMinOpacity +
                    (float)_rng.NextDouble() * (DesignTokens.Effects.ParticleMaxOpacity - DesignTokens.Effects.ParticleMinOpacity),
                Size = DesignTokens.Effects.ParticleMinSize +
                    (float)_rng.NextDouble() * (DesignTokens.Effects.ParticleMaxSize - DesignTokens.Effects.ParticleMinSize),
                Drift = (float)(_rng.NextDouble() - 0.5) * 0.3f,
                RotationSpeed = (float)(_rng.NextDouble() - 0.5) * 0.5f
            });
        }
    }


    private void StartAnimations()
    {
        // Main animation loop (~60fps)
        _animTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _animTimer.Tick += (s, e) =>
        {
            // Logo spring reveal (ease out back)
            _logoScale += (1f - _logoScale) * 0.045f;

            // Logo glow pulse
            _logoGlow = 0.5f + MathF.Sin(_meshPhase * 2f) * 0.3f;

            // Content fade in (delayed after logo)
            if (_logoScale > 0.85f && _contentOpacity < 1f)
                _contentOpacity = Math.Min(1f, _contentOpacity + 0.018f);

            // Status text fade
            _statusOpacity += (1f - _statusOpacity) * 0.08f;

            // Progress arc rotation (smooth constant)
            _arcRotation += 2.2f;
            if (_arcRotation >= 360f) _arcRotation -= 360f;

            // Ring pulse
            _ringPulse += 0.04f;
            if (_ringPulse >= MathF.PI * 2) _ringPulse -= MathF.PI * 2;

            // Progress bar smooth interpolation
            _progressBarWidth += (_progress - _progressBarWidth) * 0.06f;

            // Aurora mesh drift
            _meshPhase += 0.008f;
            foreach (var orb in _orbs)
            {
                orb.X += orb.SpeedX + MathF.Sin(_meshPhase + orb.Phase) * 0.3f;
                orb.Y += orb.SpeedY + MathF.Cos(_meshPhase * 0.7f + orb.Phase) * 0.2f;
                // Gentle boundary wrap
                if (orb.X < -orb.Radius) orb.X = Width + orb.Radius * 0.5f;
                if (orb.X > Width + orb.Radius) orb.X = -orb.Radius * 0.5f;
                if (orb.Y < -orb.Radius) orb.Y = Height + orb.Radius * 0.5f;
                if (orb.Y > Height + orb.Radius) orb.Y = -orb.Radius * 0.5f;
            }

            // Particle drift
            _particlePhase += 0.008f;
            foreach (var p in _particles)
            {
                p.Y -= p.Speed;
                p.X += p.Drift + MathF.Sin(_particlePhase + p.X * 0.005f) * 0.15f;
                if (p.Y < -40) { p.Y = Height + 40; p.X = _rng.Next(Width); }
                if (p.X < -40) p.X = Width + 40;
                if (p.X > Width + 40) p.X = -40;
            }

            // Progress (slow auto-advance when not externally driven)
            if (_progress < 0.75f && !_isFadingOut)
                _progress += 0.0015f;

            Invalidate();
        };
        _animTimer.Start();

        // Typing effect for subtitle
        _typingTimer = new System.Windows.Forms.Timer { Interval = 55 };
        _typingTimer.Tick += (s, e) =>
        {
            if (_typingIndex < _subtitle.Length)
                _typingIndex++;
            else
                _typingTimer.Stop();
        };
        // Delayed start
        Task.Delay(900).ContinueWith(_ => this.Invoke(() => _typingTimer.Start()));
    }


    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.HighQuality;
        g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;

        int cx = Width / 2;
        int cy = Height / 2 - 30;

        // ═══ LAYER 1: Deep Background ═══
        DrawBackground(g);

        // ═══ LAYER 2: Aurora Mesh Gradients ═══
        DrawAuroraMesh(g);

        // ═══ LAYER 3: Noise Texture Overlay ═══
        DrawNoiseOverlay(g);

        // ═══ LAYER 4: Floating Particles ═══
        DrawParticles(g);

        // ═══ LAYER 5: Center Content ═══
        DrawLogoComposition(g, cx, cy);
        DrawTitle(g, cx, cy);
        DrawSubtitle(g, cx, cy);
        DrawProgressBar(g, cx, cy);
        DrawStatus(g, cx, cy);

        // ═══ LAYER 6: Version Footer ═══
        DrawVersion(g);

        // ═══ LAYER 7: Vignette ═══
        DrawVignette(g);
    }

    // ────────────────────────────────────────────────────────────
    // BACKGROUND — Deep dark gradient
    // ────────────────────────────────────────────────────────────
    private void DrawBackground(Graphics g)
    {
        using var bgBrush = new LinearGradientBrush(
            ClientRectangle,
            DesignTokens.Colors.GradientSplashTop,
            DesignTokens.Colors.GradientSplashBottom,
            LinearGradientMode.Vertical);
        g.FillRectangle(bgBrush, ClientRectangle);
    }

    // ────────────────────────────────────────────────────────────
    // AURORA — Drifting colored light orbs (mesh gradient simulation)
    // ────────────────────────────────────────────────────────────
    private void DrawAuroraMesh(Graphics g)
    {
        foreach (var orb in _orbs)
        {
            ModernRenderer.DrawRadialGlow(g, orb.X, orb.Y,
                orb.Radius, orb.Color, orb.Alpha);
        }
    }

    // ────────────────────────────────────────────────────────────
    // NOISE — Subtle texture grain (approximated with dots)
    // ────────────────────────────────────────────────────────────
    private void DrawNoiseOverlay(Graphics g)
    {
        // Minimal noise overlay — just a subtle darkening at edges
        using var noiseBrush = new SolidBrush(Color.FromArgb(3, 0, 0, 0));
        // Skip actual noise pattern for performance — the aurora already provides texture
    }


    // ────────────────────────────────────────────────────────────
    // PARTICLES — Floating finance symbols
    // ────────────────────────────────────────────────────────────
    private void DrawParticles(Graphics g)
    {
        foreach (var p in _particles)
        {
            int alpha = (int)(p.Opacity * 255 * _contentOpacity);
            if (alpha < 1) continue;
            using var brush = new SolidBrush(Color.FromArgb(alpha, 140, 145, 180));
            using var font = new Font(_particleFont!.FontFamily, p.Size);
            g.DrawString(p.Symbol, font, brush, p.X, p.Y);
        }
    }

    // ────────────────────────────────────────────────────────────
    // LOGO COMPOSITION — Glass card + gradient circle + M + ring
    // ────────────────────────────────────────────────────────────
    private void DrawLogoComposition(Graphics g, int cx, int cy)
    {
        int logoSize = (int)(110 * _logoScale);
        if (logoSize < 3) return;

        int logoX = cx - logoSize / 2;
        int logoY = cy - 110 - logoSize / 2;

        // ─── Glass backdrop card (subtle) ───
        if (_logoScale > 0.5f)
        {
            float cardAlpha = Math.Min(1f, (_logoScale - 0.5f) * 3f);
            int cardSize = logoSize + 48;
            var cardRect = new RectangleF(
                cx - cardSize / 2f, cy - 110 - cardSize / 2f,
                cardSize, cardSize);
            var cardColor = Color.FromArgb((int)(12 * cardAlpha), 255, 255, 255);
            ModernRenderer.FillRoundedRect(g, cardRect, 24, cardColor);
            ModernRenderer.DrawRoundedRect(g, cardRect, 24,
                Color.FromArgb((int)(15 * cardAlpha), 255, 255, 255));
        }

        // ─── Outer glow (breathing) ───
        float glowSize = logoSize + 40 + _logoGlow * 20f;
        ModernRenderer.DrawRadialGlow(g, cx, cy - 110,
            glowSize / 2f, DesignTokens.Colors.Primary400,
            (int)(18 * _logoScale));

        // ─── Gradient circle ───
        using var logoBrush = new LinearGradientBrush(
            new Rectangle(logoX - 2, logoY - 2, logoSize + 4, logoSize + 4),
            DesignTokens.Colors.GradientAuroraStart,
            DesignTokens.Colors.GradientAuroraMid,
            LinearGradientMode.ForwardDiagonal);
        g.FillEllipse(logoBrush, logoX, logoY, logoSize, logoSize);

        // ─── Inner highlight (glass reflection) ───
        var highlightRect = new RectangleF(
            logoX + logoSize * 0.15f, logoY + logoSize * 0.08f,
            logoSize * 0.7f, logoSize * 0.4f);
        using var highlightBrush = new LinearGradientBrush(
            highlightRect,
            Color.FromArgb(30, 255, 255, 255),
            Color.FromArgb(0, 255, 255, 255),
            LinearGradientMode.Vertical);
        g.FillEllipse(highlightBrush, highlightRect);

        // ─── "M" letter ───
        int mAlpha = (int)(255 * Math.Min(1f, _logoScale * 1.2f));
        using var mBrush = new SolidBrush(Color.FromArgb(mAlpha, 255, 255, 255));
        var sf = new StringFormat
        { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
        using var mFont = new Font(_logoFont!.FontFamily, logoSize * 0.34f, FontStyle.Bold);
        g.DrawString("M", mFont, mBrush,
            new RectangleF(logoX, logoY - 2, logoSize, logoSize), sf);

        // ─── Progress ring (around logo) ───
        DrawLogoRing(g, cx, cy - 110, logoSize);
    }


    private void DrawLogoRing(Graphics g, float cx, float cy, int logoSize)
    {
        int ringSize = logoSize + 32;
        float ringX = cx - ringSize / 2f;
        float ringY = cy - ringSize / 2f;
        var ringRect = new RectangleF(ringX, ringY, ringSize, ringSize);

        // Track ring (subtle)
        float pulse = (MathF.Sin(_ringPulse) + 1f) / 2f;
        int trackAlpha = (int)(20 + pulse * 10);
        using var trackPen = new Pen(Color.FromArgb(trackAlpha, 255, 255, 255), 1.5f);
        g.DrawEllipse(trackPen, ringRect);

        // Progress arc
        float sweepAngle = _progressBarWidth * 360f;
        if (sweepAngle > 1f)
        {
            using var arcPen = new Pen(DesignTokens.Colors.Primary400, 2f)
            {
                StartCap = LineCap.Round,
                EndCap = LineCap.Round
            };
            g.DrawArc(arcPen, ringRect, _arcRotation - 90f, sweepAngle);

            // Glow dot at arc end
            float endAngle = (_arcRotation - 90f + sweepAngle) * MathF.PI / 180f;
            float dotX = cx + (ringSize / 2f) * MathF.Cos(endAngle);
            float dotY = cy + (ringSize / 2f) * MathF.Sin(endAngle);
            g.FillEllipse(new SolidBrush(Color.FromArgb(120, DesignTokens.Colors.Primary300)),
                dotX - 3, dotY - 3, 6, 6);
        }
    }

    // ────────────────────────────────────────────────────────────
    // TITLE
    // ────────────────────────────────────────────────────────────
    private void DrawTitle(Graphics g, int cx, int cy)
    {
        int alpha = (int)(255 * _contentOpacity);
        if (alpha < 1) return;

        // Main title with subtle letter spacing simulation
        using var titleBrush = new SolidBrush(
            Color.FromArgb(alpha, DesignTokens.Colors.DarkTextPrimary));
        var sf = new StringFormat { Alignment = StringAlignment.Center };
        g.DrawString("M-Finlogs", _titleFont!, titleBrush, cx, cy + 16, sf);
    }

    // ────────────────────────────────────────────────────────────
    // SUBTITLE (typing effect)
    // ────────────────────────────────────────────────────────────
    private void DrawSubtitle(Graphics g, int cx, int cy)
    {
        int alpha = (int)(200 * _contentOpacity);
        if (alpha < 1) return;

        string visible = _subtitle.Substring(0, Math.Min(_typingIndex, _subtitle.Length));
        using var brush = new SolidBrush(
            Color.FromArgb(alpha, DesignTokens.Colors.DarkTextSecondary));
        var sf = new StringFormat { Alignment = StringAlignment.Center };
        g.DrawString(visible, _subtitleFont!, brush, cx, cy + 78, sf);

        // Typing cursor
        if (_typingIndex < _subtitle.Length)
        {
            var cursorAlpha = (int)(200 * _contentOpacity * ((MathF.Sin(_meshPhase * 8f) + 1) / 2f));
            var textSize = g.MeasureString(visible, _subtitleFont!);
            float cursorX = cx + textSize.Width / 2f + 2;
            using var cursorBrush = new SolidBrush(
                Color.FromArgb(cursorAlpha, DesignTokens.Colors.Primary400));
            g.FillRectangle(cursorBrush, cursorX, cy + 80, 1.5f, 16);
        }
    }


    // ────────────────────────────────────────────────────────────
    // PROGRESS BAR — Thin, elegant, gradient-filled
    // ────────────────────────────────────────────────────────────
    private void DrawProgressBar(Graphics g, int cx, int cy)
    {
        if (_contentOpacity < 0.1f) return;

        float barWidth = 240f;
        float barHeight = 3f;
        float barX = cx - barWidth / 2f;
        float barY = cy + 118;

        int alpha = (int)(255 * _contentOpacity);

        // Track
        var trackRect = new RectangleF(barX, barY, barWidth, barHeight);
        ModernRenderer.FillRoundedRect(g, trackRect, barHeight / 2f,
            Color.FromArgb((int)(30 * _contentOpacity), 255, 255, 255));

        // Fill
        float fillWidth = barWidth * _progressBarWidth;
        if (fillWidth > 2)
        {
            var fillRect = new RectangleF(barX, barY, fillWidth, barHeight);
            using var fillBrush = new LinearGradientBrush(
                new RectangleF(barX, barY, barWidth, barHeight),
                Color.FromArgb(alpha, DesignTokens.Colors.Primary400),
                Color.FromArgb(alpha, DesignTokens.Colors.GradientAuroraMid),
                LinearGradientMode.Horizontal);
            using var fillPath = ModernRenderer.CreateRoundedRect(fillRect, barHeight / 2f);
            g.FillPath(fillBrush, fillPath);

            // End glow
            g.FillEllipse(new SolidBrush(
                Color.FromArgb((int)(60 * _contentOpacity), DesignTokens.Colors.Primary300)),
                barX + fillWidth - 4, barY - 2, 8, barHeight + 4);
        }
    }

    // ────────────────────────────────────────────────────────────
    // STATUS TEXT
    // ────────────────────────────────────────────────────────────
    private void DrawStatus(Graphics g, int cx, int cy)
    {
        int alpha = (int)(140 * _contentOpacity * _statusOpacity);
        if (alpha < 1) return;

        // Animated dots
        int dots = ((int)(_arcRotation / 40f)) % 4;
        string display = _statusText + new string('.', dots);

        using var brush = new SolidBrush(
            Color.FromArgb(alpha, DesignTokens.Colors.DarkTextMuted));
        var sf = new StringFormat { Alignment = StringAlignment.Center };
        g.DrawString(display, _statusFont!, brush, cx, cy + 140, sf);
    }

    // ────────────────────────────────────────────────────────────
    // VERSION
    // ────────────────────────────────────────────────────────────
    private void DrawVersion(Graphics g)
    {
        int alpha = (int)(50 * _contentOpacity);
        using var brush = new SolidBrush(Color.FromArgb(alpha, 255, 255, 255));
        var sf = new StringFormat { Alignment = StringAlignment.Center };
        g.DrawString(_version, _versionFont!, brush, Width / 2f, Height - 36, sf);
    }

    // ────────────────────────────────────────────────────────────
    // VIGNETTE — Soft darkening at edges
    // ────────────────────────────────────────────────────────────
    private void DrawVignette(Graphics g)
    {
        int margin = Math.Min(Width, Height) / 4;
        using var path = new GraphicsPath();
        path.AddEllipse(-margin, -margin, Width + margin * 2, Height + margin * 2);
        using var brush = new PathGradientBrush(path)
        {
            CenterColor = Color.FromArgb(0, 0, 0, 0),
            SurroundColors = new[] { Color.FromArgb(40, 0, 0, 0) }
        };
        g.FillRectangle(brush, ClientRectangle);
    }


    // ═══════════════════════════════════════════════════════════════
    // PUBLIC API (preserved from original)
    // ═══════════════════════════════════════════════════════════════

    /// <summary>Update the status text shown below the progress bar</summary>
    public void UpdateStatus(string text)
    {
        if (InvokeRequired)
            Invoke(() => { _statusText = text; _statusOpacity = 0f; });
        else
        { _statusText = text; _statusOpacity = 0f; }
    }

    /// <summary>Set progress value (0.0 to 1.0)</summary>
    public void SetProgress(float value)
    {
        _progress = Math.Clamp(value, 0f, 1f);
    }

    /// <summary>Smoothly fade out and close the splash</summary>
    public void FadeOutAndClose()
    {
        _isFadingOut = true;
        _progress = 1f;

        _fadeTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _fadeTimer.Tick += (s, e) =>
        {
            _fadeOpacity -= 0.04f;
            Opacity = Math.Max(0, _fadeOpacity);
            if (_fadeOpacity <= 0)
            {
                _fadeTimer.Stop();
                _fadeTimer.Dispose();
                Close();
            }
        };
        _fadeTimer.Start();
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _animTimer?.Stop(); _animTimer?.Dispose();
        _typingTimer?.Stop(); _typingTimer?.Dispose();
        _fadeTimer?.Stop(); _fadeTimer?.Dispose();
        base.OnFormClosing(e);
    }

    // ═══════════════════════════════════════════════════════════════
    // DATA MODELS
    // ═══════════════════════════════════════════════════════════════

    private class Particle
    {
        public string Symbol { get; set; } = "";
        public float X { get; set; }
        public float Y { get; set; }
        public float Speed { get; set; }
        public float Opacity { get; set; }
        public float Size { get; set; }
        public float Drift { get; set; }
        public float RotationSpeed { get; set; }
    }

    private class AuroraOrb
    {
        public float X { get; set; }
        public float Y { get; set; }
        public float Radius { get; set; }
        public Color Color { get; set; }
        public int Alpha { get; set; }
        public float SpeedX { get; set; }
        public float SpeedY { get; set; }
        public float Phase { get; set; }
    }
}
