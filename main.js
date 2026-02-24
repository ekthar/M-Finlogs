const { app, BrowserWindow, ipcMain, dialog, globalShortcut } = require('electron');
const { shell } = require('electron');
const path = require('path');
const { execFile, spawn, exec } = require('child_process');
const net = require('net');
const fs = require('fs');
const http = require('http');

let autoUpdaterRef = null;
let autoUpdaterReady = false;
let updateDownloadedReady = false;
let updateCheckInProgress = false;
let splashShownAt = 0;

const UPDATE_CHANNEL = (process.env.FINLOGS_UPDATE_CHANNEL || 'stable').toLowerCase();

function configureRuntimePaths() {
    try {
        const userDataDir = app.getPath('userData');
        const sessionDir = path.join(userDataDir, 'session-cache');
        const httpCacheDir = path.join(userDataDir, 'http-cache');

        if (!fs.existsSync(sessionDir)) fs.mkdirSync(sessionDir, { recursive: true });
        if (!fs.existsSync(httpCacheDir)) fs.mkdirSync(httpCacheDir, { recursive: true });

        app.setPath('sessionData', sessionDir);
        app.commandLine.appendSwitch('disk-cache-dir', httpCacheDir);
        app.commandLine.appendSwitch('disable-gpu-shader-disk-cache');
    } catch (e) {
        console.warn('Failed to configure session/cache paths:', e.message);
    }
}

configureRuntimePaths();

function isPrereleaseChannel() {
    const version = (app && typeof app.getVersion === 'function') ? app.getVersion() : '';
    const isVersionPrerelease = typeof version === 'string' && version.includes('-');
    return isVersionPrerelease || ['beta', 'alpha', 'prerelease', 'rc', 'canary'].includes(UPDATE_CHANNEL);
}

function appendUpdateLog(message) {
    try {
        const logPath = path.join(app.getPath('userData'), 'updater.log');
        fs.appendFileSync(logPath, `[${new Date().toISOString()}] ${message}\n`, 'utf8');
    } catch (_) {
        // ignore logging failures
    }
}

function getUpdateConfigSummary() {
    return {
        channel: UPDATE_CHANNEL,
        allowPrerelease: isPrereleaseChannel(),
        autoDownload: true
    };
}

function ensureAutoUpdater() {
    if (autoUpdaterRef) return autoUpdaterRef;
    const { autoUpdater } = require('electron-updater');
    autoUpdaterRef = autoUpdater;
    autoUpdaterRef.autoDownload = true;
    autoUpdaterRef.autoInstallOnAppQuit = true;
    autoUpdaterRef.allowPrerelease = isPrereleaseChannel();
    if (!app.isPackaged) {
        autoUpdaterRef.forceDevUpdateConfig = true;
    }
    appendUpdateLog(`Auto-updater initialized (channel=${UPDATE_CHANNEL}, prerelease=${autoUpdaterRef.allowPrerelease})`);
    return autoUpdaterRef;
}

function sendUpdateStatus(payload) {
    if (mainWindow && mainWindow.webContents) {
        mainWindow.webContents.send('update:status', payload);
    }
}

function wireAutoUpdaterEvents() {
    if (!autoUpdaterRef || autoUpdaterReady || !mainWindow) return;
    autoUpdaterReady = true;
    autoUpdaterRef.on('checking-for-update', () => {
        appendUpdateLog('Checking for updates');
        sendUpdateStatus({ type: 'checking', message: 'Checking for updates...' });
    });
    autoUpdaterRef.on('update-available', (info) => {
        appendUpdateLog(`Update available: v${info.version}`);
        sendUpdateStatus({ type: 'available', message: `Update available: v${info.version}` });
    });
    autoUpdaterRef.on('update-not-available', () => {
        appendUpdateLog('No update available');
        sendUpdateStatus({ type: 'none', message: 'You are up to date.' });
    });
    autoUpdaterRef.on('error', (err) => {
        appendUpdateLog(`Update error: ${err.message}`);
        sendUpdateStatus({ type: 'error', message: `Update error: ${err.message}` });
    });
    autoUpdaterRef.on('download-progress', (p) => {
        const percent = Math.round(p.percent || 0);
        appendUpdateLog(`Download progress: ${percent}%`);
        sendUpdateStatus({ type: 'progress', message: `Downloading update... ${percent}%`, percent });
    });
    autoUpdaterRef.on('update-downloaded', () => {
        updateDownloadedReady = true;
        appendUpdateLog('Update downloaded and ready to install');
        sendUpdateStatus({ type: 'downloaded', message: 'Update downloaded. Restart to install.' });
    });
}

