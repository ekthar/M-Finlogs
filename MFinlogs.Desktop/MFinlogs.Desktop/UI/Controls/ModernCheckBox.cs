namespace MFinlogs.Desktop.UI.Controls;

/// <summary>
/// Premium checkbox with animated checkmark, spring bounce,
/// and smooth state transitions.
/// </summary>
public class ModernCheckBox : Control
{
    private bool _isChecked;
    private float _checkProgress;
    private float _hoverProgress;
    private bool _isHovered;
    private System.Windows.Forms.Timer? _animTimer;

    public event EventHandler? CheckedChanged;

    public bool Checked
    {
        get => _isChecked;
        set
        {
            if (_isChecked == value) return;
            _isChecked = value;
            CheckedChanged?.Invoke(this, EventArgs.Empty);
            Invalidate();
        }
    }

    public ModernCheckBox()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.UserPaint |
                 ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.SupportsTransparentBackColor, true);

        BackColor = Color.Transparent;
        ForeColor = DesignTokens.Colors.TextPrimary;
        Font = DesignTokens.Typography.Body;
        Cursor = Cursors.Hand;
        Height = 28;

        _animTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _animTimer.Tick += (s, e) =>
        {
            bool dirty = false;
            float ct = _isChecked ? 1f : 0f;
            float cd = (ct - _checkProgress) * 0.16f;
            if (MathF.Abs(cd) > 0.001f) { _checkProgress += cd; dirty = true; }
            else _checkProgress = ct;

            float ht = _isHovered ? 1f : 0f;
            float hd = (ht - _hoverProgress) * 0.14f;
            if (MathF.Abs(hd) > 0.001f) { _hoverProgress += hd; dirty = true; }
            else _hoverProgress = ht;

            if (dirty) Invalidate();
        };
        _animTimer.Start();
    }


    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        float boxSize = 18f;
        float boxY = (Height - boxSize) / 2f;
        var boxRect = new RectangleF(2, boxY, boxSize, boxSize);
        float radius = DesignTokens.Radius.XS;

        // Hover glow
        if (_hoverProgress > 0.01f)
        {
            var glowColor = Color.FromArgb(
                (int)(30 * _hoverProgress), DesignTokens.Colors.Primary400);
            ModernRenderer.DrawShapeGlow(g, boxRect, radius, glowColor, 4);
        }

        // Box background
        if (_checkProgress > 0.01f)
        {
            // Filled state — gradient primary
            var fillColor = Color.FromArgb(
                (int)(255 * _checkProgress), DesignTokens.Colors.Primary500);
            ModernRenderer.FillRoundedRect(g, boxRect, radius, fillColor);
        }
        else
        {
            ModernRenderer.FillRoundedRect(g, boxRect, radius,
                DesignTokens.Colors.Surface);
        }

        // Border
        var borderColor = _checkProgress > 0.5f
            ? Color.FromArgb((int)(255 * _checkProgress),
                DesignTokens.Colors.Primary500)
            : _isHovered
                ? DesignTokens.Colors.StrokeHover
                : DesignTokens.Colors.Neutral300;
        ModernRenderer.DrawRoundedRect(g, boxRect, radius, borderColor, 1.5f);

        // Checkmark
        if (_checkProgress > 0.1f)
        {
            var checkColor = Color.FromArgb(
                (int)(255 * _checkProgress), Color.White);
            var checkBounds = new RectangleF(
                boxRect.X + 3, boxRect.Y + 3,
                boxRect.Width - 6, boxRect.Height - 6);
            ModernRenderer.DrawCheckmark(g, checkBounds, checkColor, 2f);
        }

        // Label text
        if (!string.IsNullOrEmpty(Text))
        {
            using var textBrush = new SolidBrush(ForeColor);
            float textX = boxSize + 10;
            float textY = (Height - Font.Height) / 2f;
            g.DrawString(Text, Font, textBrush, textX, textY);
        }
    }

    protected override void OnMouseEnter(EventArgs e)
    { _isHovered = true; base.OnMouseEnter(e); }

    protected override void OnMouseLeave(EventArgs e)
    { _isHovered = false; base.OnMouseLeave(e); }

    protected override void OnClick(EventArgs e)
    { Checked = !Checked; base.OnClick(e); }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Space) Checked = !Checked;
        base.OnKeyDown(e);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { _animTimer?.Stop(); _animTimer?.Dispose(); }
        base.Dispose(disposing);
    }
}
