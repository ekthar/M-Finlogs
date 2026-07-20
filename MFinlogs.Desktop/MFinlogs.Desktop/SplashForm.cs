namespace MFinlogs.Desktop;

/// <summary>
/// Branded splash screen shown during application startup
/// </summary>
public class SplashForm : Form
{
    private Label lblTitle;
    private Label lblSubtitle;
    private Label lblStatus;
    private ProgressBar progressBar;
    private Panel gradientPanel;
    private System.Windows.Forms.Timer animTimer;
    private int progressValue = 0;

    public SplashForm()
    {
        InitializeComponents();
        StartAnimation();
    }

    private void InitializeComponents()
    {
        this.SuspendLayout();

        // Form settings
        this.FormBorderStyle = FormBorderStyle.None;
        this.StartPosition = FormStartPosition.CenterScreen;
        this.Size = new Size(480, 320);
        this.ShowInTaskbar = false;
        this.TopMost = true;
        this.BackColor = Color.FromArgb(15, 23, 42); // Deep blue background

        // Make form rounded
        this.Region = CreateRoundedRegion(480, 320, 16);

        // Gradient panel background
        gradientPanel = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Color.Transparent
        };
        gradientPanel.Paint += GradientPanel_Paint;

        // Logo/Title
        lblTitle = new Label
        {
            Text = "M-Finlogs",
            Font = new Font("Segoe UI", 28f, FontStyle.Bold),
            ForeColor = Color.White,
            AutoSize = false,
            Size = new Size(480, 50),
            Location = new Point(0, 100),
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.Transparent
        };

        // Subtitle
        lblSubtitle = new Label
        {
            Text = "Financial Accounting Management",
            Font = new Font("Segoe UI", 11f, FontStyle.Regular),
            ForeColor = Color.FromArgb(148, 163, 184),
            AutoSize = false,
            Size = new Size(480, 25),
            Location = new Point(0, 155),
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.Transparent
        };

        // Status label
        lblStatus = new Label
        {
            Text = "Initializing...",
            Font = new Font("Segoe UI", 9f, FontStyle.Regular),
            ForeColor = Color.FromArgb(100, 116, 139),
            AutoSize = false,
            Size = new Size(480, 20),
            Location = new Point(0, 260),
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.Transparent
        };

        // Progress bar
        progressBar = new ProgressBar
        {
            Style = ProgressBarStyle.Continuous,
            Minimum = 0,
            Maximum = 100,
            Value = 0,
            Size = new Size(320, 4),
            Location = new Point(80, 240),
            BackColor = Color.FromArgb(30, 41, 59)
        };

        // Version label
        var lblVersion = new Label
        {
            Text = $"v{typeof(SplashForm).Assembly.GetName().Version?.ToString(3) ?? "1.0.0"}",
            Font = new Font("Segoe UI", 8f, FontStyle.Regular),
            ForeColor = Color.FromArgb(71, 85, 105),
            AutoSize = false,
            Size = new Size(480, 16),
            Location = new Point(0, 290),
            TextAlign = ContentAlignment.MiddleCenter,
            BackColor = Color.Transparent
        };

        gradientPanel.Controls.Add(lblTitle);
        gradientPanel.Controls.Add(lblSubtitle);
        gradientPanel.Controls.Add(lblStatus);
        gradientPanel.Controls.Add(progressBar);
        gradientPanel.Controls.Add(lblVersion);

        this.Controls.Add(gradientPanel);

        this.ResumeLayout(false);
    }

    private void GradientPanel_Paint(object? sender, PaintEventArgs e)
    {
        // Draw a subtle gradient overlay
        using var brush = new System.Drawing.Drawing2D.LinearGradientBrush(
            new Rectangle(0, 0, this.Width, this.Height),
            Color.FromArgb(15, 23, 42),
            Color.FromArgb(30, 41, 59),
            System.Drawing.Drawing2D.LinearGradientMode.ForwardDiagonal);
        e.Graphics.FillRectangle(brush, this.ClientRectangle);

        // Draw decorative circles (aurora effect)
        using var circleBrush1 = new SolidBrush(Color.FromArgb(15, 99, 102, 241)); // Indigo
        using var circleBrush2 = new SolidBrush(Color.FromArgb(10, 139, 92, 246)); // Violet
        e.Graphics.FillEllipse(circleBrush1, -50, -50, 200, 200);
        e.Graphics.FillEllipse(circleBrush2, 350, 200, 180, 180);
    }

    private Region CreateRoundedRegion(int width, int height, int radius)
    {
        using var path = new System.Drawing.Drawing2D.GraphicsPath();
        path.AddArc(0, 0, radius * 2, radius * 2, 180, 90);
        path.AddArc(width - radius * 2, 0, radius * 2, radius * 2, 270, 90);
        path.AddArc(width - radius * 2, height - radius * 2, radius * 2, radius * 2, 0, 90);
        path.AddArc(0, height - radius * 2, radius * 2, radius * 2, 90, 90);
        path.CloseFigure();
        return new Region(path);
    }

    private void StartAnimation()
    {
        animTimer = new System.Windows.Forms.Timer
        {
            Interval = 30
        };
        animTimer.Tick += (s, e) =>
        {
            if (progressValue < 90)
            {
                progressValue += 2;
                progressBar.Value = Math.Min(progressValue, 100);
            }
        };
        animTimer.Start();
    }

    /// <summary>
    /// Update the status text shown on the splash screen
    /// </summary>
    public void UpdateStatus(string status)
    {
        if (lblStatus.InvokeRequired)
        {
            lblStatus.Invoke(() => lblStatus.Text = status);
        }
        else
        {
            lblStatus.Text = status;
        }
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        animTimer?.Stop();
        animTimer?.Dispose();
        base.OnFormClosing(e);
    }
}