let mainWindow;
let splashWindow;
let backendProcess = null;

function createSplashWindow() {
    splashShownAt = Date.now();
    splashWindow = new BrowserWindow({
        width: 500,
        height: 300,
        frame: false,
        alwaysOnTop: true,
        transparent: true,
        resizable: false,
        webPreferences: {
            nodeIntegration: true
        }
    });
    splashWindow.loadFile('splash.html');
}

function createMainWindow() {
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        title: "M-Finlogs",
        autoHideMenuBar: true,
        show: false,
        backgroundColor: '#eef4ff',
        icon: path.join(__dirname, 'assets', 'finlogs.ico'),
        roundedCorners: true,
        titleBarStyle: 'hidden',
        titleBarOverlay: true,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        }
    });
    mainWindow.loadFile('index.html');

    wireAutoUpdaterEvents();

    let revealed = false;
    const revealMainWindow = () => {
        if (revealed || !mainWindow) return;
        const elapsed = Date.now() - splashShownAt;
        const minSplashMs = 1200;
        const delay = Math.max(0, minSplashMs - elapsed);

        revealed = true;
        setTimeout(() => {
            if (splashWindow && !splashWindow.isDestroyed()) splashWindow.close();
            if (mainWindow && !mainWindow.isVisible()) mainWindow.show();
        }, delay);
    };

    mainWindow.once('ready-to-show', revealMainWindow);
    mainWindow.webContents.once('did-finish-load', revealMainWindow);
    setTimeout(revealMainWindow, 10000);
}

function isPortFree(port, host = '127.0.0.1') {
    return new Promise((resolve) => {
        const tester = net.createServer()
            .once('error', () => resolve(false))
            .once('listening', () => tester.once('close', () => resolve(true)).close())
            .listen(port, host);
    });
}

function isBackendResponsive(timeoutMs = 1500) {
    return new Promise((resolve) => {
        const req = http.get({
            hostname: '127.0.0.1',
            port: 8000,
            path: '/companies',
            timeout: timeoutMs
        }, (res) => {
            res.resume();
            resolve(true);
        });
        req.on('error', () => resolve(false));
        req.on('timeout', () => {
            req.destroy();
            resolve(false);
        });
    });
}

function killPort8000() {
    return new Promise((resolve) => {
        const command = 'powershell -NoProfile -ExecutionPolicy Bypass -Command "' +
            'Get-NetTCPConnection -LocalPort 8000 -ErrorAction SilentlyContinue | ' +
            'Select-Object -First 1 -ExpandProperty OwningProcess | ' +
            'ForEach-Object { Stop-Process -Id $_ -Force -ErrorAction SilentlyContinue }"';
        exec(command, () => resolve());
    });
}

