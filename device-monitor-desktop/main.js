'use strict';

const { app, BrowserWindow, ipcMain } = require('electron');
const path  = require('path');
const axios = require('axios');

const { scanNetwork, analyzeAnomalies } = require('./scanner');
const { createCloudMockService }        = require('./cloud-mock');

let mainWindow;
let cloudServer;
const CLOUD_PORT = 3001;

// ── Cloud mock service ────────────────────────────────────────────────────────
function startCloud() {
  const mockApp = createCloudMockService();
  cloudServer = mockApp.listen(CLOUD_PORT, '127.0.0.1', () =>
    console.log(`[cloud-mock] http://127.0.0.1:${CLOUD_PORT}`)
  );
}

const cloud = (url, opts = {}) =>
  axios({ baseURL: `http://127.0.0.1:${CLOUD_PORT}`, url, ...opts }).then(r => r.data);

// ── Window ────────────────────────────────────────────────────────────────────
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1300, height: 840, minWidth: 980, minHeight: 620,
    backgroundColor: '#13161d',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: false
    }
  });

  mainWindow.loadFile(path.join(__dirname, 'src', 'index.html')).catch(err =>
    console.error('[window] load failed:', err)
  );
}

app.whenReady().then(() => {
  startCloud();
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (cloudServer) cloudServer.close();
  if (process.platform !== 'darwin') app.quit();
});

// ── IPC handlers ──────────────────────────────────────────────────────────────
let lastSnapshot = [];

ipcMain.handle('scan-network', async () => {
  try {
    const devices   = await scanNetwork(msg => mainWindow?.webContents.send('scan-progress', msg));
    const anomalies = analyzeAnomalies(devices, lastSnapshot);
    lastSnapshot    = devices;
    return { devices, anomalies };
  } catch (err) {
    console.error('[scan]', err);
    return { error: err.message };
  }
});

ipcMain.handle('enrich-device', async (_e, device) => {
  try   { return await cloud('/enrich', { method: 'POST', data: { device } }); }
  catch (err) { return { error: err.message, guidance: 'Cloud enrichment unavailable.', riskLevel: 'Unknown', services: [] }; }
});

ipcMain.handle('get-cloud-ledger', async () => {
  try   { return await cloud('/ledger'); }
  catch { return []; }
});

ipcMain.handle('get-cloud-devices', async () => {
  try   { return await cloud('/devices'); }
  catch { return []; }
});

ipcMain.handle('delete-cloud-data', async () => {
  try   { return await cloud('/data', { method: 'DELETE' }); }
  catch (err) { return { success: false, error: err.message }; }
});

ipcMain.handle('delete-device-data', async (_e, ip) => {
  try   { return await cloud(`/data/${ip}`, { method: 'DELETE' }); }
  catch (err) { return { success: false, error: err.message }; }
});

ipcMain.handle('clear-local-snapshot', async () => {
  lastSnapshot = [];
  return { success: true };
});
