# M-Finlogs Desktop

A .NET 8 WinForms application with WebView2 that wraps the M-Finlogs web accounting application. Supports two modes: **Online** (cloud-connected) and **Offline** (fully standalone with local SQLite database).

## Features

- **Dual Mode Operation**: Online mode connects to the live web app; Offline mode runs entirely locally
- **WebView2 Integration**: Full web app experience with native desktop features
- **Custom Title Bar**: Theme-aware title bar with minimize/maximize/close controls
- **System Tray**: Minimize to tray, context menu with mode switching
- **Auto-Update**: Checks GitHub releases for new versions on startup
- **Keyboard Shortcuts**: F11 (fullscreen), Ctrl+Shift+D (DevTools), Ctrl+Q (quit), Ctrl+P (print)
- **Offline Database**: Full SQLite database with same schema as PostgreSQL
- **Embedded API Server**: ASP.NET Minimal API on localhost for offline mode
- **Sync Engine**: Optional background sync between local SQLite and Supabase
- **Backup/Restore**: SQLite backup export and import
- **Auto-Backup**: Automatic daily backups with rotation
- **Print & PDF**: Native print dialog and PDF export support
- **Theme Support**: Light, Dark, Deep Blue, Warm themes reflected in title bar

## Prerequisites

- Windows 10 version 1809 or later (64-bit)
- [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0) (for building)
- [WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) (auto-installed by installer)

## Project Structure

```
MFinlogs.Desktop/
├── MFinlogs.Desktop.sln
├── MFinlogs.Desktop/
│   ├── MFinlogs.Desktop.csproj
│   ├── Program.cs                  # Entry point with splash + startup logic
│   ├── MainForm.cs                 # WebView2 host with custom title bar
│   ├── MainForm.Designer.cs
│   ├── SplashForm.cs               # Branded loading screen
│   ├── TrayManager.cs              # System tray icon + context menu
│   ├── AutoUpdater.cs              # GitHub release checker
│   ├── Config/
│   │   └── AppConfig.cs            # JSON configuration model
│   ├── LocalServer/
│   │   ├── LocalApiServer.cs       # Embedded Kestrel minimal API
│   │   └── SqliteDb.cs             # SQLite schema + data access
│   ├── Sync/
│   │   └── SyncEngine.cs           # Supabase sync (push/pull)
│   ├── Assets/
│   │   └── finlogs.ico
│   └── Properties/
│       └── launchSettings.json
├── Installer/
│   └── setup.iss                   # Inno Setup installer script
└── README.md
```

## Build & Run

### Development

```bash
cd MFinlogs.Desktop
dotnet restore
dotnet run --project MFinlogs.Desktop
```

### Release Build

```bash
dotnet publish MFinlogs.Desktop/MFinlogs.Desktop.csproj \
  -c Release \
  -r win-x64 \
  --self-contained true \
  -p:PublishSingleFile=true \
  -o publish
```

### Create Installer

1. Install [Inno Setup](https://jrsoftware.org/isinfo.php)
2. Download [WebView2 Bootstrapper](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) and place in `Installer/`
3. Run: `iscc Installer/setup.iss`

## Configuration

Settings are stored at `%LocalAppData%\MFinlogs\appsettings.json`:

```json
{
  "mode": "online",
  "onlineUrl": "https://finlogs.netlify.app",
  "localPort": 8080,
  "supabaseUrl": "",
  "supabaseKey": "",
  "autoSync": true,
  "syncInterval": 300,
  "autoStart": false,
  "minimizeToTray": true,
  "theme": "light"
}
```

### Mode Options

| Setting | Description |
|---------|-------------|
| `mode: "online"` | WebView2 loads the remote URL directly |
| `mode: "offline"` | Starts local API server + loads from localhost |

## Offline Mode

When running in offline mode:
- SQLite database is created at `%LocalAppData%\MFinlogs\finlogs.db`
- Embedded HTTP server starts on `http://localhost:8080`
- All API endpoints mirror the Python/FastAPI backend
- Data is fully local — no internet required

### Supported API Endpoints (Offline)

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | /api/auth/login | User authentication |
| GET/POST | /api/transactions | List/create transactions |
| PUT/DELETE | /api/transactions/{id} | Edit/delete transaction |
| GET/POST | /api/parties | List/create parties |
| GET | /api/dashboard | Dashboard metrics |
| GET | /api/reports/ledger | Party ledger report |
| GET | /api/reports/day-book | Day book report |
| GET | /api/reports/outstanding | Outstanding balances |
| GET | /api/reports/trial-balance | Trial balance |
| GET | /api/reports/profit-loss | Profit & loss statement |
| GET | /api/reports/daily-summary | Daily summary report |
| GET/POST | /api/inventory/snapshot | Inventory data |
| GET/POST | /api/cash-in-hand | Cash management |
| GET | /api/financial-years | Financial year list |
| GET/POST | /api/users | User management |
| GET | /api/audit | Audit logs |
| POST | /api/backup | Create backup |
| POST | /api/restore | Restore from backup |

## Sync Engine (Optional)

When `supabaseUrl` and `supabaseKey` are configured, the sync engine:
1. Pushes local transactions/parties to Supabase
2. Pulls remote changes from Supabase
3. Uses last-write-wins conflict resolution
4. Runs on configurable interval (default: 5 minutes)

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| F11 | Toggle fullscreen |
| F5 | Refresh page |
| Ctrl+P | Print current page |
| Ctrl+Shift+D | Open DevTools |
| Ctrl+Q | Quit application |

## NuGet Packages

- `Microsoft.Web.WebView2` - Chromium-based web browser control
- `Microsoft.Data.Sqlite` - SQLite database provider
- `Dapper` - Lightweight ORM for SQL queries
- `System.Text.Json` - JSON serialization
- `Microsoft.AspNetCore.App` - Framework for embedded web server

## License

Part of the M-Finlogs project. See main repository for license details.
