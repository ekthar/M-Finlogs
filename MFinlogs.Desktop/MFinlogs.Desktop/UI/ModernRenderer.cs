using System.Drawing.Drawing2D;
using System.Drawing.Text;

namespace MFinlogs.Desktop.UI;

/// <summary>
/// Premium GDI+ rendering engine. Provides reusable drawing primitives
/// for glass effects, rounded rectangles, shadows, gradients, glows,
/// and all visual effects used throughout the application.
/// </summary>
public static class ModernRenderer
{
    // ═══════════════════════════════════════════════════════════════
    // GRAPHICS SETUP
    // ═══════════════════════════════════════════════════════════════

    /// <summary>Configure graphics for premium rendering quality</summary>
    public static void SetHighQuality(Graphics g)
    {
        g.SmoothingMode = SmoothingMode.HighQuality;
        g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.CompositingQuality = CompositingQuality.HighQuality;
    }

    /// <summary>Configure for splash/overlay rendering (anti-alias text)</summary>
    public static void SetSplashQuality(Graphics g)
    {
        g.SmoothingMode = SmoothingMode.HighQuality;
        g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
    }


    // ═══════════════════════════════════════════════════════════════
    // ROUNDED RECTANGLES
    // ═══════════════════════════════════════════════════════════════

    /// <summary>Create a rounded rectangle GraphicsPath</summary>
    public static GraphicsPath CreateRoundedRect(RectangleF rect, float radius)
    {
        var path = new GraphicsPath();
        if (radius <= 0)
        {
            path.AddRectangle(rect);
            return path;
        }

        float d = radius * 2f;
        var arcRect = new RectangleF(rect.X, rect.Y, d, d);

        // Top-left
        path.AddArc(arcRect, 180, 90);
        // Top-right
        arcRect.X = rect.Right - d;
        path.AddArc(arcRect, 270, 90);
        // Bottom-right
        arcRect.Y = rect.Bottom - d;
        path.AddArc(arcRect, 0, 90);
        // Bottom-left
        arcRect.X = rect.X;
        path.AddArc(arcRect, 90, 90);

        path.CloseFigure();
        return path;
    }

    /// <summary>Create rounded rect with individual corner radii</summary>
    public static GraphicsPath CreateRoundedRect(RectangleF rect,
        float topLeft, float topRight, float bottomRight, float bottomLeft)
    {
        var path = new GraphicsPath();
        float d;

        // Top-left
        d = topLeft * 2;
        if (d > 0) path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        else path.AddLine(rect.X, rect.Y, rect.X, rect.Y);

        // Top-right
        d = topRight * 2;
        if (d > 0) path.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        else path.AddLine(rect.Right, rect.Y, rect.Right, rect.Y);

        // Bottom-right
        d = bottomRight * 2;
        if (d > 0) path.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        else path.AddLine(rect.Right, rect.Bottom, rect.Right, rect.Bottom);

        // Bottom-left
        d = bottomLeft * 2;
        if (d > 0) path.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        else path.AddLine(rect.X, rect.Bottom, rect.X, rect.Bottom);

        path.CloseFigure();
        return path;
    }


    // ═══════════════════════════════════════════════════════════════
    // FILLED SHAPES
    // ═══════════════════════════════════════════════════════════════

    /// <summary>Fill a rounded rectangle with a solid color</summary>
    public static void FillRoundedRect(Graphics g, RectangleF rect,
        float radius, Color color)
    {
        using var path = CreateRoundedRect(rect, radius);
        using var brush = new SolidBrush(color);
        g.FillPath(brush, path);
    }

    /// <summary>Fill rounded rect with linear gradient</summary>
    public static void FillRoundedRectGradient(Graphics g, RectangleF rect,
        float radius, Color startColor, Color endColor,
        LinearGradientMode mode = LinearGradientMode.Vertical)
    {
        using var path = CreateRoundedRect(rect, radius);
        using var brush = new LinearGradientBrush(
            new RectangleF(rect.X, rect.Y, rect.Width + 1, rect.Height + 1),
            startColor, endColor, mode);
        g.FillPath(brush, path);
    }

    /// <summary>Draw rounded rect border/stroke</summary>
    public static void DrawRoundedRect(Graphics g, RectangleF rect,
        float radius, Color color, float width = 1f)
    {
        using var path = CreateRoundedRect(rect, radius);
        using var pen = new Pen(color, width);
        g.DrawPath(pen, path);
    }

