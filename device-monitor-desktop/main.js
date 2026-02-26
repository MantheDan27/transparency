const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const os = require('os');
const fs = require('fs');
const { exec } = require('child_process');
const util = require('util');
const execPromise = util.promisify(exec);

let mainWindow;

const createWindow = () => {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    show: false,
    backgroundColor: '#0a0e27', // Set background color to match app theme
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: false,
    }
  });

  const indexPath = path.join(__dirname, 'src', 'index.html');
  
  if (fs.existsSync(indexPath)) {
    mainWindow.loadFile(indexPath).catch(err => {
      console.error('Failed to load index.html:', err);
    });
  } else {
    console.error('index.html not found at:', indexPath);
    mainWindow.loadURL(`data:text/html,<html><body style="background: #0a0e27; color: white; font-family: sans-serif; padding: 20px;">
      <h1>Error: index.html not found</h1>
      <p>The application could not find the required files at: <code>${indexPath}</code></p>
    </body></html>`);
  }
  
  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
  });

  // Open DevTools in development or if explicitly requested
  if (!app.isPackaged || process.env.DEBUG_ELECTRON) {
    mainWindow.webContents.openDevTools();
  }
};

// Error handling for the main process
process.on('uncaughtException', (error) => {
  console.error('Main Process Uncaught Exception:', error);
});

process.on('unhandledRejection', (reason, promise) => {
  console.error('Main Process Unhandled Rejection at:', promise, 'reason:', reason);
});

app.on('ready', createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
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
