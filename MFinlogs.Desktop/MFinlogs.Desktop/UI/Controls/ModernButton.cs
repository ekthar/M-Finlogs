using System.Drawing.Drawing2D;

namespace MFinlogs.Desktop.UI.Controls;

/// <summary>
/// Premium button control with hover lift, press depth, glow effects,
/// and smooth spring animations. Supports Primary, Secondary, Ghost,
/// Danger, and Icon variants.
/// </summary>
public class ModernButton : Control
{
    // ─── Variant ───
    public enum ButtonVariant { Primary, Secondary, Ghost, Danger, Icon }
    public enum ButtonSize { Small, Medium, Large }

    private ButtonVariant _variant = ButtonVariant.Primary;
    private ButtonSize _size = ButtonSize.Medium;
    private string _iconText = "";
    private bool _isLoading;

    // ─── Animation State ───
    private float _hoverProgress;
    private float _pressProgress;
    private float _focusProgress;
    private float _loadingAngle;
    private bool _isHovered;
    private bool _isPressed;
    private bool _isFocused;

    private System.Windows.Forms.Timer? _animTimer;


    public ButtonVariant Variant
    {
        get => _variant;
        set { _variant = value; Invalidate(); }
    }

    public ButtonSize Size
    {
        get => _size;
        set { _size = value; UpdateSize(); Invalidate(); }
    }

    public string IconText
    {
        get => _iconText;
        set { _iconText = value; Invalidate(); }
    }

    public bool IsLoading
    {
        get => _isLoading;
        set { _isLoading = value; Enabled = !value; Invalidate(); }
    }