    /// <summary>Fill a pill shape (fully rounded ends)</summary>
    public static void FillPill(Graphics g, RectangleF rect, Color color)
    {
        float radius = rect.Height / 2f;
        FillRoundedRect(g, rect, radius, color);
    }


    // ═══════════════════════════════════════════════════════════════
    // SHADOWS & ELEVATION
    // ═══════════════════════════════════════════════════════════════

    /// <summary>
    /// Draw a soft shadow behind a rounded rectangle.
    /// Uses multiple offset fills for a realistic diffused shadow.
    /// </summary>
    public static void DrawShadow(Graphics g, RectangleF rect, float radius,
        int blur, int offsetY, Color shadowColor)
    {
        int layers = Math.Min(blur / 2, 8);
        if (layers < 1) layers = 1;

        for (int i = layers; i >= 1; i--)
        {
            float expand = i * (blur / (float)layers);
            float alpha = shadowColor.A * (1f - (float)i / (layers + 1));

            var shadowRect = new RectangleF(
                rect.X - expand / 2f,
                rect.Y - expand / 2f + offsetY,
                rect.Width + expand,
                rect.Height + expand);

            using var path = CreateRoundedRect(shadowRect, radius + expand / 2f);
            using var brush = new SolidBrush(Color.FromArgb(
                (int)Math.Max(1, alpha), shadowColor.R, shadowColor.G, shadowColor.B));
            g.FillPath(brush, path);
        }
    }

    /// <summary>Draw elevation level 1 shadow (subtle)</summary>
    public static void DrawElevation1(Graphics g, RectangleF rect, float radius) =>
        DrawShadow(g, rect, radius, DesignTokens.Elevation.Shadow1Blur,
            DesignTokens.Elevation.Shadow1OffsetY, DesignTokens.Elevation.Shadow1);

    /// <summary>Draw elevation level 2 shadow (elevated)</summary>
    public static void DrawElevation2(Graphics g, RectangleF rect, float radius) =>
        DrawShadow(g, rect, radius, DesignTokens.Elevation.Shadow2Blur,
            DesignTokens.Elevation.Shadow2OffsetY, DesignTokens.Elevation.Shadow2);

    /// <summary>Draw elevation level 3 shadow (floating)</summary>
    public static void DrawElevation3(Graphics g, RectangleF rect, float radius) =>
        DrawShadow(g, rect, radius, DesignTokens.Elevation.Shadow3Blur,
            DesignTokens.Elevation.Shadow3OffsetY, DesignTokens.Elevation.Shadow3);


    // ═══════════════════════════════════════════════════════════════
    // GLOW & AMBIENT LIGHTING
    // ═══════════════════════════════════════════════════════════════

    /// <summary>
    /// Draw a radial glow (ambient light orb).
    /// Used in splash screens and backgrounds for depth.
    /// </summary>
    public static void DrawRadialGlow(Graphics g, float cx, float cy,
        float radius, Color color, int alpha = 12)
    {
        if (radius < 1) return;
        var bounds = new RectangleF(cx - radius, cy - radius, radius * 2, radius * 2);

        using var path = new GraphicsPath();
        path.AddEllipse(bounds);

        using var brush = new PathGradientBrush(path)
        {
            CenterColor = Color.FromArgb(alpha, color),
            SurroundColors = new[] { Color.FromArgb(0, color) },
            CenterPoint = new PointF(cx, cy)
        };
        g.FillEllipse(brush, bounds);
    }

    /// <summary>
    /// Draw a colored glow behind a shape (button glow, focus glow)
    /// </summary>
    public static void DrawShapeGlow(Graphics g, RectangleF rect,
        float radius, Color glowColor, int spread = 12)
    {
        for (int i = spread; i >= 1; i -= 2)
        {
            int alpha = glowColor.A * (spread - i) / (spread * 3);
            var glowRect = RectangleF.Inflate(rect, i, i);
            FillRoundedRect(g, glowRect, radius + i / 2f,
                Color.FromArgb(Math.Max(1, alpha), glowColor));
        }
    }

    /// <summary>Draw a focus ring around a control</summary>
    public static void DrawFocusRing(Graphics g, RectangleF rect,
        float radius, Color? color = null)
    {
        var ringColor = color ?? DesignTokens.Colors.FocusRing;
        int offset = DesignTokens.Sizes.FocusRingOffset;
        var ringRect = RectangleF.Inflate(rect, offset, offset);

        // Outer glow
        DrawShapeGlow(g, ringRect, radius + offset, ringColor, 6);

        // Ring stroke
        DrawRoundedRect(g, ringRect, radius + offset, ringColor,
            DesignTokens.Sizes.FocusRingWidth);
    }


