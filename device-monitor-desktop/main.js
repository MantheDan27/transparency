const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const os = require('os');
const fs = require('fs');
const { exec } = require('child_process');
const util = require('util');
const express = require('express');
const axios = require('axios');
const execPromise = util.promisify(exec);

let mainWindow;
let cloudMockApp;
let cloudMockServer;

// --- Cloud Mock Service ---
function startCloudMock() {
  cloudMockApp = express();
  cloudMockApp.use(express.json());
  
  const activityLog = [];

  cloudMockApp.post('/enrich', (req, res) => {
    const { device } = req.body;
    activityLog.push({ action: 'enrich', device, timestamp: new Date() });
    
    // Defensive guidance
    res.json({
      guidance: `Device ${device.ip} (${device.name || 'Unknown'}) was analyzed. Recommendation: Ensure strong passwords and disable unused services like SMB or RDP.`,
      riskLevel: 'Informational'
    });
  });

  cloudMockApp.delete('/data', (req, res) => {
    activityLog.push({ action: 'delete_all', timestamp: new Date() });
    res.json({ success: true, message: 'All cloud data deleted.' });
  });

  cloudMockApp.get('/ledger', (req, res) => {
    res.json(activityLog);
  });

  cloudMockServer = cloudMockApp.listen(3000, () => {
    console.log('Cloud Mock Service running on port 3000');
  });
}

// --- Scanner Logic ---
async function discoverDevices() {
  const devices = [];
  const networkInfo = os.networkInterfaces();
  let baseIp = '192.168.1'; // Default fallback
  
  // Try to find local IP
  for (const name of Object.keys(networkInfo)) {
    for (const net of networkInfo[name]) {
      if (net.family === 'IPv4' && !net.internal) {
        baseIp = net.address.split('.').slice(0, 3).join('.');
      }
    }
  }

  console.log(`Scanning base IP: ${baseIp}.x`);

  // Mock discovery for speed in this demo, but structured for real logic
  const mockDevices = [
    { ip: `${baseIp}.1`, mac: '00:11:22:33:44:55', name: 'Gateway', ports: [80, 443] },
    { ip: `${baseIp}.10`, mac: 'AA:BB:CC:DD:EE:FF', name: 'Work Laptop', ports: [22, 445] },
    { ip: `${baseIp}.15`, mac: 'Unknown', name: 'IoT Device', ports: [8080, 23] }
  ];

  return mockDevices;
}

function analyzeAnomalies(devices, previousSnapshot = []) {
  const anomalies = [];
  const riskyPorts = [23, 445, 3389, 139]; // Telnet, SMB, RDP

  devices.forEach(device => {
    // Risky ports rule
    const exposedRiskyPorts = device.ports.filter(p => riskyPorts.includes(p));
    if (exposedRiskyPorts.length > 0) {
      anomalies.push({
        type: 'Risky Port',
        severity: 'High',
        device: device.ip,
        description: `Device exposes dangerous ports: ${exposedRiskyPorts.join(', ')}`
      });
    }

    // MAC unknown rule
    if (device.mac === 'Unknown') {
      anomalies.push({
        type: 'Unknown MAC',
        severity: 'Medium',
        device: device.ip,
        description: 'Device MAC address could not be resolved.'
      });
    }

    // New device rule (vs snapshot)
    const exists = previousSnapshot.find(d => d.ip === device.ip);
    if (!exists && previousSnapshot.length > 0) {
      anomalies.push({
        type: 'New Device',
        severity: 'Low',
        device: device.ip,
        description: 'New device detected on network since last scan.'
      });
    }
  });

  return anomalies;
}

// --- Window Management ---
const createWindow = () => {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    backgroundColor: '#0a0e27',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: false,
    }
  });

  const indexPath = path.join(__dirname, 'src', 'index.html');
  console.log('Loading index.html from:', indexPath);
  
  mainWindow.loadFile(indexPath).catch(err => {
    console.error('Failed to load index.html:', err);
  });
  
  mainWindow.once('ready-to-show', () => {
    console.log('Main window ready to show');
    mainWindow.show();
  });

  // mainWindow.webContents.openDevTools();
};

app.whenReady().then(() => {
  startCloudMock();
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    if (cloudMockServer) cloudMockServer.close();
    app.quit();
  }
});

// --- IPC Handlers ---
let lastSnapshot = [];

ipcMain.handle('scan-network', async () => {
  try {
    const devices = await discoverDevices();
    const anomalies = analyzeAnomalies(devices, lastSnapshot);
    lastSnapshot = devices;
    return { devices, anomalies };
  } catch (error) {
    console.error('Scan Error:', error);
    return { error: error.message };
  }
});

ipcMain.handle('get-cloud-ledger', async () => {
  try {
    const response = await axios.get('http://localhost:3000/ledger');
    return response.data;
  } catch (error) {
    return [];
  }
});

ipcMain.handle('delete-cloud-data', async () => {
  try {
    const response = await axios.delete('http://localhost:3000/data');
    return response.data;
  } catch (error) {
    return { success: false, error: error.message };
  }
});

ipcMain.handle('enrich-device', async (event, device) => {
  try {
    const response = await axios.post('http://localhost:3000/enrich', { device });
    return response.data;
  } catch (error) {
    return { guidance: 'Cloud enrichment unavailable.' };
  }
});
