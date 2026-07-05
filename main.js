const { app, BrowserWindow, ipcMain, dialog, globalShortcut } = require('electron');
const { shell } = require('electron');
const path = require('path');
const { execFile, spawn, exec } = require('child_process');
const net = require('net');
const fs = require('fs');
const http = require('http');
const crypto = require('crypto');

// --- RAM & performance optimizations ---
app.commandLine.appendSwitch('disable-accelerated-2d-canvas');
app.commandLine.appendSwitch('disable-background-timer-throttling');
app.commandLine.appendSwitch('disable-renderer-backgrounding');
app.commandLine.appendSwitch('js-flags', '--max-old-space-size=384 --expose-gc');
app.commandLine.appendSwitch('no-pings');

let autoUpdaterRef = null;
let autoUpdaterReady = false;

function sendUpdateStatus(payload) {
    if (mainWindow && mainWindow.webContents) {
        mainWindow.webContents.send('update:status', payload);
    }
}

function wireAutoUpdaterEvents() {
    if (!autoUpdaterRef || autoUpdaterReady || !mainWindow) return;
    autoUpdaterReady = true;
    autoUpdaterRef.on('checking-for-update', () => sendUpdateStatus({ type: 'checking', message: 'Checking for updates...' }));
    autoUpdaterRef.on('update-available', (info) => sendUpdateStatus({ type: 'available', message: `Update available: v${info.version}` }));
    autoUpdaterRef.on('update-not-available', () => sendUpdateStatus({ type: 'none', message: 'You are up to date.' }));
    autoUpdaterRef.on('error', (err) => sendUpdateStatus({ type: 'error', message: `Update error: ${err.message}` }));
    autoUpdaterRef.on('download-progress', (p) => {
        const percent = Math.round(p.percent || 0);
        sendUpdateStatus({ type: 'progress', message: `Downloading update... ${percent}%`, percent });
    });
    autoUpdaterRef.on('update-downloaded', () => {
        sendUpdateStatus({ type: 'downloaded', message: 'Update downloaded. Restart to install.' });
    });
}

let mainWindow;
let splashWindow;
let backendProcess = null;
let isQuitting = false;
let backendRestartAttempted = false;

function writeCrashLog(prefix, detail) {
    try {
        const logPath = path.join(app.getPath('userData'), 'main-crash.log');
        fs.appendFileSync(logPath, `\n[${new Date().toISOString()}] ${prefix}: ${detail}\n`);
    } catch (_) {}
}

process.on('uncaughtException', (err) => {
    writeCrashLog('uncaughtException', (err && err.stack) || err);
});

process.on('unhandledRejection', (reason) => {
    writeCrashLog('unhandledRejection', reason);
});

function applyTitleBarTheme(theme) {
    if (!mainWindow || process.platform !== 'win32') return;
    const mode = (theme || 'white').toLowerCase();
    let color = '#22364a';
    let symbolColor = '#eaf2ff';

    if (mode === 'dark') {
        color = '#050c14';
        symbolColor = '#f5f5f5';
    } else if (mode === 'white' || mode === 'light') {
        color = '#f3f4f6';
        symbolColor = '#111827';
    }

    mainWindow.setTitleBarOverlay({
        color,
        symbolColor,
        height: 30
    });
}

function createSplashWindow() {
    splashWindow = new BrowserWindow({
        width: 500,
        height: 300,
        frame: false,
        alwaysOnTop: true,
        transparent: true,
        resizable: false,
        show: false,
        webPreferences: {
            nodeIntegration: false,
            contextIsolation: true,
            preload: path.join(__dirname, 'preload.js')
        }
    });
    splashWindow.loadFile('splash.html');
    splashWindow.once('ready-to-show', () => splashWindow.show());
}

function createMainWindow() {
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        title: "M-Finlogs",
        autoHideMenuBar: true,
        show: false,
        backgroundColor: '#f4eedf',
        icon: path.join(__dirname, 'assets', 'finlogs.ico'),
        roundedCorners: false,
        titleBarStyle: 'hidden',
        titleBarOverlay: true,
        webPreferences: {
            nodeIntegration: false,
            contextIsolation: true,
            preload: path.join(__dirname, 'preload.js'),
            spellcheck: false,
            v8CacheOptions: 'bypassHeatCheck',
            backgroundThrottling: false
        }
    });
    mainWindow.loadFile('index.html');
    applyTitleBarTheme('white');

    mainWindow.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
    mainWindow.webContents.on('will-navigate', (event, url) => {
        if (!url.startsWith('file://')) {
            event.preventDefault();
        }
    });

    wireAutoUpdaterEvents();

    mainWindow.webContents.on('render-process-gone', (event, details) => {
        writeCrashLog('render-process-gone', JSON.stringify(details));
        if (isQuitting) return;
        dialog.showErrorBox('M-Finlogs crashed', `The application window crashed (reason: ${details.reason}). It will now restart.`);
        app.relaunch();
        app.exit(0);
    });

    mainWindow.once('ready-to-show', () => {
        if (splashWindow) splashWindow.close();
        mainWindow.show();
    });
}

