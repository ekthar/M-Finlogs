using MFinlogs.Desktop;
using MFinlogs.Desktop.Config;
using MFinlogs.Desktop.LocalServer;
using MFinlogs.Desktop.UI;

namespace MFinlogs.Desktop;

internal static class Program
{
    public static AppConfig Config { get; private set; } = null!;
    public static LocalApiServer? LocalServer { get; private set; }

    [STAThread]
    static void Main(string[] args)
    {
        // High DPI support
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        // Initialize design system fonts
        DesignTokens.Typography.Initialize();

        // Single instance check
        using var mutex = new Mutex(true, "MFinlogs_SingleInstance", out bool isNew);
        if (!isNew)
        {
            NativeMethods.BroadcastActivateMessage();
            return;
        }

        // Load configuration
        Config = AppConfig.Load();

        // Handle --minimized flag (auto-start in tray)
        bool startMinimized = args.Contains("--minimized");

        // Handle --restore flag (backup file association)
        var restoreIdx = Array.IndexOf(args, "--restore");
        if (restoreIdx >= 0 && restoreIdx + 1 < args.Length)
        {
            var path = args[restoreIdx + 1];
            if (File.Exists(path))
            {
                try { SqliteDb.Initialize(Config.DatabasePath); SqliteDb.Restore(path); }
                catch { }
            }
        }

        // === FIRST-RUN SETUP ===
        // Show setup wizard if URL is empty (first time or reset)
        if (Config.NeedsSetup)
        {
            var setup = new SetupForm(isFirstRun: true);
            Application.Run(setup);

            if (!setup.Completed)
            {
                // User closed without completing — exit
                return;
            }

            // Reload config after setup saved it
            Config = AppConfig.Load();
        }

        // === FULLSCREEN SPLASH ===
        SplashForm? splash = null;
        if (!startMinimized)
        {
            splash = new SplashForm();
            splash.Show();
            Application.DoEvents();
        }

        // === INITIALIZE LOCAL SERVER (offline/hybrid) ===
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
                Application.DoEvents();
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"Failed to start local server:\n{ex.Message}\n\nSwitching to online mode.",
                    "M-Finlogs", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                Config.Mode = "online";
                Config.Save();
            }
        }
        else
        {
            // Online mode — just simulate progress for splash visual
            splash?.UpdateStatus("Connecting");
            splash?.SetProgress(0.6f);
            Application.DoEvents();
            Thread.Sleep(600);
            splash?.SetProgress(0.9f);
            Application.DoEvents();
        }

        // === CREATE MAIN FORM ===
        splash?.UpdateStatus("Ready");
        splash?.SetProgress(1.0f);
        Application.DoEvents();
        Thread.Sleep(400);

        var mainForm = new MainForm();

        if (startMinimized)
        {
            mainForm.WindowState = FormWindowState.Minimized;
            mainForm.ShowInTaskbar = false;
            mainForm.Visible = false;
        }

        // === FADE OUT SPLASH ===
        if (splash != null)
        {
            splash.FadeOutAndClose();
            // Wait for fade to finish
            while (splash.Visible)
            {
                Application.DoEvents();
                Thread.Sleep(16);
            }
            splash.Dispose();
            splash = null;
        }

        // === RUN APP ===
        Application.Run(mainForm);
    }

    /// <summary>
    /// Open the settings dialog (called from tray menu or Ctrl+,)
    /// </summary>
    public static void OpenSettings(Form owner)
    {
        var setup = new SetupForm(isFirstRun: false);
        setup.ShowDialog(owner);

        if (setup.Completed)
        {
            // Reload config
            Config = AppConfig.Load();
        }
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
