const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

function fail(msg) {
  console.error(`\n[build preflight] ERROR: ${msg}`);
  process.exit(1);
}

function warn(msg) {
  console.warn(`[build preflight] WARN: ${msg}`);
}

const isWindows = process.platform === 'win32';
if (!isWindows) {
  warn('This preflight is tuned for Windows packaging. Skipping Windows checks.');
  process.exit(0);
}

const systemRoot = process.env.SystemRoot || 'C:\\Windows';
const cmdPath = path.join(systemRoot, 'System32', 'cmd.exe');

if (!fs.existsSync(cmdPath)) {
  fail(`cmd.exe not found at ${cmdPath}. Electron packaging may fail with spawn cmd.exe ENOENT.`);
}

const comSpec = process.env.ComSpec || '';
if (!comSpec || !comSpec.toLowerCase().endsWith('cmd.exe')) {
  warn(`ComSpec is '${comSpec || '(empty)'}'. Build scripts will override ComSpec to ${cmdPath}.`);
}

try {
  const pyVersionOut = execSync('python -V', { stdio: ['ignore', 'pipe', 'pipe'] }).toString().trim();
  const match = pyVersionOut.match(/Python\s+(\d+)\.(\d+)\.(\d+)/i);
  if (match) {
    const major = Number(match[1]);
    const minor = Number(match[2]);
    if (major > 3 || (major === 3 && minor >= 14)) {
      warn(`Detected ${pyVersionOut}. Python 3.14+ may cause PyInstaller/pydantic warnings. Prefer Python 3.12 for backend build.`);
    }
  }
} catch (_) {
  warn('Unable to detect Python version with `python -V`.');
}

console.log('[build preflight] OK: Windows shell prerequisites look good.');