    public ModernButton()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.UserPaint |
                 ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.ResizeRedraw |
                 ControlStyles.SupportsTransparentBackColor, true);

        BackColor = Color.Transparent;
        ForeColor = DesignTokens.Colors.TextOnPrimary;
        Font = DesignTokens.Typography.Button;
        Cursor = Cursors.Hand;
        TabStop = true;
        UpdateSize();
        StartAnimationTimer();
    }

    private void UpdateSize()
    {
        int h = _size switch
        {
            ButtonSize.Small => DesignTokens.Sizes.ButtonHeightSM,
            ButtonSize.Large => DesignTokens.Sizes.ButtonHeightLG,
            _ => DesignTokens.Sizes.ButtonHeightMD
        };
        MinimumSize = new System.Drawing.Size(h, h);
        if (Height < h) Height = h;
    }

    private void StartAnimationTimer()
    {
        _animTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _animTimer.Tick += (s, e) =>
        {
            bool needsRepaint = false;

            // Hover spring
            float hoverTarget = _isHovered ? 1f : 0f;
            float hoverDelta = (hoverTarget - _hoverProgress) * 0.15f;
            if (MathF.Abs(hoverDelta) > 0.001f)
            { _hoverProgress += hoverDelta; needsRepaint = true; }
            else _hoverProgress = hoverTarget;

            // Press spring
            float pressTarget = _isPressed ? 1f : 0f;
            float pressDelta = (pressTarget - _pressProgress) * 0.2f;
            if (MathF.Abs(pressDelta) > 0.001f)
            { _pressProgress += pressDelta; needsRepaint = true; }
            else _pressProgress = pressTarget;

            // Focus
            float focusTarget = _isFocused ? 1f : 0f;
            float focusDelta = (focusTarget - _focusProgress) * 0.12f;
            if (MathF.Abs(focusDelta) > 0.001f)
            { _focusProgress += focusDelta; needsRepaint = true; }
            else _focusProgress = focusTarget;

            // Loading spinner
            if (_isLoading) { _loadingAngle += 8f; needsRepaint = true; }

            if (needsRepaint) Invalidate();
        };
        _animTimer.Start();
    }


    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        float radius = _size == ButtonSize.Small
            ? DesignTokens.Radius.SM : DesignTokens.Radius.MD;
        var rect = new RectangleF(2, 2, Width - 4, Height - 4);

        // Scale on press (subtle squish)
        float scale = 1f - _pressProgress * 0.02f;
        float offsetY = -_hoverProgress * 1.5f + _pressProgress * 0.5f;

        var drawRect = new RectangleF(
            rect.X + rect.Width * (1 - scale) / 2f,
            rect.Y + rect.Height * (1 - scale) / 2f + offsetY,
            rect.Width * scale,
            rect.Height * scale);

        // Get colors based on variant
        GetVariantColors(out var bgColor, out var textColor,
            out var hoverBg, out var borderColor, out var glowColor);

        // Focus ring
        if (_focusProgress > 0.01f)
        {
            var focusColor = Color.FromArgb(
                (int)(80 * _focusProgress), glowColor);
            ModernRenderer.DrawFocusRing(g, drawRect, radius, focusColor);
        }

        // Shadow (hover lift)
        if (_hoverProgress > 0.01f && _variant != ButtonVariant.Ghost)
        {
            ModernRenderer.DrawShadow(g, drawRect, radius,
                (int)(12 * _hoverProgress), (int)(3 * _hoverProgress),
                Color.FromArgb((int)(20 * _hoverProgress), 0, 0, 0));
        }

        // Background
        if (_variant == ButtonVariant.Ghost)
        {
            if (_hoverProgress > 0.01f)
                ModernRenderer.FillRoundedRect(g, drawRect, radius,
                    Color.FromArgb((int)(hoverBg.A * _hoverProgress), hoverBg));
        }
        else if (_variant == ButtonVariant.Primary)
        {
            // Gradient background for primary
            Color startC = Color.FromArgb(bgColor.R, bgColor.G, bgColor.B);
            Color endC = Color.FromArgb(
                Math.Max(0, bgColor.R - 15),
                Math.Max(0, bgColor.G - 10),
                Math.Min(255, bgColor.B + 10));
            ModernRenderer.FillRoundedRectGradient(g, drawRect, radius,
                startC, endC, LinearGradientMode.Vertical);

            // Hover glow overlay
            if (_hoverProgress > 0.01f)
                ModernRenderer.FillRoundedRect(g, drawRect, radius,
                    Color.FromArgb((int)(20 * _hoverProgress), 255, 255, 255));
        }
        else
        {
            ModernRenderer.FillRoundedRect(g, drawRect, radius, bgColor);
            if (_hoverProgress > 0.01f)
                ModernRenderer.FillRoundedRect(g, drawRect, radius,
                    Color.FromArgb((int)(hoverBg.A * _hoverProgress), hoverBg));
        }

        // Border
        if (borderColor.A > 0 && _variant != ButtonVariant.Primary)
            ModernRenderer.DrawRoundedRect(g, drawRect, radius, borderColor);

        // Content
        if (_isLoading)
            DrawLoadingSpinner(g, drawRect, textColor);
        else
            DrawContent(g, drawRect, textColor);
    }


    private void DrawContent(Graphics g, RectangleF rect, Color textColor)
    {
        var font = _size switch
        {
            ButtonSize.Small => DesignTokens.Typography.ButtonSmall,
            _ => DesignTokens.Typography.Button
        };

        if (!string.IsNullOrEmpty(_iconText) && !string.IsNullOrEmpty(Text))
        {
            // Icon + text layout
            var textSize = g.MeasureString(Text, font);
            float totalWidth = 18 + 6 + textSize.Width;
            float startX = rect.X + (rect.Width - totalWidth) / 2f;

            using var iconBrush = new SolidBrush(textColor);
            g.DrawString(_iconText, DesignTokens.Typography.Body, iconBrush,
                startX, rect.Y + (rect.Height - 16) / 2f);

            using var textBrush = new SolidBrush(textColor);
            g.DrawString(Text, font, textBrush,
                startX + 24, rect.Y + (rect.Height - textSize.Height) / 2f);
        }
        else
        {
            ModernRenderer.DrawTextCentered(g, Text, font, textColor, rect);
        }
    }

    private void DrawLoadingSpinner(Graphics g, RectangleF rect, Color color)
    {
        float size = 16f;
        float cx = rect.X + rect.Width / 2f;
        float cy = rect.Y + rect.Height / 2f;

        using var pen = new Pen(Color.FromArgb(60, color), 2f)
        {
            StartCap = System.Drawing.Drawing2D.LineCap.Round,
            EndCap = System.Drawing.Drawing2D.LineCap.Round
        };
        g.DrawEllipse(pen, cx - size / 2, cy - size / 2, size, size);

        using var arcPen = new Pen(color, 2f)
        {
            StartCap = System.Drawing.Drawing2D.LineCap.Round,
            EndCap = System.Drawing.Drawing2D.LineCap.Round
        };
        g.DrawArc(arcPen, cx - size / 2, cy - size / 2, size, size,
            _loadingAngle, 90f);
    }

    private void GetVariantColors(out Color bg, out Color text,
        out Color hoverBg, out Color border, out Color glow)
    {
        if (!Enabled)
        {
            bg = DesignTokens.Colors.Disabled;
            text = DesignTokens.Colors.DisabledText;
            hoverBg = Color.Transparent;
            border = DesignTokens.Colors.Stroke;
            glow = Color.Transparent;
            return;
        }

        switch (_variant)
        {
            case ButtonVariant.Primary:
                bg = DesignTokens.Colors.Primary500;
                text = DesignTokens.Colors.TextOnPrimary;
                hoverBg = Color.FromArgb(20, 255, 255, 255);
                border = Color.Transparent;
                glow = DesignTokens.Colors.Primary400;
                break;
            case ButtonVariant.Secondary:
                bg = DesignTokens.Colors.Surface;
                text = DesignTokens.Colors.TextPrimary;
                hoverBg = Color.FromArgb(12, 0, 0, 0);
                border = DesignTokens.Colors.Stroke;
                glow = DesignTokens.Colors.Primary300;
                break;
            case ButtonVariant.Ghost:
                bg = Color.Transparent;
                text = DesignTokens.Colors.TextSecondary;
                hoverBg = Color.FromArgb(10, 0, 0, 0);
                border = Color.Transparent;
                glow = DesignTokens.Colors.Primary300;
                break;
            case ButtonVariant.Danger:
                bg = DesignTokens.Colors.Danger;
                text = Color.White;
                hoverBg = Color.FromArgb(20, 255, 255, 255);
                border = Color.Transparent;
                glow = DesignTokens.Colors.Danger;
                break;
            default:
                bg = DesignTokens.Colors.Surface;
                text = DesignTokens.Colors.TextPrimary;
                hoverBg = Color.FromArgb(10, 0, 0, 0);
                border = DesignTokens.Colors.Stroke;
                glow = DesignTokens.Colors.Primary300;
                break;
        }
    }


    // ─── Mouse Events ───
    protected override void OnMouseEnter(EventArgs e)
    { _isHovered = true; base.OnMouseEnter(e); }

    protected override void OnMouseLeave(EventArgs e)
    { _isHovered = false; _isPressed = false; base.OnMouseLeave(e); }

    protected override void OnMouseDown(MouseEventArgs e)
    { if (e.Button == MouseButtons.Left) _isPressed = true; base.OnMouseDown(e); }

    protected override void OnMouseUp(MouseEventArgs e)
    { _isPressed = false; base.OnMouseUp(e); }

    protected override void OnGotFocus(EventArgs e)
    { _isFocused = true; base.OnGotFocus(e); }

    protected override void OnLostFocus(EventArgs e)
    { _isFocused = false; base.OnLostFocus(e); }

    // ─── Keyboard ───
    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Enter || e.KeyCode == Keys.Space)
        { _isPressed = true; Invalidate(); }
        base.OnKeyDown(e);
    }

    protected override void OnKeyUp(KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Enter || e.KeyCode == Keys.Space)
        {
            _isPressed = false;
            OnClick(EventArgs.Empty);
            Invalidate();
        }
        base.OnKeyUp(e);
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
