const { contextBridge, ipcRenderer } = require('electron');

const ALLOWED_INVOKE_CHANNELS = new Set([
    'config:readClient',
    'config:writeClient',
    'update:check',
    'update:restart',
    'app:getVersion',
    'window:setTheme',
    'folder:openAutoBackup',
    'dialog:openBackup',
    'dialog:save',
    'server:install',
    'server:uninstall',
    'server:restart',
    'server:stop'
]);

function invokeAllowed(channel, ...args) {
    if (!ALLOWED_INVOKE_CHANNELS.has(channel)) {
        throw new Error(`IPC channel not allowed: ${channel}`);
    }
    return ipcRenderer.invoke(channel, ...args);
}

contextBridge.exposeInMainWorld('electronAPI', {
    invoke: (channel, ...args) => invokeAllowed(channel, ...args),
    onUpdateStatus: (callback) => {
        const handler = (_event, payload) => callback(payload);
        ipcRenderer.on('update:status', handler);
        return () => ipcRenderer.removeListener('update:status', handler);
    }
});