function isPortFree(port, host = '127.0.0.1') {
    return new Promise((resolve) => {
        const tester = net.createServer()
            .once('error', () => resolve(false))
            .once('listening', () => tester.once('close', () => resolve(true)).close())
            .listen(port, host);
    });
}

function httpGetStatus(pathname, timeoutMs = 1500) {
    return new Promise((resolve) => {
        const req = http.get({
            hostname: '127.0.0.1',
            port: 8000,
            path: pathname,
            timeout: timeoutMs
        }, (res) => {
            res.resume();
            resolve(res.statusCode || 0);
        });
        req.on('error', () => resolve(0));
        req.on('timeout', () => {
            req.destroy();
            resolve(0);
        });
    });
}

async function isBackendResponsive(timeoutMs = 1500) {
    // Compatibility check: app now expects both core and FY endpoints.
    const companiesStatus = await httpGetStatus('/companies', timeoutMs);
    if (companiesStatus < 200 || companiesStatus >= 300) return false;

    const fyStatus = await httpGetStatus('/financial-years', timeoutMs);
    if (fyStatus < 200 || fyStatus >= 300) return false;

    return true;
}

function killPort8000() {
    return new Promise((resolve) => {
        const command = 'powershell -NoProfile -ExecutionPolicy Bypass -Command "' +
            'Get-NetTCPConnection -LocalPort 8000 -ErrorAction SilentlyContinue | ' +
            'Select-Object -ExpandProperty OwningProcess -Unique | ' +
            'ForEach-Object { Stop-Process -Id $_ -Force -ErrorAction SilentlyContinue }"';
        exec(command, () => resolve());
    });
}

function deleteServerTask() {
    return new Promise((resolve) => {
        const command = 'powershell -NoProfile -ExecutionPolicy Bypass -Command "' +
            '& "$env:WINDIR\\System32\\schtasks.exe" /Delete /TN "M-FinlogsServer" /F 2>$null; ' +
            'exit 0"';
        exec(command, () => resolve());
    });
}

function ensureJwtSecret(userDataPath) {
    const secretPath = path.join(userDataPath, 'jwt_secret.key');
    let jwtSecret = (process.env.JWT_SECRET_KEY || '').trim();

    if (jwtSecret.length >= 32) {
        return jwtSecret;
    }

    if (fs.existsSync(secretPath)) {
        const fromFile = (fs.readFileSync(secretPath, 'utf8') || '').trim();
        if (fromFile.length >= 32) {
            return fromFile;
        }
    }

    jwtSecret = crypto.randomBytes(48).toString('hex');
    fs.writeFileSync(secretPath, jwtSecret, { encoding: 'utf8', mode: 0o600 });
    return jwtSecret;
}

function readClientConfig(userDataPath) {
    try {
        const configPath = path.join(userDataPath, 'db_config.json');
        if (!fs.existsSync(configPath)) return {};
        return JSON.parse(fs.readFileSync(configPath, 'utf8') || '{}');
    } catch (_e) {
        return {};
    }
}