async function startBackend() {
    // Start backend in both dev and production mode
    const { spawn } = require('child_process');
    
    const envVars = { ...process.env, FINLOGS_CONFIG_DIR: app.getPath('userData') };

    let portFree = await isPortFree(8000, '127.0.0.1');
    if (!portFree) {
        const responsive = await isBackendResponsive();
        if (responsive) {
            console.warn('Backend already running on port 8000.');
            return true;
        }
        await killPort8000();
        portFree = await isPortFree(8000, '127.0.0.1');
        if (!portFree) {
            console.warn('Backend not started: port 8000 still in use.');
            return false;
        }
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
        backendProcess.on('exit', (code) => logStream.write(`\n[${new Date().toISOString()}] Backend exit: ${code}\n`));
        return true;
    } else {
        // Development: start uvicorn server
        console.log("Dev Mode: Starting uvicorn backend server...");
        backendProcess = spawn('python', [
            '-m', 'uvicorn', 
            'backend:app', 
            '--host', '127.0.0.1', 
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

ipcMain.handle('app:getVersion', async () => {
    return app.getVersion();
});

ipcMain.handle('update:getConfig', async () => {
    return getUpdateConfigSummary();
});

ipcMain.handle('update:check', async () => {
    if (updateCheckInProgress) {
        return { status: 'Update check already in progress...' };
    }

    try {
        try {
            ensureAutoUpdater();
        } catch (e) {
            appendUpdateLog(`Auto-updater unavailable: ${e.message}`);
            return { status: 'Auto-updater not available', error: e.message };
        }

        updateDownloadedReady = false;
        updateCheckInProgress = true;

        wireAutoUpdaterEvents();

        appendUpdateLog('Check request sent');
        autoUpdaterRef.checkForUpdates()
            .catch((e) => {
                appendUpdateLog(`Async update check failed: ${e.message}`);
                sendUpdateStatus({ type: 'error', message: `Update error: ${e.message}` });
            })
            .finally(() => {
                updateCheckInProgress = false;
            });

        return { status: 'Checking for updates...' };
    } catch (e) {
        updateCheckInProgress = false;
        appendUpdateLog(`Update check failed: ${e.message}`);
        return { status: 'Update check failed', error: e.message };
    }
});

ipcMain.handle('update:restart', async () => {
    try {
        if (autoUpdaterRef && updateDownloadedReady) {
            appendUpdateLog('Installing downloaded update via quitAndInstall');
            autoUpdaterRef.quitAndInstall();
            return { status: 'Restarting to install update...' };
        }
        appendUpdateLog('Install requested but no downloaded update is ready');
        return { status: 'No downloaded update ready. Check for updates first.' };
    } catch (e) {
        appendUpdateLog(`Restart failed: ${e.message}`);
        return { status: 'Restart failed', error: e.message };
    }
});

ipcMain.handle('folder:openAutoBackup', async () => {
    try {
        const fs = require('fs');
        const backupDir = 'C:\\Finlogs\\Auto';
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
        killBackend();
        await new Promise((r) => setTimeout(r, 300));
        const portFree = await isPortFree(8000, '127.0.0.1');
        if (!portFree) {
            return { success: false, error: 'Port 8000 is still in use. Stop the existing backend and try again.' };
        }
        const started = await startBackend();
        if (!started) {
            return { success: false, error: 'Backend did not start. Port may be in use.' };
        }
        return { success: true };
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('server:stop', async () => {
    try {
        killBackend();
        const { exec } = require('child_process');

        const command = 'powershell -NoProfile -ExecutionPolicy Bypass -Command "' +
            'Get-NetTCPConnection -LocalPort 8000 -ErrorAction SilentlyContinue | ' +
            'Select-Object -First 1 -ExpandProperty OwningProcess | ' +
            'ForEach-Object { Stop-Process -Id $_ -Force -ErrorAction SilentlyContinue }"';

        return await new Promise((resolve) => {
            exec(command, async (err) => {
                if (err) return resolve({ success: false, error: err.message });
                const portFree = await isPortFree(8000, '127.0.0.1');
                if (!portFree) {
                    return resolve({ success: false, error: 'Port 8000 is still in use.' });
                }
                resolve({ success: true });
            });
        });
    } catch (e) {
        return { success: false, error: e.message };
    }
});

ipcMain.handle('server:install', async () => {
    try {
        const taskName = 'M-FinlogsServer';
        const userDataPath = app.getPath('userData');
        
        // Create a startup script instead of inline command
        const scriptPath = path.join(userDataPath, 'start_server.bat');
        
        let scriptContent;
        if (app.isPackaged) {
            const backendPath = path.join(process.resourcesPath, 'backend.exe');
            scriptContent = `@echo off
set FINLOGS_CONFIG_DIR=${userDataPath}
set FINLOGS_HOST=0.0.0.0
set FINLOGS_PORT=8000
"${backendPath}"`;
        } else {
            scriptContent = `@echo off
cd /d "${__dirname}"
set FINLOGS_CONFIG_DIR=${userDataPath}
set FINLOGS_HOST=0.0.0.0
set FINLOGS_PORT=8000
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
