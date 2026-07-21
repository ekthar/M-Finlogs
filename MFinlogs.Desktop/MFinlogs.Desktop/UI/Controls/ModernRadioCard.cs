namespace MFinlogs.Desktop.UI.Controls;

/// <summary>
/// Premium radio card — a selectable card with title, description,
/// animated selection ring, and hover effects. Used for mode selection
/// in the setup wizard. Inspired by Linear/Stripe onboarding.
/// </summary>
public class ModernRadioCard : Control
{
    private string _title = "";
    private string _description = "";
    private bool _isSelected;
    private string _iconSymbol = "";

    // Animation state
    private float _hoverProgress;
    private float _selectProgress;
    private bool _isHovered;

    private System.Windows.Forms.Timer? _animTimer;

    public event EventHandler? Selected;

    public string Title
    {
        get => _title;
        set { _title = value; Invalidate(); }
    }

    public string Description
    {
        get => _description;
        set { _description = value; Invalidate(); }
    }

    public string IconSymbol
    {
        get => _iconSymbol;
        set { _iconSymbol = value; Invalidate(); }
    }

    public bool IsSelected
    {
        get => _isSelected;
        set
        {
            if (_isSelected == value) return;
            _isSelected = value;
            Invalidate();
            if (value) Selected?.Invoke(this, EventArgs.Empty);
        }
    }


    public ModernRadioCard()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.UserPaint |
                 ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.ResizeRedraw |
                 ControlStyles.SupportsTransparentBackColor, true);

        BackColor = Color.Transparent;
        Cursor = Cursors.Hand;
        Height = 72;

        _animTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _animTimer.Tick += (s, e) =>
        {
            bool dirty = false;
            float ht = _isHovered ? 1f : 0f;
            float hd = (ht - _hoverProgress) * 0.14f;
            if (MathF.Abs(hd) > 0.001f) { _hoverProgress += hd; dirty = true; }
            else _hoverProgress = ht;

            float st = _isSelected ? 1f : 0f;
            float sd = (st - _selectProgress) * 0.12f;
            if (MathF.Abs(sd) > 0.001f) { _selectProgress += sd; dirty = true; }
            else _selectProgress = st;

            if (dirty) Invalidate();
        };
        _animTimer.Start();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        float radius = DesignTokens.Radius.LG;
        var rect = new RectangleF(1, 1, Width - 2, Height - 2);
        float offsetY = -_hoverProgress * 1f;
        var drawRect = new RectangleF(rect.X, rect.Y + offsetY,
            rect.Width, rect.Height);

        // Shadow on hover
        if (_hoverProgress > 0.01f)
            ModernRenderer.DrawShadow(g, drawRect, radius,
                (int)(10 * _hoverProgress), (int)(2 * _hoverProgress),
                Color.FromArgb((int)(12 * _hoverProgress), 0, 0, 0));

        // Background
        var bgColor = _isSelected
            ? Color.FromArgb((int)(255 * _selectProgress),
                DesignTokens.Colors.Primary50)
            : DesignTokens.Colors.Surface;
        ModernRenderer.FillRoundedRect(g, drawRect, radius, bgColor);

        // Border
        var borderColor = _isSelected
            ? Color.FromArgb((int)(255 * _selectProgress),
                DesignTokens.Colors.Primary400)
            : _isHovered
                ? DesignTokens.Colors.StrokeHover
                : DesignTokens.Colors.Stroke;
        ModernRenderer.DrawRoundedRect(g, drawRect, radius, borderColor,
            _isSelected ? 1.5f : 1f);

        // Radio indicator
        float radioX = drawRect.X + 16;
        float radioY = drawRect.Y + drawRect.Height / 2f - 9;
        DrawRadioIndicator(g, radioX, radioY, 18f);

        // Icon
        float contentX = radioX + 30;
        if (!string.IsNullOrEmpty(_iconSymbol))
        {
            using var iconBrush = new SolidBrush(_isSelected
                ? DesignTokens.Colors.Primary500
                : DesignTokens.Colors.TextTertiary);
            g.DrawString(_iconSymbol, DesignTokens.Typography.H3, iconBrush,
                contentX, drawRect.Y + drawRect.Height / 2f - 12);
            contentX += 32;
        }

        // Title
        using var titleBrush = new SolidBrush(DesignTokens.Colors.TextPrimary);
        g.DrawString(_title, DesignTokens.Typography.Title, titleBrush,
            contentX, drawRect.Y + 14);

        // Description
        using var descBrush = new SolidBrush(DesignTokens.Colors.TextTertiary);
        g.DrawString(_description, DesignTokens.Typography.Caption, descBrush,
            contentX, drawRect.Y + 36);
    }


    private void DrawRadioIndicator(Graphics g, float x, float y, float size)
    {
        var outerRect = new RectangleF(x, y, size, size);

        // Outer ring
        var ringColor = _isSelected
            ? DesignTokens.Colors.Primary500
            : DesignTokens.Colors.Neutral300;
        using var ringPen = new Pen(ringColor, _isSelected ? 2f : 1.5f);
        g.DrawEllipse(ringPen, outerRect);

        // Inner dot (animated scale)
        if (_selectProgress > 0.01f)
        {
            float dotSize = 8f * _selectProgress;
            float dotX = x + (size - dotSize) / 2f;
            float dotY = y + (size - dotSize) / 2f;
            using var dotBrush = new SolidBrush(
                Color.FromArgb((int)(255 * _selectProgress),
                    DesignTokens.Colors.Primary500));
            g.FillEllipse(dotBrush, dotX, dotY, dotSize, dotSize);
        }
    }

    protected override void OnMouseEnter(EventArgs e)
    { _isHovered = true; base.OnMouseEnter(e); }

    protected override void OnMouseLeave(EventArgs e)
    { _isHovered = false; base.OnMouseLeave(e); }

    protected override void OnClick(EventArgs e)
    { IsSelected = true; base.OnClick(e); }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { _animTimer?.Stop(); _animTimer?.Dispose(); }
        base.Dispose(disposing);
    }
}