    // ═══════════════════════════════════════════════════════════════
    // GLASS & SURFACE EFFECTS
    // ═══════════════════════════════════════════════════════════════

    /// <summary>
    /// Draw a glass/frosted panel (no actual blur in GDI+ but simulates with opacity)
    /// </summary>
    public static void DrawGlassPanel(Graphics g, RectangleF rect, float radius,
        Color? backgroundColor = null, float opacity = 0.85f)
    {
        var bgColor = backgroundColor ?? Color.White;
        var glassColor = Color.FromArgb((int)(opacity * 255), bgColor);

        // Background fill
        FillRoundedRect(g, rect, radius, glassColor);

        // Subtle top highlight (glass edge)
        var highlightRect = new RectangleF(rect.X + 1, rect.Y + 1,
            rect.Width - 2, rect.Height / 3f);
        FillRoundedRect(g, highlightRect, radius - 1,
            Color.FromArgb(8, 255, 255, 255));

        // Border
        DrawRoundedRect(g, rect, radius,
            Color.FromArgb((int)(DesignTokens.Effects.GlassBorderOpacity * 255), 255, 255, 255));
    }

    /// <summary>
    /// Draw a dark glass panel (for splash/dark mode surfaces)
    /// </summary>
    public static void DrawDarkGlassPanel(Graphics g, RectangleF rect,
        float radius, float opacity = 0.6f)
    {
        var glassColor = Color.FromArgb((int)(opacity * 255), 20, 22, 38);
        FillRoundedRect(g, rect, radius, glassColor);

        // Subtle border
        DrawRoundedRect(g, rect, radius, Color.FromArgb(20, 255, 255, 255));
    }

    // ═══════════════════════════════════════════════════════════════
    // PROGRESS & INDICATORS
    // ═══════════════════════════════════════════════════════════════

    /// <summary>Draw a smooth progress bar with rounded ends</summary>
    public static void DrawProgressBar(Graphics g, RectangleF rect,
        float progress, Color trackColor, Color fillColor, float radius = -1)
    {
        if (radius < 0) radius = rect.Height / 2f;
        progress = Math.Clamp(progress, 0f, 1f);

        // Track
        FillRoundedRect(g, rect, radius, trackColor);

        // Fill
        if (progress > 0.01f)
        {
            var fillRect = new RectangleF(rect.X, rect.Y,
                rect.Width * progress, rect.Height);
            if (fillRect.Width >= radius * 2)
            {
                FillRoundedRectGradient(g, fillRect, radius,
                    fillColor, Color.FromArgb(
                        fillColor.R + 30 > 255 ? 255 : fillColor.R + 30,
                        fillColor.G + 20 > 255 ? 255 : fillColor.G + 20,
                        fillColor.B + 10 > 255 ? 255 : fillColor.B + 10),
                    LinearGradientMode.Horizontal);
            }
            else
            {
                FillRoundedRect(g, fillRect, radius, fillColor);
            }
        }
    }


    /// <summary>Draw a circular progress arc (like the splash logo ring)</summary>
    public static void DrawProgressArc(Graphics g, RectangleF bounds,
        float progress, float rotation, Color trackColor, Color fillColor,
        float thickness = 2.5f)
    {
        // Track ring
        using var trackPen = new Pen(trackColor, thickness);
        g.DrawEllipse(trackPen, bounds);

        // Progress arc
        if (progress > 0.005f)
        {
            float sweepAngle = progress * 360f;
            using var fillPen = new Pen(fillColor, thickness)
            {
                StartCap = LineCap.Round,
                EndCap = LineCap.Round
            };
            g.DrawArc(fillPen, bounds, rotation - 90f, sweepAngle);
        }
    }

    /// <summary>Draw a pulsing dot indicator</summary>
    public static void DrawPulsingDot(Graphics g, float cx, float cy,
        float baseSize, float pulse, Color color)
    {
        float size = baseSize + pulse * 4f;
        int alpha = (int)(255 * (1f - pulse * 0.5f));

        g.FillEllipse(new SolidBrush(Color.FromArgb(
            (int)(alpha * 0.3f), color)), cx - size, cy - size, size * 2, size * 2);
        g.FillEllipse(new SolidBrush(Color.FromArgb(alpha, color)),
            cx - baseSize, cy - baseSize, baseSize * 2, baseSize * 2);
    }

