'use strict';

const { app, BrowserWindow, ipcMain, shell } = require('electron');
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

// ── Scan state ────────────────────────────────────────────────────────────────
let lastSnapshot   = [];
let lastScanResult = null;   // stored for report export

// ── IPC handlers ──────────────────────────────────────────────────────────────
ipcMain.handle('scan-network', async () => {
  try {
    const scannedAt = new Date().toISOString();
    const devices   = await scanNetwork(msg => mainWindow?.webContents.send('scan-progress', msg));
    const anomalies = analyzeAnomalies(devices, lastSnapshot);
    lastSnapshot    = devices;
    lastScanResult  = { devices, anomalies, scannedAt };
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
  lastSnapshot   = [];
  lastScanResult = null;
  return { success: true };
});

// ── New: local device cache management ───────────────────────────────────────
ipcMain.handle('get-local-devices', async () => {
  // Return IP list of devices in the current local snapshot
  return lastSnapshot.map(d => d.ip);
});

ipcMain.handle('delete-local-device', async (_e, ip) => {
  const before = lastSnapshot.length;
  lastSnapshot = lastSnapshot.filter(d => d.ip !== ip);
  if (lastScanResult) {
    lastScanResult = {
      ...lastScanResult,
      devices: lastScanResult.devices.filter(d => d.ip !== ip),
    };
  }
  return { success: true, removed: before - lastSnapshot.length };
});

// ── New: data preview before cloud enrichment ─────────────────────────────────
ipcMain.handle('get-cloud-preview', async (_e, device) => {
  return {
    endpoint:       'POST /enrich',
    serviceUrl:     `http://127.0.0.1:${CLOUD_PORT}/enrich`,
    dataFields: [
      { field: 'ip',       value: device.ip,              category: 'network_metadata',    description: 'Device IP address on local network' },
      { field: 'hostname', value: device.name || '—',     category: 'device_identity',     description: 'DNS-resolved hostname, if available' },
      { field: 'ports',    value: device.ports || [],     category: 'service_inventory',   description: 'List of open TCP ports detected by scanner' },
    ],
    dataCategories:  ['network_metadata', 'device_identity', 'service_inventory'],
    retentionPolicy: 'Session only — data is held in memory and purged when the application closes.',
    purpose:         'Defensive risk assessment — returns hardening guidance and service-level remediation steps. No exploit instructions are returned.',
    thirdParties:    'None — this enrichment service runs locally (localhost) and makes no external network connections.',
  };
});

// ── New: export full scan report ─────────────────────────────────────────────
ipcMain.handle('export-report', async () => {
  try {
    const ledger = await cloud('/ledger');
    return {
      reportVersion:    '1.0',
      reportFormat:     'transparency-scan-report',
      generatedAt:      new Date().toISOString(),
      generatedBy:      'Transparency v1.0.5',
      scan:             lastScanResult || { devices: [], anomalies: [], scannedAt: null },
      cloudLedger:      ledger,
      summary: {
        totalDevices:   lastScanResult?.devices?.length ?? 0,
        totalAnomalies: lastScanResult?.anomalies?.length ?? 0,
        highSeverity:   lastScanResult?.anomalies?.filter(a => a.severity === 'High').length ?? 0,
        mediumSeverity: lastScanResult?.anomalies?.filter(a => a.severity === 'Medium').length ?? 0,
        lowSeverity:    lastScanResult?.anomalies?.filter(a => a.severity === 'Low').length ?? 0,
        cloudSends:     ledger.filter(e => e.action === 'SEND').length,
      }
    };
  } catch (err) {
    return { error: err.message };
  }
});

// ── New: open external URL via OS browser ─────────────────────────────────────
ipcMain.handle('open-external-url', async (_e, url) => {
  // Only allow https:// links for safety
  if (typeof url === 'string' && url.startsWith('https://')) {
    await shell.openExternal(url);
    return { success: true };
  }
  return { success: false, error: 'Only HTTPS URLs are permitted.' };
});
