using System.Drawing.Drawing2D;
using System.Drawing.Text;
using MFinlogs.Desktop.Config;

namespace MFinlogs.Desktop;

/// <summary>
/// First-run setup wizard. Shows on first launch (no config file exists)
/// or when user clicks "Settings" from tray menu / Ctrl+,.
/// Collects: app URL, mode, startup options.
/// </summary>
public class SetupForm : Form
{
    private TextBox txtUrl = null!;
    private RadioButton rbOnline = null!;
    private RadioButton rbOffline = null!;
    private RadioButton rbHybrid = null!;
    private CheckBox chkAutoStart = null!;
    private CheckBox chkMinimizeToTray = null!;
    private Button btnStart = null!;
    private Label lblError = null!;
    private Panel contentPanel = null!;

    // Fonts
    private Font titleFont = null!;
    private Font bodyFont = null!;
    private Font smallFont = null!;

    // Result
    public bool Completed { get; private set; } = false;

    public SetupForm(bool isFirstRun = true)
    {
        LoadFonts();
        InitializeUI(isFirstRun);
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
                titleFont = new Font(pfc.Families[0], 22f, FontStyle.Bold);
                bodyFont = new Font(pfc.Families.Length > 1 ? pfc.Families[1] : pfc.Families[0], 10f);
                smallFont = new Font(pfc.Families.Length > 1 ? pfc.Families[1] : pfc.Families[0], 8.5f);
                return;
            }
        }
        catch { }
        titleFont = new Font("Segoe UI", 22f, FontStyle.Bold);
        bodyFont = new Font("Segoe UI", 10f);
        smallFont = new Font("Segoe UI", 8.5f);
    }

    private void InitializeUI(bool isFirstRun)
    {
        // Form settings
        this.Text = "M-Finlogs Setup";
        this.FormBorderStyle = FormBorderStyle.None;
        this.StartPosition = FormStartPosition.CenterScreen;
        this.Size = new Size(500, 520);
        this.BackColor = Color.FromArgb(250, 250, 252);
        this.DoubleBuffered = true;
        this.ShowInTaskbar = true;
        this.TopMost = true;

        // Rounded corners
        this.Region = CreateRoundedRegion(500, 520, 16);
        this.Paint += SetupForm_Paint;

        // Set icon
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
            try { this.Icon = new Icon(iconPath); } catch { }

        // Content panel
        contentPanel = new Panel
        {
            Location = new Point(40, 40),
            Size = new Size(420, 440),
            BackColor = Color.Transparent
        };

        // Title
        var lblTitle = new Label
        {
            Text = isFirstRun ? "Welcome to M-Finlogs" : "Settings",
            Font = titleFont,
            ForeColor = Color.FromArgb(15, 15, 20),
            AutoSize = true,
            Location = new Point(0, 0)
        };

        // Subtitle
        var lblSubtitle = new Label
        {
            Text = isFirstRun
                ? "Let's set up your desktop app."
                : "Configure your M-Finlogs desktop app.",
            Font = bodyFont,
            ForeColor = Color.FromArgb(100, 116, 139),
            AutoSize = true,
            Location = new Point(0, 40)
        };

        // URL section
        var lblUrl = new Label
        {
            Text = "Web App URL",
            Font = bodyFont,
            ForeColor = Color.FromArgb(30, 30, 40),
            AutoSize = true,
            Location = new Point(0, 85)
        };

        txtUrl = new TextBox
        {
            Text = Program.Config.OnlineUrl,
            Font = bodyFont,
            Size = new Size(420, 36),
            Location = new Point(0, 110),
            BorderStyle = BorderStyle.FixedSingle,
            BackColor = Color.White,
            ForeColor = Color.FromArgb(23, 23, 23)
        };
        txtUrl.GotFocus += (s, e) => txtUrl.SelectAll();

        var lblUrlHint = new Label
        {
            Text = "e.g. https://m-finlogs.vercel.app or your custom domain",
            Font = smallFont,
            ForeColor = Color.FromArgb(148, 163, 184),
            AutoSize = true,
            Location = new Point(0, 138)
        };

        // Mode section
        var lblMode = new Label
        {
            Text = "Mode",
            Font = bodyFont,
            ForeColor = Color.FromArgb(30, 30, 40),
            AutoSize = true,
            Location = new Point(0, 175)
        };

        rbOnline = new RadioButton
        {
            Text = "Online — use web app directly (recommended)",
            Font = bodyFont,
            ForeColor = Color.FromArgb(50, 50, 60),
            AutoSize = true,
            Location = new Point(0, 200),
            Checked = Program.Config.IsOnlineMode || string.IsNullOrEmpty(Program.Config.Mode)
        };

        rbOffline = new RadioButton
        {
            Text = "Offline — local database, no internet needed",
            Font = bodyFont,
            ForeColor = Color.FromArgb(50, 50, 60),
            AutoSize = true,
            Location = new Point(0, 228),
            Checked = Program.Config.IsOfflineMode
        };

        rbHybrid = new RadioButton
        {
            Text = "Hybrid — web UI with local database",
            Font = bodyFont,
            ForeColor = Color.FromArgb(50, 50, 60),
            AutoSize = true,
            Location = new Point(0, 256),
            Checked = Program.Config.IsHybridMode
        };

        // Options section
        var lblOptions = new Label
        {
            Text = "Options",
            Font = bodyFont,
            ForeColor = Color.FromArgb(30, 30, 40),
            AutoSize = true,
            Location = new Point(0, 300)
        };

        chkAutoStart = new CheckBox
        {
            Text = "Start with Windows",
            Font = bodyFont,
            ForeColor = Color.FromArgb(50, 50, 60),
            AutoSize = true,
            Location = new Point(0, 325),
            Checked = Program.Config.AutoStart
        };

        chkMinimizeToTray = new CheckBox
        {
            Text = "Minimize to system tray on close",
            Font = bodyFont,
            ForeColor = Color.FromArgb(50, 50, 60),
            AutoSize = true,
            Location = new Point(0, 353),
            Checked = Program.Config.MinimizeToTray
        };

        // Error label
        lblError = new Label
        {
            Text = "",
            Font = smallFont,
            ForeColor = Color.FromArgb(220, 38, 38),
            AutoSize = true,
            Location = new Point(0, 395),
            Visible = false
        };

        // Button
        btnStart = new Button
        {
            Text = isFirstRun ? "Get Started  →" : "Save Settings",
            Font = new Font(bodyFont.FontFamily, 10.5f, FontStyle.Bold),
            Size = new Size(420, 44),
            Location = new Point(0, 415),
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(99, 102, 241),
            ForeColor = Color.White,
            Cursor = Cursors.Hand
        };
        btnStart.FlatAppearance.BorderSize = 0;
        btnStart.FlatAppearance.MouseOverBackColor = Color.FromArgb(79, 82, 221);
        btnStart.FlatAppearance.MouseDownBackColor = Color.FromArgb(67, 56, 202);
        btnStart.Click += BtnStart_Click;

        // Add all to content panel
        contentPanel.Controls.AddRange(new Control[]
        {
            lblTitle, lblSubtitle, lblUrl, txtUrl, lblUrlHint,
            lblMode, rbOnline, rbOffline, rbHybrid,
            lblOptions, chkAutoStart, chkMinimizeToTray,
            lblError, btnStart
        });

        this.Controls.Add(contentPanel);

        // Close button (top right)
        var btnClose = new Label
        {
            Text = "✕",
            Font = new Font("Segoe UI", 12f),
            ForeColor = Color.FromArgb(148, 163, 184),
            AutoSize = true,
            Location = new Point(468, 12),
            Cursor = Cursors.Hand
        };
        btnClose.Click += (s, e) =>
        {
            if (isFirstRun)
            {
                // Can't skip first-run setup — need at least a URL
                lblError.Text = "Please enter a URL to continue.";
                lblError.Visible = true;
            }
            else
            {
                this.Close();
            }
        };
        btnClose.MouseEnter += (s, e) => btnClose.ForeColor = Color.FromArgb(220, 38, 38);
        btnClose.MouseLeave += (s, e) => btnClose.ForeColor = Color.FromArgb(148, 163, 184);
        this.Controls.Add(btnClose);

        // Allow Enter key to submit
        this.AcceptButton = btnStart;
    }

    private void BtnStart_Click(object? sender, EventArgs e)
    {
        var url = txtUrl.Text.Trim();

        // Validate URL
        if (string.IsNullOrWhiteSpace(url))
        {
            ShowError("Please enter a URL.");
            return;
        }

        if (!url.StartsWith("http://") && !url.StartsWith("https://"))
        {
            url = "https://" + url;
            txtUrl.Text = url;
        }

        if (!Uri.TryCreate(url, UriKind.Absolute, out _))
        {
            ShowError("Please enter a valid URL.");
            return;
        }

        // Save config
        var config = Program.Config;
        config.OnlineUrl = url.TrimEnd('/');
        config.Mode = rbOnline.Checked ? "online" : rbOffline.Checked ? "offline" : "hybrid";
        config.AutoStart = chkAutoStart.Checked;
        config.MinimizeToTray = chkMinimizeToTray.Checked;
        config.Save();

        // Handle auto-start registry
        AutoUpdater.SetAutoStart(config.AutoStart);

        Completed = true;
        this.Close();
    }

    private void ShowError(string msg)
    {
        lblError.Text = msg;
        lblError.Visible = true;
    }

    private void SetupForm_Paint(object? sender, PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.HighQuality;

        // Subtle top accent gradient
        using var accentBrush = new LinearGradientBrush(
            new Rectangle(0, 0, this.Width, 4),
            Color.FromArgb(99, 102, 241),
            Color.FromArgb(139, 92, 246),
            LinearGradientMode.Horizontal);
        g.FillRectangle(accentBrush, 0, 0, this.Width, 4);

        // Border
        using var borderPen = new Pen(Color.FromArgb(226, 232, 240), 1f);
        using var path = CreateRoundedPath(new Rectangle(0, 0, Width - 1, Height - 1), 16);
        g.DrawPath(borderPen, path);
    }

    private Region CreateRoundedRegion(int w, int h, int r)
    {
        using var path = CreateRoundedPath(new Rectangle(0, 0, w, h), r);
        return new Region(path);
    }

    private static GraphicsPath CreateRoundedPath(Rectangle rect, int radius)
    {
        var path = new GraphicsPath();
        int d = radius * 2;
        path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        path.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        path.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        path.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        path.CloseFigure();
        return path;
    }

    // Drop shadow
    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            cp.ClassStyle |= 0x00020000; // CS_DROPSHADOW
            return cp;
        }
    }
}
