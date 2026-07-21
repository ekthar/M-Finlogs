namespace MFinlogs.Desktop.UI.Controls;

/// <summary>
/// Animated status dot with pulsing glow. Shows connection/sync state.
/// </summary>
public class ModernStatusIndicator : Control
{
    public enum StatusState { Connected, Syncing, Offline, Error }

    private StatusState _state = StatusState.Connected;
    private float _pulsePhase;
    private System.Windows.Forms.Timer? _animTimer;

    public StatusState State
    {
        get => _state;
        set { _state = value; Invalidate(); }
    }

    public ModernStatusIndicator()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.UserPaint |
                 ControlStyles.OptimizedDoubleBuffer |
                 ControlStyles.SupportsTransparentBackColor, true);

        BackColor = Color.Transparent;
        Size = new Size(12, 12);

        _animTimer = new System.Windows.Forms.Timer { Interval = 32 };
        _animTimer.Tick += (s, e) =>
        {
            _pulsePhase += 0.05f;
            if (_pulsePhase >= MathF.PI * 2) _pulsePhase -= MathF.PI * 2;
            Invalidate();
        };
        _animTimer.Start();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        float cx = Width / 2f;
        float cy = Height / 2f;
        float baseRadius = 4f;

        var color = _state switch
        {
            StatusState.Connected => DesignTokens.Colors.Success,
            StatusState.Syncing => DesignTokens.Colors.Info,
            StatusState.Offline => DesignTokens.Colors.TextMuted,
            StatusState.Error => DesignTokens.Colors.Danger,
            _ => DesignTokens.Colors.TextMuted
        };

        // Pulse glow (only for active states)
        if (_state == StatusState.Connected || _state == StatusState.Syncing)
        {
            float pulse = (MathF.Sin(_pulsePhase) + 1f) / 2f;
            float glowRadius = baseRadius + pulse * 3f;
            int glowAlpha = (int)(40 * (1f - pulse));
            g.FillEllipse(new SolidBrush(Color.FromArgb(glowAlpha, color)),
                cx - glowRadius, cy - glowRadius, glowRadius * 2, glowRadius * 2);
        }

        // Solid dot
        g.FillEllipse(new SolidBrush(color),
            cx - baseRadius, cy - baseRadius, baseRadius * 2, baseRadius * 2);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { _animTimer?.Stop(); _animTimer?.Dispose(); }
        base.Dispose(disposing);
    }
}
