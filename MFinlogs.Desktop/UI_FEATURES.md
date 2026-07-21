# M-Finlogs Desktop — UI/UX Feature Specification

> This document describes the current UI features, screens, animations, and interaction patterns of the M-Finlogs .NET 8 WinForms desktop application. Use this as a reference for understanding, extending, or redesigning the app.

---

## Architecture Overview

```
.NET 8 WinForms App + WebView2
├── SetupForm     (First-run wizard — collects user config)
├── SplashForm    (Fullscreen animated loading screen)
├── MainForm      (WebView2 browser wrapping the web app)
├── TrayManager   (System tray icon + context menu)
├── GlobalHotkey  (Ctrl+Shift+F system-wide shortcut)
└── ProtocolHandler (mfinlogs:// deep link protocol)
```

**Tech Stack:** .NET 8, WinForms, WebView2, GDI+ custom painting, Win32 interop

---

## Screen 1: First-Run Setup Wizard (`SetupForm.cs`)

### When it appears:
- On **first launch** (no `%LocalAppData%\MFinlogs\appsettings.json` exists)
- When `onlineUrl` is empty in config
- When user presses `Ctrl+,` or clicks "Settings" in tray menu

### Visual Design:
- **Size:** 500×520px, centered on screen
- **Shape:** Rounded corners (16px radius), borderless
- **Background:** Near-white (#FAFAFC)
- **Top accent:** 4px gradient bar (indigo #6366F1 → violet #8B5CF6)
- **Shadow:** Windows CS_DROPSHADOW class style
- **Font:** Inter Tight (bundled), fallback Segoe UI

### Layout (top to bottom):
```
┌─ 4px indigo→violet gradient accent bar ──────────────── [✕] ─┐
│                                                               │
│  Welcome to M-Finlogs          (22pt bold, near-black)       │
│  Let's set up your desktop app.  (10pt, slate gray)          │
│                                                               │
│  Web App URL                    (10pt, dark)                  │
│  ┌─────────────────────────────────────────────────────┐     │
│  │ https://m-finlogs.vercel.app                        │     │
│  └─────────────────────────────────────────────────────┘     │
│  e.g. https://m-finlogs.vercel.app or your custom domain     │
│                                                               │
│  Mode                           (10pt, dark)                  │
│  ○ Online — use web app directly (recommended)               │
│  ○ Offline — local database, no internet needed              │
│  ○ Hybrid — web UI with local database                       │
│                                                               │
│  Options                        (10pt, dark)                  │
│  □ Start with Windows                                        │
│  □ Minimize to system tray on close                          │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐     │
│  │            Get Started  →                            │     │ ← indigo button
│  └─────────────────────────────────────────────────────┘     │
└───────────────────────────────────────────────────────────────┘
```

### Interactions:
| Element | Behavior |
|---------|----------|
| URL input | Auto-selects all on focus. Auto-prepends `https://` if missing |
| Close button (✕) | On first-run: shows error "Please enter a URL". On settings mode: closes dialog |
| "Get Started" button | Validates URL, saves config, sets `Completed = true`, closes form |
| Close button hover | Gray → Red color transition |
| Button hover | Indigo #6366F1 → darker #4F52DD |
| Enter key | Submits form (AcceptButton) |

### Validation:
- Empty URL → "Please enter a URL."
- Invalid URL format → "Please enter a valid URL."
- Auto-prepends `https://` if scheme is missing

---

## Screen 2: Fullscreen Animated Splash (`SplashForm.cs`)

### When it appears:
- After setup completes (or on every launch if config exists)
- NOT shown if `--minimized` flag is passed

### Visual Design:
- **Size:** Fullscreen (fills entire primary monitor)
- **Background:** Vertical gradient (#08080F → #10101F, near-black to dark navy)
- **Animation:** 60fps custom GDI+ paint loop (16ms timer interval)
- **Font:** Inter Tight Bold 42pt (title), 14pt (subtitle), 10pt (status)

### Visual Elements:

#### 1. Floating Particles (35 total)
- **Symbols:** ₹, $, %, ∑, △, ◇, ≡, ⊞, ◈, ⟐
- **Behavior:** Drift upward at varying speeds (0.3–1.0 px/frame), horizontal sine-wave drift
- **Opacity:** Very faint (3%–9% alpha)
- **Size:** 12–22pt
- **Recycling:** When a particle exits top, it respawns at bottom with random X

#### 2. Ambient Glow Orbs (2)
- Large radial gradient circles (400–500px diameter)
- Colors: Indigo (#6366F1) and Violet (#8B5CF6)
- Very low alpha (6–8) — subtle atmospheric lighting
- Positioned off-center to create depth

#### 3. Logo Circle (center)
- **Size:** 100px diameter (after animation completes)
- **Fill:** Linear gradient (indigo → violet, diagonal)
- **Content:** Letter "M" in white, bold, 36pt
- **Animation:** Springs from 0% to 100% scale using spring physics: `scale += (1.0 - scale) * 0.06`
- **Glow:** 12px outer glow ring (20% alpha indigo)

#### 4. Progress Arc (around logo)
- **Shape:** Circle outline (logo size + 28px)
- **Track:** 2px white at 30% alpha (full circle)
- **Fill:** Arc stroke in indigo, 2.5px, round caps
- **Sweep:** `progress * 360°` (0→360 as loading completes)
- **Rotation:** Arc start angle rotates at +3°/frame (creates spinner effect)

#### 5. Title Text
- **Text:** "M-Finlogs"
- **Font:** Inter Tight Bold 42pt
- **Color:** White, fades in with `contentOpacity` (0→1)
- **Position:** Centered, below logo

#### 6. Subtitle (typing effect)
- **Text:** "Financial Accounting"
- **Font:** Inter Tight Regular 14pt
- **Color:** Muted slate (70% of content opacity)
- **Animation:** Characters appear one by one (60ms interval), starts after 800ms delay

#### 7. Status Text
- **Text:** "Starting..." / "Connecting..." / "Ready"
- **Animated dots:** Cycles through 0–3 dots (tied to arc angle)
- **Color:** Very muted (50% of content opacity)

#### 8. Version (bottom center)
- **Text:** "v1.0.0"
- **Color:** 40% alpha white
- **Position:** 40px from bottom edge

### Animation Timeline:
```
0.0s   Form appears fullscreen, particles already floating
0.0s   Logo starts scaling from 0% (spring animation)
0.5s   Logo reaches ~80% → content starts fading in
0.8s   Subtitle typing begins (letter by letter)
1.0s   Status shows "Connecting..."
1.5s   Progress arc at ~50%
2.0s   Progress at ~85% (eases out, slows down)
2.5s   Program.cs sets progress=1.0, calls FadeOutAndClose()
2.5-3.0s  Form opacity reduces 0.05/frame (1.0→0.0)
3.0s   Form closes, MainForm appears
```

### Fade-Out:
- `this.Opacity` decreases by 0.05 per frame (16ms)
- Takes ~320ms (20 frames) to fully disappear
- Form closes itself when opacity reaches 0

---

## Screen 3: Main Application Window (`MainForm.cs`)

### Visual Design:
- **Window style:** Native Windows title bar (`FormBorderStyle.Sizable`)
- **Title:** "M-Finlogs" with app icon
- **Client area:** 100% WebView2 (no custom chrome, no overlap)
- **Default size:** 1280×800px
- **Minimum size:** 1024×600px
- **Position:** Restored from saved config, or centered on screen

### Why native title bar:
The web app (Next.js) has its own navigation bar at the top. Any custom title bar creates overlap/conflict. The native Windows title bar:
- Sits ABOVE the WebView (handled by OS)
- Zero overlap with web content
- Provides standard minimize/maximize/close
- Supports Windows Snap, drag, resize natively

### WebView2 Configuration:
| Setting | Value |
|---------|-------|
| Status bar | Disabled |
| Context menus | Enabled |
| Zoom control | Enabled |
| DevTools | Enabled |
| Service workers | Cleared on startup |
| New window requests | Handled in-app (no popups) |

### Keyboard Shortcuts:
| Shortcut | Action |
|----------|--------|
| `F11` | Toggle borderless fullscreen |
| `F5` | Reload page |
| `Ctrl+P` | Print (browser print dialog) |
| `Ctrl+Shift+D` | Open DevTools |
| `Ctrl+Q` | Quit application |
| `Ctrl+,` | Open Settings dialog |
| `Ctrl+Shift+F` | (Global) Bring app to foreground from anywhere |

### Fullscreen Mode (F11):
- Saves current `FormBorderStyle` and `WindowState`
- Sets `FormBorderStyle.None` + `WindowState.Maximized`
- Web app fills entire screen edge-to-edge
- Press F11 again to restore

### Window State Persistence:
- On close/minimize: saves Width, Height, X, Y, IsMaximized to config
- On next launch: restores exact position and size
- If maximized when closed: opens maximized

---

## System Tray (`TrayManager.cs`)

### Tray Icon:
- Custom `finlogs.ico` (fallback: SystemIcons.Application)
- Tooltip: "M-Finlogs"
- Double-click: Show/activate window
- Right-click: Context menu

### Context Menu Items:
```
┌──────────────────────────────────┐
│ ● Open M-Finlogs (bold)         │
├──────────────────────────────────┤
│ ▶ Mode                          │
│   ├ ✓ Online (Cloud DB)         │
│   ├   Offline (Local DB)        │
│   └   Hybrid (Online UI + Local)│
│ Save Page as PDF                 │
├──────────────────────────────────┤
│ Check for Updates                │
│ Settings                         │
├──────────────────────────────────┤
│ Exit                             │
└──────────────────────────────────┘
```

### Tray Behaviors:
| Action | Result |
|--------|--------|
| Close button (X) on window | Minimizes to tray (if configured) |
| Minimize to taskbar | Hides to tray (if configured) |
| "Exit" from tray | Full application exit |
| Balloon notification | "Minimized to system tray" on hide |

---

## Global Hotkey (`GlobalHotkey.cs`)

- **Shortcut:** `Ctrl+Shift+F`
- **Scope:** System-wide (works even when app is in tray)
- **Action:** Shows window, brings to front, activates
- **Implementation:** Win32 `RegisterHotKey` API, handled in `WndProc`
- **Cleanup:** Unregistered on form close via `Dispose()`

---

## Protocol Handler (`ProtocolHandler.cs`)

- **Protocol:** `mfinlogs://`
- **Registration:** HKCU registry (auto-registered on first launch)
- **Usage:** `mfinlogs://dashboard`, `mfinlogs://reports/ledger?party=ABC`
- **Behavior:** Opens app (or activates if running) and navigates WebView to the path

---

## Single Instance Enforcement

- **Mechanism:** Named `Mutex` ("MFinlogs_SingleInstance")
- **If second instance launched:** Broadcasts `WM_SHOWMFINLOGS` registered message
- **First instance receives:** Shows from tray and activates
- **No duplicate windows** possible

---

## Modes of Operation

| Mode | WebView URL | Database | Use Case |
|------|-------------|----------|----------|
| **Online** | User-configured URL (e.g. Vercel) | Cloud (via web app) | Standard use, always up-to-date |
| **Offline** | `http://localhost:8080` | Local SQLite | No internet, fully standalone |
| **Hybrid** | User-configured URL | Local SQLite (API intercepted) | Cloud UI + local data privacy |

### Hybrid Mode — API Interception:
JavaScript injected via `AddScriptToExecuteOnDocumentCreatedAsync`:
- Overrides `window.fetch()` — redirects `/api/*` to localhost
- Overrides `XMLHttpRequest.open()` — same redirect
- Allows cloud-hosted UI to use local SQLite database

---

## Configuration (`AppConfig.cs`)

**File location:** `%LocalAppData%\MFinlogs\appsettings.json`

```json
{
  "mode": "online",
  "onlineUrl": "https://m-finlogs.vercel.app",
  "localPort": 8080,
  "supabaseUrl": "",
  "supabaseKey": "",
  "autoSync": true,
  "syncInterval": 300,
  "autoStart": false,
  "minimizeToTray": true,
  "theme": "light",
  "windowWidth": 1280,
  "windowHeight": 800,
  "windowX": 100,
  "windowY": 50,
  "isMaximized": false
}
```

### Config Load Priority:
1. `%LocalAppData%\MFinlogs\appsettings.json` (user-specific, persists across updates)
2. `appsettings.json` next to the .exe (bundled default — copied to #1 on first run)

---

## Auto-Updater (`AutoUpdater.cs`)

- Checks GitHub releases API on startup (silent)
- Compares current assembly version vs. latest tag
- If newer version available: shows dialog with version info + download link
- "Check for Updates" in tray menu: forces non-silent check
- Also handles auto-start registry (`HKCU\...\Run`)

---

## Fonts & Assets

| Asset | Location | Usage |
|-------|----------|-------|
| `InterTight-Bold.ttf` | `Assets/` | Titles, buttons, logo text |
| `InterTight-Regular.ttf` | `Assets/` | Body text, labels, status |
| `finlogs.ico` | `Assets/` | Window icon, tray icon, taskbar |
| Segoe MDL2 Assets | System font | Window control button icons (if ever used) |
| Segoe UI | System font | Fallback if Inter Tight not found |

---

## Color Palette

| Name | Hex | Usage |
|------|-----|-------|
| Indigo | #6366F1 | Accent, buttons, progress, logo |
| Violet | #8B5CF6 | Secondary accent, gradients |
| Near-black | #08080F | Splash background top |
| Dark navy | #10101F | Splash background bottom |
| White | #FAFAFC | Setup form background |
| Slate gray | #64748B | Muted text |
| Red | #DC2626 | Error text, close hover |
| Green | #22C55E | Online mode indicator |
| Orange | #FB923C | Offline mode indicator |

---

## Command-Line Flags

| Flag | Effect |
|------|--------|
| `--minimized` | Start in tray (no window shown) |
| `--restore <path>` | Restore SQLite backup from file |
| `--url <mfinlogs://...>` | Navigate to deep link on launch |

---

## Platform Requirements

- Windows 10 version 1809+ (64-bit)
- .NET 8 Runtime (self-contained build includes it)
- WebView2 Runtime (prompted to install if missing)
- ~150MB disk space (self-contained single-file publish)
