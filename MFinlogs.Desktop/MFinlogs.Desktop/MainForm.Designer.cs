namespace MFinlogs.Desktop;

partial class MainForm
{
    private System.ComponentModel.IContainer components = null;

    protected override void Dispose(bool disposing)
    {
        if (disposing && (components != null))
        {
            components.Dispose();
        }
        base.Dispose(disposing);
    }

    private void InitializeComponent()
    {
        this.components = new System.ComponentModel.Container();
        this.SuspendLayout();

        // MainForm
        this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
        this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
        this.ClientSize = new System.Drawing.Size(1280, 800);
        this.MinimumSize = new System.Drawing.Size(1024, 600);
        this.Name = "MainForm";
        this.Text = "M-Finlogs";
        this.DoubleBuffered = true;

        // Set icon
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "finlogs.ico");
        if (File.Exists(iconPath))
        {
            try
            {
                this.Icon = new Icon(iconPath);
            }
            catch { }
        }

        this.ResumeLayout(false);
    }
}
