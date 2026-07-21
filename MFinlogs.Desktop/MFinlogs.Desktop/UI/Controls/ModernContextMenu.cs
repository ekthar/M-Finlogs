using System.Drawing.Drawing2D;

namespace MFinlogs.Desktop.UI.Controls;

/// <summary>
/// Premium owner-drawn context menu with rounded corners, section headers,
/// icons, hover animations, and modern styling. Replaces the system
/// ContextMenuStrip with a floating modern panel.
/// </summary>
public class ModernContextMenu : ToolStripRenderer
{
    private readonly float _radius = DesignTokens.Radius.LG;

    public ModernContextMenu()
    {
    }

    protected override void OnRenderToolStripBackground(
        ToolStripRenderEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        var rect = new RectangleF(0, 0,
            e.ToolStrip.Width - 1, e.ToolStrip.Height - 1);

        // Shadow (simplified - just a darker border area)
        ModernRenderer.DrawShadow(g, rect, _radius, 20, 6,
            Color.FromArgb(25, 0, 0, 0));

        // Background
        ModernRenderer.FillRoundedRect(g, rect, _radius,
            DesignTokens.Colors.Surface);

        // Border
        ModernRenderer.DrawRoundedRect(g, rect, _radius,
            DesignTokens.Colors.Stroke);
    }

    protected override void OnRenderMenuItemBackground(
        ToolStripItemRenderEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        if (e.Item.Selected && e.Item.Enabled)
        {
            var rect = new RectangleF(4, 1, e.Item.Width - 8, e.Item.Height - 2);
            ModernRenderer.FillRoundedRect(g, rect, DesignTokens.Radius.SM,
                DesignTokens.Colors.Hover);
        }
    }


    protected override void OnRenderItemText(ToolStripItemTextRenderEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        var color = e.Item.Enabled
            ? DesignTokens.Colors.TextPrimary
            : DesignTokens.Colors.TextMuted;

        if (e.Item.Font.Bold)
            color = DesignTokens.Colors.TextPrimary;

        var font = e.Item.Font.Bold
            ? DesignTokens.Typography.Title
            : DesignTokens.Typography.Body;

        using var brush = new SolidBrush(color);
        float y = e.Item.ContentRectangle.Y +
            (e.Item.ContentRectangle.Height - font.Height) / 2f;
        g.DrawString(e.Item.Text, font, brush, e.TextRectangle.X, y);
    }

    protected override void OnRenderSeparator(ToolStripSeparatorRenderEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        float y = e.Item.Height / 2f;
        using var pen = new Pen(DesignTokens.Colors.StrokeLight, 1f);
        g.DrawLine(pen, 12, y, e.Item.Width - 12, y);
    }

    protected override void OnRenderItemCheck(ToolStripItemImageRenderEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        var checkRect = new RectangleF(
            e.ImageRectangle.X + 2, e.ImageRectangle.Y + 4,
            14, 14);
        ModernRenderer.DrawCheckmark(g, checkRect,
            DesignTokens.Colors.Primary500, 2f);
    }

    protected override void OnRenderArrow(ToolStripArrowRenderEventArgs e)
    {
        var g = e.Graphics;
        ModernRenderer.SetHighQuality(g);

        var rect = new RectangleF(
            e.ArrowRectangle.X, e.ArrowRectangle.Y + 4,
            12, 12);
        ModernRenderer.DrawChevronRight(g, rect,
            DesignTokens.Colors.TextTertiary);
    }

    /// <summary>
    /// Apply this renderer to a ContextMenuStrip for premium styling
    /// </summary>
    public static void Apply(ContextMenuStrip menu)
    {
        menu.Renderer = new ModernContextMenu();
        menu.BackColor = DesignTokens.Colors.Surface;
        menu.ShowImageMargin = true;
        menu.ShowCheckMargin = true;

        // Style all items
        foreach (ToolStripItem item in menu.Items)
        {
            if (item is ToolStripMenuItem menuItem)
            {
                menuItem.Padding = new Padding(8, 4, 8, 4);
                // Recurse into dropdowns
                if (menuItem.HasDropDownItems)
                {
                    menuItem.DropDown.Renderer = new ModernContextMenu();
                    menuItem.DropDown.BackColor = DesignTokens.Colors.Surface;
                }
            }
        }
    }
}
