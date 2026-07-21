namespace MFinlogs.Desktop.UI.Controls;

/// <summary>
/// Premium text input with floating label, animated focus ring, glow,
/// validation states, and smooth transitions. Wraps a standard TextBox
/// with custom-painted chrome.
/// </summary>
public class ModernTextBox : UserControl
{
    private readonly TextBox _innerBox;
    private string _placeholder = "";
    private string _label = "";
    private string _hint = "";
    private string _errorText = "";
    private bool _hasError;

    // Animation state
    private float _focusProgress;
    private float _hoverProgress;
    private float _errorShake;
    private bool _isFocused;
    private bool _isHovered;

    private System.Windows.Forms.Timer? _animTimer;


    public string Placeholder
    {
        get => _placeholder;
        set { _placeholder = value; Invalidate(); }
    }

    public string Label
    {
        get => _label;
        set { _label = value; Invalidate(); }
    }

    public string Hint
    {
        get => _hint;
        set { _hint = value; Invalidate(); }
    }

    public string ErrorText
    {
        get => _errorText;
        set { _errorText = value; _hasError = !string.IsNullOrEmpty(value); Invalidate(); }
    }

    public override string Text
    {
        get => _innerBox.Text;
        set => _innerBox.Text = value ?? "";
    }

    public bool UsePasswordChar
    {
        get => _innerBox.UseSystemPasswordChar;
        set => _innerBox.UseSystemPasswordChar = value;
    }

    public event EventHandler? TextValueChanged;

    public ModernTextBox()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.UserPaint |
                 ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.ResizeRedraw, true);

        Height = 64; // Label + input + hint
        BackColor = Color.Transparent;

        _innerBox = new TextBox
        {
            BorderStyle = BorderStyle.None,
            Font = DesignTokens.Typography.Body,
            ForeColor = DesignTokens.Colors.TextPrimary,
            BackColor = DesignTokens.Colors.Surface,
            Location = new Point(DesignTokens.Spacing.InputPaddingX + 2, 24),
            Anchor = AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Top
        };

        _innerBox.TextChanged += (s, e) => { TextValueChanged?.Invoke(this, e); Invalidate(); };
        _innerBox.GotFocus += (s, e) => { _isFocused = true; Invalidate(); };
        _innerBox.LostFocus += (s, e) => { _isFocused = false; Invalidate(); };

        Controls.Add(_innerBox);
        StartAnimationTimer();
    }

    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        _innerBox.Width = Width - (DesignTokens.Spacing.InputPaddingX * 2) - 4;
        _innerBox.Location = new Point(DesignTokens.Spacing.InputPaddingX + 2,
            string.IsNullOrEmpty(_label) ? 12 : 24);
    }


    private void StartAnimationTimer()
    {
        _animTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _animTimer.Tick += (s, e) =>
        {
            bool dirty = false;

            float focusTarget = _isFocused ? 1f : 0f;
            float fd = (focusTarget - _focusProgress) * 0.14f;
            if (MathF.Abs(fd) > 0.001f) { _focusProgress += fd; dirty = true; }
            else _focusProgress = focusTarget;

            float hoverTarget = _isHovered ? 1f : 0f;
            float hd = (hoverTarget - _hoverProgress) * 0.12f;
            if (MathF.Abs(hd) > 0.001f) { _hoverProgress += hd; dirty = true; }
            else _hoverProgress = hoverTarget;

            if (_errorShake > 0.01f) { _errorShake *= 0.85f; dirty = true; }
            else _errorShake = 0;

            if (dirty) Invalidate();
        };
        _animTimer.Start();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        int labelHeight = string.IsNullOrEmpty(_label) ? 0 : 18;
        int hintHeight = (!string.IsNullOrEmpty(_hint) || _hasError) ? 18 : 0;

        // Input box area
        float inputY = labelHeight;
        float inputH = Height - labelHeight - hintHeight;
        float shakeOffset = _errorShake * 4f * MathF.Sin(_errorShake * 20f);

        var inputRect = new RectangleF(
            1 + shakeOffset, inputY, Width - 3, inputH);
        float radius = DesignTokens.Radius.SM;

        // Label
        if (!string.IsNullOrEmpty(_label))
        {
            var labelColor = _isFocused
                ? DesignTokens.Colors.Primary500
                : DesignTokens.Colors.TextSecondary;
            using var labelBrush = new SolidBrush(labelColor);
            g.DrawString(_label, DesignTokens.Typography.Label, labelBrush, 2, 0);
        }

        // Focus glow
        if (_focusProgress > 0.01f && !_hasError)
        {
            var glowColor = Color.FromArgb(
                (int)(40 * _focusProgress), DesignTokens.Colors.Primary400);
            ModernRenderer.DrawShapeGlow(g, inputRect, radius, glowColor, 6);
        }

        // Background
        ModernRenderer.FillRoundedRect(g, inputRect, radius,
            DesignTokens.Colors.Surface);

        // Border
        Color borderColor;
        if (_hasError)
            borderColor = DesignTokens.Colors.Danger;
        else if (_isFocused)
            borderColor = Color.FromArgb(
                (int)(255 * _focusProgress), DesignTokens.Colors.Primary500);
        else if (_isHovered)
            borderColor = DesignTokens.Colors.StrokeHover;
        else
            borderColor = DesignTokens.Colors.Stroke;

        ModernRenderer.DrawRoundedRect(g, inputRect, radius, borderColor,
            _isFocused ? 1.5f : 1f);

        // Placeholder
        if (string.IsNullOrEmpty(_innerBox.Text) && !_isFocused &&
            !string.IsNullOrEmpty(_placeholder))
        {
            using var phBrush = new SolidBrush(DesignTokens.Colors.TextMuted);
            g.DrawString(_placeholder, DesignTokens.Typography.Body, phBrush,
                DesignTokens.Spacing.InputPaddingX + 2, inputY + inputH / 2f - 8);
        }

        // Hint / Error text
        if (_hasError)
        {
            using var errBrush = new SolidBrush(DesignTokens.Colors.Danger);
            g.DrawString(_errorText, DesignTokens.Typography.Caption, errBrush,
                2, inputY + inputH + 2);
        }
        else if (!string.IsNullOrEmpty(_hint))
        {
            using var hintBrush = new SolidBrush(DesignTokens.Colors.TextTertiary);
            g.DrawString(_hint, DesignTokens.Typography.Caption, hintBrush,
                2, inputY + inputH + 2);
        }
    }


    /// <summary>Trigger error shake animation</summary>
    public void ShakeError()
    {
        _errorShake = 1f;
    }

    public void ClearError()
    {
        _errorText = "";
        _hasError = false;
        Invalidate();
    }

    public void SelectAll() => _innerBox.SelectAll();
    public new void Focus() => _innerBox.Focus();

    protected override void OnMouseEnter(EventArgs e)
    { _isHovered = true; base.OnMouseEnter(e); }

    protected override void OnMouseLeave(EventArgs e)
    { _isHovered = false; base.OnMouseLeave(e); }

    protected override void OnClick(EventArgs e)
    { _innerBox.Focus(); base.OnClick(e); }

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
