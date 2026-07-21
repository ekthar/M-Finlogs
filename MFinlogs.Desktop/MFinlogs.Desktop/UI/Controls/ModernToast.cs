namespace MFinlogs.Desktop.UI.Controls;

/// <summary>
/// Premium toast notification with slide-in animation, auto-dismiss,
/// blur backdrop, and type variants (success, error, warning, info).
/// Floats above the form content.
/// </summary>
public class ModernToast : Control
{
    public enum ToastType { Info, Success, Warning, Error }

    private string _message = "";
    private ToastType _type = ToastType.Info;
    private float _slideProgress; // 0 = hidden, 1 = visible
    private float _opacity;
    private bool _isShowing;
    private bool _isDismissing;
    private System.Windows.Forms.Timer? _animTimer;
    private System.Windows.Forms.Timer? _autoDismissTimer;
    private int _autoDismissMs = 4000;

    public string Message
    {
        get => _message;
        set { _message = value; Invalidate(); }
    }

    public ToastType Type
    {
        get => _type;
        set { _type = value; Invalidate(); }
    }

    public int AutoDismissMs
    {
        get => _autoDismissMs;
        set => _autoDismissMs = value;
    }

    public ModernToast()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.UserPaint |
                 ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.SupportsTransparentBackColor, true);

        BackColor = Color.Transparent;
        Visible = false;
        Height = DesignTokens.Sizes.ToastMinHeight;
        Width = DesignTokens.Sizes.ToastWidth;

        _animTimer = new System.Windows.Forms.Timer { Interval = 16 };
        _animTimer.Tick += OnAnimTick;
    }


    /// <summary>Show the toast with slide-in animation</summary>
    public void Show(string message, ToastType type = ToastType.Info)
    {
        _message = message;
        _type = type;
        _slideProgress = 0f;
        _opacity = 0f;
        _isShowing = true;
        _isDismissing = false;
        Visible = true;
        BringToFront();
        _animTimer?.Start();

        // Auto-dismiss
        _autoDismissTimer?.Stop();
        _autoDismissTimer?.Dispose();
        _autoDismissTimer = new System.Windows.Forms.Timer
        { Interval = _autoDismissMs };
        _autoDismissTimer.Tick += (s, e) =>
        {
            _autoDismissTimer?.Stop();
            Dismiss();
        };
        _autoDismissTimer.Start();
    }

    /// <summary>Dismiss the toast with slide-out animation</summary>
    public void Dismiss()
    {
        _isDismissing = true;
        _isShowing = false;
        _animTimer?.Start();
    }

    private void OnAnimTick(object? sender, EventArgs e)
    {
        bool dirty = false;

        if (_isShowing)
        {
            _slideProgress += (1f - _slideProgress) * 0.12f;
            _opacity += (1f - _opacity) * 0.15f;
            dirty = true;
            if (_slideProgress > 0.99f)
            {
                _slideProgress = 1f;
                _opacity = 1f;
                _isShowing = false;
            }
        }
        else if (_isDismissing)
        {
            _slideProgress -= _slideProgress * 0.15f;
            _opacity -= _opacity * 0.18f;
            dirty = true;
            if (_opacity < 0.01f)
            {
                _isDismissing = false;
                Visible = false;
                _animTimer?.Stop();
            }
        }

        if (dirty) Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        float slideOffset = (1f - _slideProgress) * -20f;
        var rect = new RectangleF(0, slideOffset, Width, Height);
        float radius = DesignTokens.Radius.LG;

        // Get type color
        GetTypeColors(out var accentColor, out var bgColor, out var iconSymbol);

        // Shadow
        ModernRenderer.DrawShadow(g, rect, radius, 16, 4,
            Color.FromArgb((int)(20 * _opacity), 0, 0, 0));

        // Background
        var fillColor = Color.FromArgb((int)(240 * _opacity), bgColor);
        ModernRenderer.FillRoundedRect(g, rect, radius, fillColor);

        // Border
        ModernRenderer.DrawRoundedRect(g, rect, radius,
            Color.FromArgb((int)(40 * _opacity), accentColor));

        // Left accent bar
        var accentRect = new RectangleF(rect.X + 4, rect.Y + 12, 3, rect.Height - 24);
        ModernRenderer.FillRoundedRect(g, accentRect, 2,
            Color.FromArgb((int)(255 * _opacity), accentColor));

        // Icon
        using var iconBrush = new SolidBrush(
            Color.FromArgb((int)(255 * _opacity), accentColor));
        g.DrawString(iconSymbol, DesignTokens.Typography.Title, iconBrush,
            rect.X + 16, rect.Y + rect.Height / 2f - 10);

        // Message
        using var textBrush = new SolidBrush(
            Color.FromArgb((int)(230 * _opacity), DesignTokens.Colors.TextPrimary));
        var textRect = new RectangleF(rect.X + 40, rect.Y + 8,
            rect.Width - 56, rect.Height - 16);
        g.DrawString(_message, DesignTokens.Typography.Body, textBrush, textRect);
    }


    private void GetTypeColors(out Color accent, out Color bg, out string icon)
    {
        switch (_type)
        {
            case ToastType.Success:
                accent = DesignTokens.Colors.Success;
                bg = DesignTokens.Colors.SuccessLight;
                icon = "\u2713"; // checkmark
                break;
            case ToastType.Warning:
                accent = DesignTokens.Colors.Warning;
                bg = DesignTokens.Colors.WarningLight;
                icon = "\u26A0"; // warning
                break;
            case ToastType.Error:
                accent = DesignTokens.Colors.Danger;
                bg = DesignTokens.Colors.DangerLight;
                icon = "\u2717"; // X mark
                break;
            default:
                accent = DesignTokens.Colors.Info;
                bg = DesignTokens.Colors.InfoLight;
                icon = "\u2139"; // info
                break;
        }
    }

    protected override void OnClick(EventArgs e)
    {
        Dismiss();
        base.OnClick(e);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _animTimer?.Stop(); _animTimer?.Dispose();
            _autoDismissTimer?.Stop(); _autoDismissTimer?.Dispose();
        }
        base.Dispose(disposing);
    }
}