    // ═══════════════════════════════════════════════════════════════
    // TEXT DRAWING HELPERS
    // ═══════════════════════════════════════════════════════════════

    private static readonly StringFormat CenterFormat = new()
    {
        Alignment = StringAlignment.Center,
        LineAlignment = StringAlignment.Center
    };

    private static readonly StringFormat LeftFormat = new()
    {
        Alignment = StringAlignment.Near,
        LineAlignment = StringAlignment.Center
    };

    /// <summary>Draw text centered in a rectangle</summary>
    public static void DrawTextCentered(Graphics g, string text, Font font,
        Color color, RectangleF rect)
    {
        using var brush = new SolidBrush(color);
        g.DrawString(text, font, brush, rect, CenterFormat);
    }

    /// <summary>Draw text at a point (horizontally centered)</summary>
    public static void DrawTextCenter(Graphics g, string text, Font font,
        Color color, float x, float y)
    {
        using var brush = new SolidBrush(color);
        var sf = new StringFormat { Alignment = StringAlignment.Center };
        g.DrawString(text, font, brush, x, y, sf);
    }

    /// <summary>Draw text left-aligned in a rectangle</summary>
    public static void DrawTextLeft(Graphics g, string text, Font font,
        Color color, RectangleF rect)
    {
        using var brush = new SolidBrush(color);
        g.DrawString(text, font, brush, rect, LeftFormat);
    }


    // ═══════════════════════════════════════════════════════════════
    // ICONS (Vector-style using GDI+ paths)
    // ═══════════════════════════════════════════════════════════════

    /// <summary>Draw a checkmark icon</summary>
    public static void DrawCheckmark(Graphics g, RectangleF bounds, Color color,
        float thickness = 2f)
    {
        using var pen = new Pen(color, thickness)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round,
            LineJoin = LineJoin.Round
        };
        float x = bounds.X, y = bounds.Y, w = bounds.Width, h = bounds.Height;
        g.DrawLines(pen, new PointF[]
        {
            new(x + w * 0.18f, y + h * 0.52f),
            new(x + w * 0.40f, y + h * 0.72f),
            new(x + w * 0.82f, y + h * 0.28f)
        });
    }

    /// <summary>Draw an X/close icon</summary>
    public static void DrawCloseIcon(Graphics g, RectangleF bounds, Color color,
        float thickness = 1.5f)
    {
        using var pen = new Pen(color, thickness)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round
        };
        float m = bounds.Width * 0.25f;
        g.DrawLine(pen, bounds.X + m, bounds.Y + m,
            bounds.Right - m, bounds.Bottom - m);
        g.DrawLine(pen, bounds.Right - m, bounds.Y + m,
            bounds.X + m, bounds.Bottom - m);
    }

    /// <summary>Draw a chevron (right arrow)</summary>
    public static void DrawChevronRight(Graphics g, RectangleF bounds,
        Color color, float thickness = 1.5f)
    {
        using var pen = new Pen(color, thickness)
        {
            StartCap = LineCap.Round,
            EndCap = LineCap.Round,
            LineJoin = LineJoin.Round
        };
        float x = bounds.X, y = bounds.Y, w = bounds.Width, h = bounds.Height;
        g.DrawLines(pen, new PointF[]
        {
            new(x + w * 0.35f, y + h * 0.2f),
            new(x + w * 0.65f, y + h * 0.5f),
            new(x + w * 0.35f, y + h * 0.8f)
        });
    }

    /// <summary>Draw a settings gear icon (simplified)</summary>
    public static void DrawGearIcon(Graphics g, RectangleF bounds, Color color,
        float thickness = 1.5f)
    {
        using var pen = new Pen(color, thickness);
        float cx = bounds.X + bounds.Width / 2f;
        float cy = bounds.Y + bounds.Height / 2f;
        float inner = bounds.Width * 0.2f;
        float outer = bounds.Width * 0.4f;

        // Center circle
        g.DrawEllipse(pen, cx - inner, cy - inner, inner * 2, inner * 2);

        // Gear teeth (6 lines)
        for (int i = 0; i < 6; i++)
        {
            float angle = i * 60f * MathF.PI / 180f;
            g.DrawLine(pen,
                cx + inner * MathF.Cos(angle), cy + inner * MathF.Sin(angle),
                cx + outer * MathF.Cos(angle), cy + outer * MathF.Sin(angle));
        }
    }
}