function isLocalApiBase(apiBase) {
    if (!apiBase || typeof apiBase !== 'string') return true;
    try {
        const parsed = new URL(/^https?:\/\//i.test(apiBase) ? apiBase : `http://${apiBase}`);
        const host = parsed.hostname.toLowerCase();
        return host === '127.0.0.1' || host === 'localhost' || host === '::1';
    } catch (_e) {
        return true;
    }
}

function getAppModePath() {
    return path.join(app.getPath('userData'), 'app_mode.json');
}

function getAppMode() {
    try {
        const modePath = getAppModePath();
        if (!fs.existsSync(modePath)) return null;
        const parsed = JSON.parse(fs.readFileSync(modePath, 'utf8') || '{}');
        if (!parsed || (parsed.mode !== 'server' && parsed.mode !== 'client')) return null;
        return parsed;
    } catch (_e) {
        return null;
    }
}

function saveAppMode(modeData) {
    fs.writeFileSync(getAppModePath(), JSON.stringify(modeData || {}, null, 2), 'utf8');
}

async function startBackend() {
    // Start backend in both dev and production mode
    const { spawn } = require('child_process');

    const userDataPath = app.getPath('userData');
    const clientConfig = readClientConfig(userDataPath);
    const appMode = getAppMode();
    if (appMode && appMode.mode === 'client') {
        console.log('App mode is "client"; skipping local backend startup.');
        return true;
    }
    if (!isLocalApiBase(clientConfig.api_base)) {
        console.log(`Remote API configured (${clientConfig.api_base}); skipping local backend startup.`);
        return true;
    }

    let jwtSecret = '';
    try {
        jwtSecret = ensureJwtSecret(userDataPath);
    } catch (e) {
        console.error('Failed to initialize JWT secret:', e);
        return false;
    }

    const envVars = {
        ...process.env,
        FINLOGS_CONFIG_DIR: userDataPath,
        JWT_SECRET_KEY: jwtSecret
    };

    killBackend();

    let portFree = await isPortFree(8000, '127.0.0.1');
    if (!portFree) {
        if (await isBackendResponsive(1500)) {
            console.log('Using existing responsive backend on port 8000.');
            return true;
        }
        console.warn('Backend not started: port 8000 is in use by a non-responsive process.');
        return false;
    }

    if (app.isPackaged) {
        // Production: use compiled backend.exe
        const backendPath = path.join(process.resourcesPath, 'backend.exe');
        if (!fs.existsSync(backendPath)) {
            const msg = `Backend executable not found at: ${backendPath}\n` +
                `Build backend.exe (PyInstaller) and place it in dist/backend.exe before packaging.`;
            console.error(msg);
            try {
                const logPath = path.join(app.getPath('userData'), 'backend.log');
                fs.appendFileSync(logPath, `\n[${new Date().toISOString()}] ${msg}\n`);
            } catch (_) {}
            dialog.showErrorBox('Backend missing', msg);
            return false;
        }
        console.log("Starting backend:", backendPath);
        const logPath = path.join(app.getPath('userData'), 'backend.log');
        const logStream = fs.createWriteStream(logPath, { flags: 'a' });
        logStream.write(`\n[${new Date().toISOString()}] Starting backend: ${backendPath}\n`);

        backendProcess = spawn(backendPath, [], {
            env: envVars,
            cwd: process.resourcesPath,
            windowsHide: true
        });

        backendProcess.stdout.on('data', (data) => logStream.write(data));
        backendProcess.stderr.on('data', (data) => logStream.write(data));
        backendProcess.on('exit', (code) => {
            logStream.write(`\n[${new Date().toISOString()}] Backend exit: ${code}\n`);
            if (code !== 0 && !isQuitting && !backendRestartAttempted) {
                backendRestartAttempted = true;
                logStream.write(`\n[${new Date().toISOString()}] Backend exited unexpectedly, attempting one auto-restart in 2s...\n`);
                setTimeout(() => {
                    if (!isQuitting) startBackend();
                }, 2000);
            }
        });
        return true;
    } else {
        // Development: start uvicorn server
        // Respect FINLOGS_HOST so dev mode behaves the same as the packaged
        // backend.exe when binding for LAN access (e.g. server mode).
        const bindHost = envVars.FINLOGS_HOST || '127.0.0.1';
        console.log(`Dev Mode: Starting uvicorn backend server on ${bindHost}...`);
        backendProcess = spawn('python', [
            '-m', 'uvicorn', 
            'backend:app', 
            '--host', bindHost, 
            '--port', '8000'
        ], {
            cwd: __dirname,
            shell: false,
            env: envVars
        });
        
        backendProcess.stdout.on('data', (data) => {
            console.log(`Backend: ${data}`);
        });
        
        backendProcess.stderr.on('data', (data) => {
            console.error(`Backend Error: ${data}`);
        });
        return true;
    }
}

function killBackend() {
    if (backendProcess) {
        backendProcess.kill();
        backendProcess = null;
    }
}

app.whenReady().then(async () => {
    await startBackend(); // Start Python Server
    createSplashWindow();
    createMainWindow();

    globalShortcut.register('Alt+E', () => {
        if (!mainWindow) return;
        if (mainWindow.webContents.isDevToolsOpened()) {
            mainWindow.webContents.closeDevTools();
        } else {
            mainWindow.webContents.openDevTools({ mode: 'detach' });
        }
    });
}).catch((err) => {
    console.error('App startup error:', err);
});

app.on('before-quit', () => {
    isQuitting = true;
});

app.on('child-process-gone', (event, details) => {
    writeCrashLog('child-process-gone', JSON.stringify(details));
});

app.on('will-quit', () => {
    globalShortcut.unregisterAll();
    killBackend();
});

app.on('window-all-closed', () => {
    killBackend();
    if (process.platform !== 'darwin') app.quit();
});

// IPC
ipcMain.handle('dialog:save', async (event, defaultName) => {
    const { canceled, filePath } = await dialog.showSaveDialog({
        defaultPath: defaultName,
        filters: [{ name: 'Excel Files', extensions: ['xlsx'] }]
    });
    if (canceled) return null;
    return filePath;
});

ipcMain.handle('dialog:saveBackup', async (event, defaultName) => {
    const { canceled, filePath } = await dialog.showSaveDialog({
        defaultPath: defaultName,
        filters: [{ name: 'SQL Server Backup', extensions: ['bak'] }]
    });
    if (canceled) return null;
    return filePath;
});

ipcMain.handle('dialog:open', async () => {
    const { canceled, filePaths } = await dialog.showOpenDialog({
        properties: ['openFile'],
        filters: [{ name: 'Backup Files', extensions: ['db'] }]
    });
    if (canceled || !filePaths.length) return null;
    return filePaths[0];
});

ipcMain.handle('dialog:openBackup', async () => {
    const { canceled, filePaths } = await dialog.showOpenDialog({
        properties: ['openFile'],
        filters: [{ name: 'SQL Server Backup', extensions: ['bak'] }]
    });
    if (canceled || !filePaths.length) return null;
    return filePaths[0];
});

ipcMain.handle('app:getUserDataPath', async () => {
    return app.getPath('userData');
});

ipcMain.handle('config:readClient', async () => {
    try {
        const configPath = path.join(app.getPath('userData'), 'db_config.json');
        if (!fs.existsSync(configPath)) return {};
        const raw = fs.readFileSync(configPath, 'utf8');
        return JSON.parse(raw || '{}');
    } catch (_e) {
        return {};
    }
});

ipcMain.handle('config:writeClient', async (_event, configJson) => {
    try {
        const configPath = path.join(app.getPath('userData'), 'db_config.json');
        fs.writeFileSync(configPath, JSON.stringify(configJson || {}, null, 2), 'utf8');
        return { success: true };
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('app:getVersion', async () => {
    return app.getVersion();
});

ipcMain.handle('app:getAppMode', async () => {
    return getAppMode();
});

ipcMain.handle('app:saveAppMode', async (event, modeData) => {
    try {
        saveAppMode(modeData);
        return { success: true };
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('update:check', async () => {
    try {
        if (!autoUpdaterRef) {
            try {
                ({ autoUpdater: autoUpdaterRef } = require('electron-updater'));
                autoUpdaterRef.autoDownload = true;
                autoUpdaterRef.autoInstallOnAppQuit = true;
                autoUpdaterRef.allowPrerelease = true;
                if (!app.isPackaged) {
                    autoUpdaterRef.forceDevUpdateConfig = true;
                }
            } catch (e) {
                return { status: 'Auto-updater not available', error: e.message };
            }
        }

        wireAutoUpdaterEvents();

        await autoUpdaterRef.checkForUpdates();
        return { status: 'Checking for updates...' };
    } catch (e) {
        return { status: 'Update check failed', error: e.message };
    }
});

ipcMain.handle('update:restart', async () => {
    try {
        if (autoUpdaterRef) {
            autoUpdaterRef.quitAndInstall();
            return { status: 'Restarting to install update...' };
        }
        return { status: 'No update ready to install.' };
    } catch (e) {
        return { status: 'Restart failed', error: e.message };
    }
});

ipcMain.handle('window:setTheme', async (_event, theme) => {
    applyTitleBarTheme(theme);
    return { status: 'ok' };
});

ipcMain.handle('folder:openAutoBackup', async () => {
    try {
        const fs = require('fs');
        const configPath = path.join(app.getPath('userData'), 'db_config.json');
        let backupRoot = 'C:\\Finlogs';
        if (fs.existsSync(configPath)) {
            try {
                const raw = fs.readFileSync(configPath, 'utf8');
                const parsed = JSON.parse(raw || '{}');
                const configured = typeof parsed.backup_dir === 'string' ? parsed.backup_dir.trim() : '';
                if (configured) {
                    backupRoot = configured;
                }
            } catch (_e) {
                // keep fallback
            }
        }
        const backupDir = path.join(backupRoot, 'Auto');
        if (!fs.existsSync(backupDir)) {
            fs.mkdirSync(backupDir, { recursive: true });
        }
        await shell.openPath(backupDir);
        return { success: true };
    } catch (e) {
        return { success: false, error: e.message };
    }
});


ipcMain.handle('server:restart', async () => {
    try {
        backendRestartAttempted = false;
        killBackend();
        await new Promise((r) => setTimeout(r, 220));
        const started = await startBackend();
        if (!started) {
            return { success: false, error: 'Backend did not start. Port 8000 may still be in use.' };
        }
        return { success: true };
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('server:stop', async () => {
    try {
        killBackend();
        const portFree = await isPortFree(8000, '127.0.0.1');
        if (!portFree) {
            return { success: false, error: 'The in-app backend stopped, but another process is still using port 8000.' };
        }
        return { success: true };
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('server:install', async () => {
    try {
        const taskName = 'M-FinlogsServer';
        const userDataPath = app.getPath('userData');
        const jwtSecret = ensureJwtSecret(userDataPath);
        
        // Create a startup script instead of inline command
        const scriptPath = path.join(userDataPath, 'start_server.bat');
        
        let scriptContent;
        if (app.isPackaged) {
            const backendPath = path.join(process.resourcesPath, 'backend.exe');
            scriptContent = `@echo off
set FINLOGS_CONFIG_DIR=${userDataPath}
set FINLOGS_HOST=0.0.0.0
set FINLOGS_PORT=8000
set JWT_SECRET_KEY=${jwtSecret}
"${backendPath}"`;
        } else {
            scriptContent = `@echo off
cd /d "${__dirname}"
set FINLOGS_CONFIG_DIR=${userDataPath}
set FINLOGS_HOST=0.0.0.0
set FINLOGS_PORT=8000
set JWT_SECRET_KEY=${jwtSecret}
python -m uvicorn backend:app --host 0.0.0.0 --port 8000`;
        }
        
        // Write the script file
        fs.writeFileSync(scriptPath, scriptContent, 'utf8');
        
        const { exec } = require('child_process');
        return await new Promise((resolve) => {
            // Try with highest privileges first (boot start)
            exec(`schtasks /Create /F /SC ONSTART /RL HIGHEST /TN "${taskName}" /TR "\\"${scriptPath}\\""`, (err, stdout, stderr) => {
                if (!err) return resolve({ success: true });

                const errMsg = (stderr || err.message || '').toString();
                if (errMsg.toLowerCase().includes('access is denied')) {
                    // Retry as current user at logon without admin
                    exec(`schtasks /Create /F /SC ONLOGON /RL LIMITED /TN "${taskName}" /TR "\\"${scriptPath}\\""`, (err2, stdout2, stderr2) => {
                        if (!err2) return resolve({ success: true, warning: 'Installed as user logon task.' });

                        // Final fallback: copy script to Startup folder
                        try {
                            const startupDir = app.getPath('startup');
                            const startupPath = path.join(startupDir, 'M-FinlogsServer.bat');
                            fs.copyFileSync(scriptPath, startupPath);
                            return resolve({ success: true, warning: 'Installed via Startup folder.' });
                        } catch (e) {
                            return resolve({ success: false, error: (stderr2 || err2.message || e.message).toString() });
                        }
                    });
                } else {
                    return resolve({ success: false, error: errMsg });
                }
            });
        });
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('server:uninstall', async () => {
    try {
        const { exec } = require('child_process');
        const userDataPath = app.getPath('userData');
        const scriptPath = path.join(userDataPath, 'start_server.bat');
        const startupPath = path.join(app.getPath('startup'), 'M-FinlogsServer.bat');
        
        return await new Promise((resolve) => {
            exec(`schtasks /Delete /F /TN "M-FinlogsServer"`, (err, stdout, stderr) => {
                // Try to delete the script file (ignore errors if it doesn't exist)
                try {
                    if (fs.existsSync(scriptPath)) {
                        fs.unlinkSync(scriptPath);
                    }
                    if (fs.existsSync(startupPath)) {
                        fs.unlinkSync(startupPath);
                    }
                } catch (e) {
                    console.error('Could not delete script file:', e);
                }

                if (err) {
                    const errMsg = (stderr || err.message || '').toString();
                    if (errMsg.toLowerCase().includes('cannot find the file specified')) {
                        return resolve({ success: true, warning: 'Task not found. Nothing to remove.' });
                    }
                    return resolve({ success: false, error: errMsg });
                }
                resolve({ success: true });
            });
        });
    } catch (e) {
        return { success: false, error: e.message };
    }
});
