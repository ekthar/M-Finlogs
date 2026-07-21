using MFinlogs.Desktop;
using MFinlogs.Desktop.Config;
using MFinlogs.Desktop.LocalServer;

namespace MFinlogs.Desktop;

internal static class Program
{
    public static AppConfig Config { get; private set; } = null!;
    public static LocalApiServer? LocalServer { get; private set; }
    private static SplashForm? splash;

    [STAThread]
    static void Main(string[] args)
    {
        // High DPI support
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        // Single instance check (prevent multiple windows)
        using var mutex = new Mutex(true, "MFinlogs_SingleInstance", out bool isNew);
        if (!isNew)
        {
            // Another instance is running — activate it instead
            NativeMethods.BroadcastActivateMessage();
            return;
        }

        // Load configuration
        Config = AppConfig.Load();

        // Handle --minimized flag (auto-start in tray)
        bool startMinimized = args.Contains("--minimized");

        // Handle --restore flag (restore backup from file association)
        var restoreFile = args.FirstOrDefault(a => a.StartsWith("--restore"));
        if (restoreFile != null)
        {
            var path = args.SkipWhile(a => a == "--restore").FirstOrDefault() ?? "";
            if (File.Exists(path))
            {
                try { SqliteDb.Initialize(Config.DatabasePath); SqliteDb.Restore(path); }
                catch { }
            }
        }

        // Show splash screen (unless starting minimized)
        if (!startMinimized)
        {
            splash = new SplashForm();
            splash.Show();
            Application.DoEvents();
        }

        // Initialize local server for offline/hybrid mode
        if (Config.NeedsLocalServer)
        {
            try
            {
                splash?.UpdateStatus("Initializing database");
                splash?.SetProgress(0.2f);
                Application.DoEvents();

                SqliteDb.Initialize(Config.DatabasePath);

                splash?.UpdateStatus("Starting local server");
                splash?.SetProgress(0.5f);
                Application.DoEvents();

                LocalServer = new LocalApiServer(Config.LocalPort, Config.DatabasePath);
                LocalServer.Start();

                splash?.SetProgress(0.8f);
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"Failed to start local server:\n{ex.Message}\n\nSwitching to online mode.",
                    "M-Finlogs", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                Config.Mode = "online";
            }
        }

        splash?.UpdateStatus("Loading");
        splash?.SetProgress(0.9f);
        Application.DoEvents();

        // Brief pause so splash is visible
        Thread.Sleep(800);

        // Create main form
        var mainForm = new MainForm();

        if (startMinimized)
        {
            mainForm.WindowState = FormWindowState.Minimized;
            mainForm.ShowInTaskbar = false;
            mainForm.Visible = false;
        }

        // Fade out splash
        if (splash != null)
        {
            splash.SetProgress(1.0f);
            splash.FadeOutAndClose();
            // Wait for fade to complete
            while (splash.Visible)
            {
                Application.DoEvents();
                Thread.Sleep(16);
            }
            splash.Dispose();
            splash = null;
        }

        Application.Run(mainForm);
    }

    /// <summary>
    /// Clean shutdown of background services
    /// </summary>
    public static void Shutdown()
    {
        try { LocalServer?.Stop(); } catch { }
        Config.Save();
    }
}

/// <summary>
/// Native interop for single-instance activation
/// </summary>
internal static class NativeMethods
{
    private const int HWND_BROADCAST = 0xFFFF;
    private static readonly int WM_SHOWMFINLOGS = RegisterWindowMessage("WM_SHOWMFINLOGS");

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern bool PostMessage(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam);

    [System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
    private static extern int RegisterWindowMessage(string message);

    public static void BroadcastActivateMessage()
    {
        PostMessage((IntPtr)HWND_BROADCAST, WM_SHOWMFINLOGS, IntPtr.Zero, IntPtr.Zero);
    }

    public static int ActivateMessage => WM_SHOWMFINLOGS;
}
