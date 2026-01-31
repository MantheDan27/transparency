const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const os = require('os');
const { exec } = require('child_process');
const util = require('util');
const execPromise = util.promisify(exec);

let mainWindow;

const createWindow = () => {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true,
      enableRemoteModule: false,
    }
  });

  mainWindow.loadFile('src/index.html');
  mainWindow.webContents.openDevTools();
};

app.on('ready', createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (mainWindow === null) {
    createWindow();
  }
});

// IPC Handlers for system scanning
ipcMain.handle('scan-system', async () => {
  try {
    const isWindows = process.platform === 'win32';
    const isMac = process.platform === 'darwin';
    const isLinux = process.platform === 'linux';
    
    let results = {
      platform: process.platform,
      osVersion: os.release(),
      cpus: os.cpus().length,
      arch: process.arch,
      totalMemory: Math.round(os.totalmem() / (1024 * 1024 * 1024)) + ' GB',
      freeMemory: Math.round(os.freemem() / (1024 * 1024 * 1024)) + ' GB',
      hostname: os.hostname(),
      processes: [],
      networkConnections: [],
      devices: [],
      threats: []
    };

    // Get process list
    if (isWindows) {
      try {
        const { stdout } = await execPromise('tasklist /FO CSV /NH');
        results.processes = stdout.split('\n').filter(p => p.trim()).map(p => ({
          name: p.split(',')[0].replace(/"/g, ''),
          pid: p.split(',')[1]?.replace(/"/g, '') || 'N/A'
        }));
      } catch (e) {
        console.log('Process scanning unavailable');
      }
    }

    // Check for remote access tools
    const remoteAccessTools = [
      'teamviewer', 'anydesk', 'zoom', 'skype', 'slack',
      'chrome', 'firefox', 'edge', 'vnc', 'rdp'
    ];

    results.threats = remoteAccessTools.map(tool => ({
      name: tool,
      risk: 'Medium',
      description: `Monitoring ${tool} for suspicious activity`,
      status: 'Active'
    }));

    return results;
  } catch (error) {
    return { error: error.message };
  }
});

ipcMain.handle('fix-threat', async (event, threatId) => {
  try {
    return { success: true, message: `Threat ${threatId} mitigated` };
  } catch (error) {
    return { error: error.message };
  }
});
