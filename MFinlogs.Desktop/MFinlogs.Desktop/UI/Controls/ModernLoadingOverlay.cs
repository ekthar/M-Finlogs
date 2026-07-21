using System.Drawing.Drawing2D;

namespace MFinlogs.Desktop.UI.Controls;

/// <summary>
/// Premium loading overlay with animated gradient shimmer, spinning indicator,
/// and smooth fade transitions. Covers the WebView during load.
/// </summary>
public class ModernLoadingOverlay : Control
{
    private float _opacity = 1f;
    private float _spinnerAngle;
    private float _shimmerPhase;
    private bool _isFadingOut;
    private System.Windows.Forms.Timer? _animTimer;

    public ModernLoadingOverlay()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.UserPaint |
                 ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.SupportsTransparentBackColor, true);

        BackColor = Color.Transparent;
        Dock = DockStyle.Fill;

        _animTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _animTimer.Tick += (s, e) =>
        {
            _spinnerAngle += 5f;
            if (_spinnerAngle >= 360) _spinnerAngle -= 360;
            _shimmerPhase += 0.02f;
            if (_shimmerPhase >= 1f) _shimmerPhase -= 1f;

            if (_isFadingOut)
            {
                _opacity -= 0.04f;
                if (_opacity <= 0)
                {
                    _opacity = 0;
                    _isFadingOut = false;
                    Visible = false;
                    _animTimer?.Stop();
                }
            }
            Invalidate();
        };
        _animTimer.Start();
    }

    /// <summary>Show loading overlay</summary>
    public void ShowLoading()
    {
        _opacity = 1f;
        _isFadingOut = false;
        Visible = true;
        BringToFront();
        _animTimer?.Start();
    }

    /// <summary>Fade out and hide</summary>
    public void HideLoading()
    {
        _isFadingOut = true;
    }


    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        int alpha = (int)(255 * _opacity);
        if (alpha < 1) return;

        // Background
        using var bgBrush = new SolidBrush(
            Color.FromArgb(alpha, DesignTokens.Colors.Background));
        g.FillRectangle(bgBrush, ClientRectangle);

        // Shimmer gradient (diagonal moving)
        float shimmerX = -Width + _shimmerPhase * Width * 3;
        var shimmerRect = new RectangleF(shimmerX, 0, Width, Height);
        using var shimmerBrush = new LinearGradientBrush(
            new RectangleF(shimmerX, 0, Width * 0.6f, Height),
            Color.FromArgb(0, DesignTokens.Colors.Primary100),
            Color.FromArgb((int)(30 * _opacity), DesignTokens.Colors.Primary200),
            LinearGradientMode.ForwardDiagonal);
        g.FillRectangle(shimmerBrush, ClientRectangle);

        // Center content
        float cx = Width / 2f;
        float cy = Height / 2f - 20;

        // Spinner
        float spinSize = 32f;
        var spinRect = new RectangleF(cx - spinSize / 2, cy - spinSize / 2,
            spinSize, spinSize);

        // Track
        using var trackPen = new Pen(
            Color.FromArgb((int)(40 * _opacity), DesignTokens.Colors.Primary300), 2.5f);
        g.DrawEllipse(trackPen, spinRect);

        // Arc
        using var arcPen = new Pen(
            Color.FromArgb(alpha, DesignTokens.Colors.Primary500), 2.5f)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round
        };
        g.DrawArc(arcPen, spinRect, _spinnerAngle, 80f);

        // "Loading" text
        using var textBrush = new SolidBrush(
            Color.FromArgb((int)(180 * _opacity), DesignTokens.Colors.TextTertiary));
        var sf = new StringFormat { Alignment = StringAlignment.Center };
        g.DrawString("Loading", DesignTokens.Typography.Caption, textBrush,
            cx, cy + spinSize / 2 + 12, sf);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { _animTimer?.Stop(); _animTimer?.Dispose(); }
        base.Dispose(disposing);
    }
}
