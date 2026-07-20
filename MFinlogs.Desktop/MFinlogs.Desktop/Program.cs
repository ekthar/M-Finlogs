using MFinlogs.Desktop;
using MFinlogs.Desktop.Config;
using MFinlogs.Desktop.LocalServer;

namespace MFinlogs.Desktop;

internal static class Program
{
    public static AppConfig Config { get; private set; } = null!;
    public static LocalApiServer? LocalServer { get; private set; }

    [STAThread]
    static void Main()
    {
        // High DPI support
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        // Load configuration
        Config = AppConfig.Load();

        // Show splash screen
        var splash = new SplashForm();
        splash.Show();
        Application.DoEvents();

        // Initialize local server for offline mode
        if (Config.IsOfflineMode)
        {
            try
            {
                splash.UpdateStatus("Initializing local database...");
                Application.DoEvents();

                // Initialize SQLite database
                SqliteDb.Initialize(Config.DatabasePath);

                splash.UpdateStatus("Starting local server...");
                Application.DoEvents();

                // Start embedded API server
                LocalServer = new LocalApiServer(Config.LocalPort, Config.DatabasePath);
                LocalServer.Start();
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"Failed to start offline mode:\n{ex.Message}\n\nSwitching to online mode.",
                    "M-Finlogs",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                Config.Mode = "online";
            }
        }

        splash.UpdateStatus("Loading application...");
        Application.DoEvents();

        // Small delay for splash visibility
        Thread.Sleep(1500);

        // Create and show main form
        var mainForm = new MainForm();

        splash.Close();
        splash.Dispose();

        Application.Run(mainForm);
    }

    /// <summary>
    /// Clean shutdown of background services
    /// </summary>
    public static void Shutdown()
    {
        try
        {
            LocalServer?.Stop();
        }
        catch { }

        Config.Save();
    }
}
