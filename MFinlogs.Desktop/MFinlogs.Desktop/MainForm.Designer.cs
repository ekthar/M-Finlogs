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
        this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
        this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
        this.ClientSize = new System.Drawing.Size(1280, 800);
        this.MinimumSize = new System.Drawing.Size(1024, 600);
        this.Name = "MainForm";
        this.Text = "M-Finlogs";
        this.DoubleBuffered = true;

        this.ResumeLayout(false);
    }
}
