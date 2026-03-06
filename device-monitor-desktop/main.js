'use strict';

const { app, BrowserWindow, ipcMain, shell, dialog } = require('electron');
const path  = require('path');
const os    = require('os');
const net   = require('net');
const dns   = require('dns');
const { exec } = require('child_process');
const util  = require('util');
const axios = require('axios');
const fs    = require('fs');

const { scanNetwork, analyzeAnomalies, getLocalNetwork, getLocalNetworks } = require('./scanner');
const { createCloudMockService } = require('./cloud-mock');
const http = require('http');

const execPromise = util.promisify(exec);
const dnsLookup   = util.promisify(dns.lookup);
const dnsReverse  = util.promisify(dns.reverse);
const dnsResolve4 = util.promisify(dns.resolve4);

let mainWindow;
let cloudServer;
const CLOUD_PORT = 3001;

// ── Persistent data directory ─────────────────────────────────────────────────
const DATA_DIR = path.join(app.getPath('userData'), 'transparency-data');
function ensureDataDir() {
  if (!fs.existsSync(DATA_DIR)) fs.mkdirSync(DATA_DIR, { recursive: true });
}
ensureDataDir(); // Must run before any loadJSON/saveJSON calls
function dataFile(name) { return path.join(DATA_DIR, name); }
function loadJSON(file, def) {
  try { return JSON.parse(fs.readFileSync(dataFile(file), 'utf8')); }
  catch { return def; }
}
function saveJSON(file, data) {
  try { fs.writeFileSync(dataFile(file), JSON.stringify(data, null, 2)); }
  catch (e) { console.error('[save]', e); }
}

// ── In-memory state ───────────────────────────────────────────────────────────
let lastSnapshot   = [];
let lastScanResult = null;

// Device history: mac → { firstSeen, lastSeen, ipHistory, portHistory, nameHistory, uptimeRatio }
let deviceHistory  = loadJSON('device-history.json', {});

// Device metadata: mac|ip → { customName, trustState, tags, notes, watchlist }
let deviceMeta     = loadJSON('device-meta.json', {});

// Alerts store
let alertRules     = loadJSON('alert-rules.json', getDefaultAlertRules());
let triggeredAlerts = [];
let alertDebounce  = {}; // ruleId+deviceKey → lastTriggeredMs

// Snapshots
let snapshots      = loadJSON('snapshots.json', []);

// ── Monitoring state ──────────────────────────────────────────────────────────
let monitoringTimer   = null;
let monitoringConfig  = loadJSON('monitoring-config.json', {
  enabled: false,
  intervalMinutes: 5,
  quietHoursStart: null,
  quietHoursEnd:   null,
  alertOnOutage:   true,
  alertOnGatewayMacChange: true,
  alertOnDnsChange: true,
  alertOnHighLatency: true,
  highLatencyThresholdMs: 100,
});
let internetStatus    = { online: true, lastCheck: null, latencyMs: null };
let lastGatewayMac    = null;
let lastDnsServers    = null;
let localApiServer    = null;
const LOCAL_API_PORT  = 7722;

// Latency history: ip → [{ts, ms}]
let latencyHistory = {};
const MAX_LATENCY_ENTRIES = 100;

// ── Default alert rules ───────────────────────────────────────────────────────
function getDefaultAlertRules() {
  return [
    {
      id: 'rule-new-device',
      name: 'New unknown device joined',
      enabled: true,
      conditions: { deviceFilter: 'unknown', eventType: 'new_device', timeWindow: null },
      actions: { notification: true, webhook: null, severity: 'medium', debounceMinutes: 1 },
      createdAt: new Date().toISOString(),
    },
    {
      id: 'rule-risky-port',
      name: 'Risky port exposed',
      enabled: true,
      conditions: { deviceFilter: 'all', eventType: 'risky_port', timeWindow: null },
      actions: { notification: true, webhook: null, severity: 'high', debounceMinutes: 10 },
      createdAt: new Date().toISOString(),
    },
    {
      id: 'rule-port-changed',
      name: 'Port profile changed',
      enabled: true,
      conditions: { deviceFilter: 'watchlist', eventType: 'port_changed', timeWindow: null },
      actions: { notification: true, webhook: null, severity: 'medium', debounceMinutes: 5 },
      createdAt: new Date().toISOString(),
    },
  ];
}

// ── Monitoring helpers ────────────────────────────────────────────────────────
function isInQuietHours() {
  const { quietHoursStart, quietHoursEnd } = monitoringConfig;
  if (!quietHoursStart || !quietHoursEnd) return false;
  const now = new Date();
  const [sh, sm] = quietHoursStart.split(':').map(Number);
  const [eh, em] = quietHoursEnd.split(':').map(Number);
  const nowMin   = now.getHours() * 60 + now.getMinutes();
  const startMin = sh * 60 + sm;
  const endMin   = eh * 60 + em;
  return startMin <= endMin
    ? nowMin >= startMin && nowMin < endMin
    : nowMin >= startMin || nowMin < endMin;
}

async function checkInternetConnectivity() {
  try {
    const start = Date.now();
    await axios.head('https://1.1.1.1', { timeout: 6000, validateStatus: () => true });
    const latencyMs = Date.now() - start;
    const wasOffline = !internetStatus.online;
    internetStatus = { online: true, lastCheck: new Date().toISOString(), latencyMs };
    if (wasOffline) {
      triggerAlert('monitor-internet-restored', 'Internet Restored', 'low',
        'Internet is back', 'Internet connectivity has been restored.', 'internet', {});
      mainWindow?.webContents?.send('internet-status', internetStatus);
    }
    if (monitoringConfig.alertOnHighLatency && latencyMs > monitoringConfig.highLatencyThresholdMs) {
      triggerAlert('monitor-high-latency', 'High Gateway Latency', 'low',
        'High internet latency', `Internet latency is ${latencyMs}ms (threshold: ${monitoringConfig.highLatencyThresholdMs}ms).`, 'internet', { latencyMs });
    }
  } catch {
    const wasOnline = internetStatus.online;
    internetStatus = { online: false, lastCheck: new Date().toISOString(), latencyMs: null };
    if (wasOnline && monitoringConfig.alertOnOutage) {
      triggerAlert('monitor-outage', 'Internet Outage', 'high',
        'Internet outage detected', 'No internet connectivity — could be ISP issue or gateway problem.', 'internet', {});
      mainWindow?.webContents?.send('internet-status', internetStatus);
    }
  }
}

async function checkGatewayMacChange() {
  if (!monitoringConfig.alertOnGatewayMacChange) return;
  try {
    let gwIp = null;
    if (process.platform === 'win32') {
      const { stdout } = await execPromise('route print 0.0.0.0', { timeout: 4000 });
      const m = stdout.match(/0\.0\.0\.0\s+0\.0\.0\.0\s+([\d.]+)/);
      if (m) gwIp = m[1];
    } else if (process.platform === 'darwin') {
      const { stdout } = await execPromise('netstat -nr | grep "^default"', { timeout: 4000 });
      const m = stdout.match(/default\s+([\d.]+)/);
      if (m) gwIp = m[1];
    } else {
      const { stdout } = await execPromise('ip route show default', { timeout: 4000 });
      const m = stdout.match(/via\s+([\d.]+)/);
      if (m) gwIp = m[1];
    }
    if (!gwIp) return;

    // Get ARP entry for gateway
    let gwMac = null;
    try {
      const cmd = process.platform === 'win32' ? `arp -a ${gwIp}` : `arp -n ${gwIp} 2>/dev/null`;
      const { stdout: arpOut } = await execPromise(cmd, { timeout: 3000 });
      const macM = arpOut.match(/([0-9a-f]{2}[:-]){5}[0-9a-f]{2}/i);
      if (macM) gwMac = macM[0].toLowerCase().replace(/-/g, ':');
    } catch { /* ignore */ }

    if (gwMac && lastGatewayMac && gwMac !== lastGatewayMac) {
      triggerAlert('monitor-gateway-mac', 'Gateway MAC Changed', 'high',
        'Possible rogue access point', `Gateway ${gwIp} MAC changed from ${lastGatewayMac} to ${gwMac}. Possible evil-twin or ARP spoofing.`,
        gwIp, { oldMac: lastGatewayMac, newMac: gwMac });
    }
    if (gwMac) lastGatewayMac = gwMac;
  } catch { /* ignore */ }
}

function checkDnsServerChange() {
  if (!monitoringConfig.alertOnDnsChange) return;
  const currentDns = dns.getServers().sort().join(',');
  if (lastDnsServers && currentDns !== lastDnsServers) {
    triggerAlert('monitor-dns-changed', 'DNS Servers Changed', 'high',
      'DNS server change detected', `DNS servers changed from [${lastDnsServers}] to [${currentDns}]. Possible DNS hijacking.`,
      'network', { oldDns: lastDnsServers, newDns: currentDns });
  }
  lastDnsServers = currentDns;
}

async function runMonitorScan() {
  if (isInQuietHours()) return;
  try {
    const devices   = await scanNetwork(msg => mainWindow?.webContents.send('monitor-progress', msg), { mode: 'quick', gentle: true });
    const anomalies = analyzeAnomalies(devices, lastSnapshot);
    lastSnapshot    = devices;
    lastScanResult  = { devices, anomalies, scannedAt: new Date().toISOString(), mode: 'monitor' };
    updateDeviceHistory(devices);
    processAlertsForScan(devices, anomalies);

    await Promise.allSettled([
      checkInternetConnectivity(),
      checkGatewayMacChange(),
    ]);
    checkDnsServerChange();

    const devicesWithMeta = devices.map(d => ({
      ...d,
      meta: getMeta(getDeviceKey(d)),
      history: deviceHistory[getDeviceKey(d)] || null,
    }));

    mainWindow?.webContents?.send('monitor-scan-complete', {
      devices: devicesWithMeta, anomalies,
      scannedAt: new Date().toISOString(),
      internetStatus,
    });
  } catch (err) {
    console.error('[monitor]', err);
  }
}

function startMonitoring() {
  if (monitoringTimer) clearInterval(monitoringTimer);
  monitoringConfig.enabled = true;
  saveJSON('monitoring-config.json', monitoringConfig);
  const ms = Math.max(1, monitoringConfig.intervalMinutes) * 60 * 1000;
  runMonitorScan(); // run immediately
  monitoringTimer = setInterval(runMonitorScan, ms);
  mainWindow?.webContents?.send('monitor-status', { enabled: true, intervalMinutes: monitoringConfig.intervalMinutes });
}

function stopMonitoring() {
  if (monitoringTimer) { clearInterval(monitoringTimer); monitoringTimer = null; }
  monitoringConfig.enabled = false;
  saveJSON('monitoring-config.json', monitoringConfig);
  mainWindow?.webContents?.send('monitor-status', { enabled: false });
}

// ── Local read-only API ───────────────────────────────────────────────────────
function startLocalApi() {
  if (localApiServer) return;
  localApiServer = http.createServer((req, res) => {
    res.setHeader('Content-Type', 'application/json');
    res.setHeader('Access-Control-Allow-Origin', '127.0.0.1');
    if (req.method !== 'GET') { res.writeHead(405); return res.end(JSON.stringify({ error: 'Read-only API' })); }

    if (req.url === '/api/devices') {
      res.writeHead(200);
      return res.end(JSON.stringify(lastSnapshot.map(d => ({ ip: d.ip, mac: d.mac, name: d.name, type: d.deviceType, meta: getMeta(getDeviceKey(d)) }))));
    }
    if (req.url === '/api/status') {
      res.writeHead(200);
      return res.end(JSON.stringify({ deviceCount: lastSnapshot.length, internetStatus, monitoring: { enabled: monitoringConfig.enabled, intervalMinutes: monitoringConfig.intervalMinutes } }));
    }
    if (req.url === '/api/alerts') {
      res.writeHead(200);
      return res.end(JSON.stringify(triggeredAlerts.slice(0, 50)));
    }
    res.writeHead(404);
    res.end(JSON.stringify({ error: 'Not found', routes: ['/api/devices', '/api/status', '/api/alerts'] }));
  });
  localApiServer.listen(LOCAL_API_PORT, '127.0.0.1', () => console.log(`[local-api] http://127.0.0.1:${LOCAL_API_PORT}`));
}

// ── Alert helpers ─────────────────────────────────────────────────────────────
function getMeta(key) {
  return deviceMeta[key] || { customName: null, trustState: 'unknown', tags: [], notes: '', watchlist: false };
}

function getDeviceKey(device) {
  return (device.mac && device.mac !== 'Unknown') ? `mac:${device.mac}` : `ip:${device.ip}`;
}

function triggerAlert(ruleId, ruleName, severity, title, message, deviceIp, data = {}) {
  const debounceKey = `${ruleId}:${deviceIp}`;
  const rule = alertRules.find(r => r.id === ruleId);
  const debounceMs = (rule?.actions?.debounceMinutes || 5) * 60 * 1000;
  const now = Date.now();

  if (alertDebounce[debounceKey] && (now - alertDebounce[debounceKey]) < debounceMs) return;
  alertDebounce[debounceKey] = now;

  const alert = {
    id: `alert-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`,
    ruleId, ruleName, severity, title, message, deviceIp,
    timestamp: new Date().toISOString(),
    acknowledged: false,
    data,
  };
  triggeredAlerts.unshift(alert);
  if (triggeredAlerts.length > 500) triggeredAlerts = triggeredAlerts.slice(0, 500);

  // Send to renderer
  mainWindow?.webContents?.send('new-alert', alert);

  // Fire webhook if configured
  if (rule?.actions?.webhook) {
    axios.post(rule.actions.webhook, alert, { timeout: 5000 }).catch(() => {});
  }
}

function processAlertsForScan(devices, anomalies) {
  // Check quiet hours for user-defined rules
  const quiet = isInQuietHours();

  for (const rule of alertRules) {
    if (!rule.enabled) continue;
    if (quiet && rule.conditions.quietHours !== false) continue; // respect quiet hours

    for (const anomaly of anomalies) {
      const meta  = getMeta(`ip:${anomaly.device}`);
      const trust = meta.trustState;

      // Check device filter
      let pass = false;
      if (rule.conditions.deviceFilter === 'all') pass = true;
      else if (rule.conditions.deviceFilter === 'unknown' && trust === 'unknown') pass = true;
      else if (rule.conditions.deviceFilter === 'watchlist' && meta.watchlist) pass = true;
      else if (rule.conditions.deviceFilter === trust) pass = true;
      if (!pass) continue;

      // Check event type
      if (rule.conditions.eventType === 'new_device' && anomaly.type === 'New Device') {
        triggerAlert(rule.id, rule.name, rule.actions.severity, 'New device joined', `Device ${anomaly.device} appeared on your network.`, anomaly.device, { anomaly });
      }
      if (rule.conditions.eventType === 'risky_port' && anomaly.type === 'Risky Port') {
        triggerAlert(rule.id, rule.name, rule.actions.severity, 'Risky port detected', `${anomaly.device} has exposed service(s): ${anomaly.description}`, anomaly.device, { anomaly });
      }
      if (rule.conditions.eventType === 'port_changed' && anomaly.type === 'Ports Changed') {
        triggerAlert(rule.id, rule.name, rule.actions.severity, 'Port profile changed', `${anomaly.device}: ${anomaly.description}`, anomaly.device, { anomaly });
      }
      if (rule.conditions.eventType === 'device_offline' && anomaly.type === 'Device Offline') {
        triggerAlert(rule.id, rule.name, rule.actions.severity, 'Device went offline', `${anomaly.device} is no longer reachable.`, anomaly.device, { anomaly });
      }
      if (rule.conditions.eventType === 'ip_changed' && anomaly.type === 'IP Changed') {
        triggerAlert(rule.id, rule.name, rule.actions.severity, 'Device IP changed', `${anomaly.device}: ${anomaly.description}`, anomaly.device, { anomaly });
      }
    }
  }
}

// ── Device history tracking ───────────────────────────────────────────────────
function updateDeviceHistory(devices) {
  const now = new Date().toISOString();
  for (const dev of devices) {
    const key = getDeviceKey(dev);
    if (!deviceHistory[key]) {
      deviceHistory[key] = {
        key, mac: dev.mac, firstSeen: now, lastSeen: now,
        ipHistory: [], portHistory: [], nameHistory: [],
        onlineChecks: 0, onlineCount: 0,
      };
    }
    const h = deviceHistory[key];
    h.lastSeen = now;
    h.onlineChecks = (h.onlineChecks || 0) + 1;
    h.onlineCount  = (h.onlineCount  || 0) + 1;

    // Track IP changes
    const lastIp = h.ipHistory.length > 0 ? h.ipHistory[h.ipHistory.length - 1].ip : null;
    if (lastIp !== dev.ip) {
      h.ipHistory.push({ ip: dev.ip, ts: now });
      if (h.ipHistory.length > 20) h.ipHistory = h.ipHistory.slice(-20);
    }

    // Track port changes
    const lastPorts = h.portHistory.length > 0 ? h.portHistory[h.portHistory.length - 1].ports : null;
    const portsStr  = JSON.stringify(dev.ports.sort((a, b) => a - b));
    if (lastPorts === null || JSON.stringify(lastPorts.sort((a, b) => a - b)) !== portsStr) {
      h.portHistory.push({ ports: dev.ports, ts: now });
      if (h.portHistory.length > 20) h.portHistory = h.portHistory.slice(-20);
    }

    // Track name changes
    const displayName = dev.hostname || dev.name;
    const lastName = h.nameHistory.length > 0 ? h.nameHistory[h.nameHistory.length - 1].name : null;
    if (lastName !== displayName && displayName) {
      h.nameHistory.push({ name: displayName, ts: now });
      if (h.nameHistory.length > 10) h.nameHistory = h.nameHistory.slice(-10);
    }

    // Latency tracking
    if (dev.latencyMs != null) {
      if (!latencyHistory[dev.ip]) latencyHistory[dev.ip] = [];
      latencyHistory[dev.ip].push({ ts: now, ms: dev.latencyMs });
      if (latencyHistory[dev.ip].length > MAX_LATENCY_ENTRIES)
        latencyHistory[dev.ip] = latencyHistory[dev.ip].slice(-MAX_LATENCY_ENTRIES);
    }
  }
  saveJSON('device-history.json', deviceHistory);
}

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
    width: 1400, height: 860, minWidth: 1100, minHeight: 680,
    backgroundColor: '#13161d',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      nodeIntegration: false, contextIsolation: true, sandbox: false
    }
  });
  mainWindow.loadFile(path.join(__dirname, 'src', 'index.html'))
    .catch(err => console.error('[window]', err));
}

app.whenReady().then(() => {
  ensureDataDir();
  startCloud();
  createWindow();
  // Resume monitoring if it was enabled before
  if (monitoringConfig.enabled) {
    // Delay slightly to let window load first
    setTimeout(() => startMonitoring(), 3000);
  }
  // Initialize DNS baseline
  lastDnsServers = dns.getServers().sort().join(',');
  // Start local REST API with API-key authentication
  setTimeout(() => restartLocalApiWithAuth(), 500);
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  stopMonitoring();
  if (cloudServer) cloudServer.close();
  if (localApiServer) localApiServer.close();
  if (process.platform !== 'darwin') app.quit();
});

// ── IPC: Network scanning ─────────────────────────────────────────────────────
ipcMain.handle('scan-network', async (_e, opts = {}) => {
  try {
    const scannedAt = new Date().toISOString();
    const devices   = await scanNetwork(msg => mainWindow?.webContents.send('scan-progress', msg), opts);
    const anomalies = analyzeAnomalies(devices, lastSnapshot);
    lastSnapshot    = devices;
    lastScanResult  = { devices, anomalies, scannedAt, mode: opts.mode || 'standard' };

    updateDeviceHistory(devices);
    processAlertsForScan(devices, anomalies);

    // Fire script hooks (defined later in file — safe to call after first scan)
    try {
      if (typeof runScriptHooks === 'function') {
        runScriptHooks('scan_complete', { deviceCount: devices.length, anomalyCount: anomalies.length, mode: opts.mode });
        const newDevs = anomalies.filter(a => a.type === 'New Device');
        if (newDevs.length) runScriptHooks('new_device', { devices: newDevs.map(a => a.device) });
      }
    } catch { /* hooks not loaded yet */ }

    // Merge metadata into device objects for renderer
    const devicesWithMeta = devices.map(d => ({
      ...d,
      meta: getMeta(getDeviceKey(d)),
      history: deviceHistory[getDeviceKey(d)] || null,
    }));

    return { devices: devicesWithMeta, anomalies, scannedAt };
  } catch (err) {
    console.error('[scan]', err);
    return { error: err.message };
  }
});

// ── IPC: Device metadata ──────────────────────────────────────────────────────
ipcMain.handle('get-device-meta', async (_e, key) => {
  return getMeta(key);
});

ipcMain.handle('set-device-meta', async (_e, key, updates) => {
  deviceMeta[key] = { ...getMeta(key), ...updates };
  saveJSON('device-meta.json', deviceMeta);
  return { success: true };
});

ipcMain.handle('get-all-device-meta', async () => {
  return deviceMeta;
});

// ── IPC: Device history ───────────────────────────────────────────────────────
ipcMain.handle('get-device-history', async (_e, key) => {
  return deviceHistory[key] || null;
});

ipcMain.handle('get-all-device-history', async () => {
  return deviceHistory;
});

ipcMain.handle('get-latency-history', async (_e, ip) => {
  return latencyHistory[ip] || [];
});

// ── IPC: Alerts ───────────────────────────────────────────────────────────────
ipcMain.handle('get-alerts', async () => {
  return triggeredAlerts;
});

ipcMain.handle('acknowledge-alert', async (_e, alertId) => {
  const alert = triggeredAlerts.find(a => a.id === alertId);
  if (alert) { alert.acknowledged = true; return { success: true }; }
  return { success: false, error: 'Alert not found' };
});

ipcMain.handle('dismiss-alert', async (_e, alertId) => {
  triggeredAlerts = triggeredAlerts.filter(a => a.id !== alertId);
  return { success: true };
});

ipcMain.handle('clear-all-alerts', async () => {
  triggeredAlerts = [];
  return { success: true };
});

ipcMain.handle('get-alert-rules', async () => alertRules);

ipcMain.handle('create-alert-rule', async (_e, rule) => {
  const newRule = {
    ...rule,
    id: `rule-${Date.now()}`,
    createdAt: new Date().toISOString(),
  };
  alertRules.push(newRule);
  saveJSON('alert-rules.json', alertRules);
  return { success: true, rule: newRule };
});

ipcMain.handle('update-alert-rule', async (_e, id, updates) => {
  const idx = alertRules.findIndex(r => r.id === id);
  if (idx === -1) return { success: false, error: 'Rule not found' };
  alertRules[idx] = { ...alertRules[idx], ...updates };
  saveJSON('alert-rules.json', alertRules);
  return { success: true };
});

ipcMain.handle('delete-alert-rule', async (_e, id) => {
  alertRules = alertRules.filter(r => r.id !== id);
  saveJSON('alert-rules.json', alertRules);
  return { success: true };
});

// ── IPC: Snapshots ────────────────────────────────────────────────────────────
ipcMain.handle('save-snapshot', async (_e, name) => {
  if (!lastScanResult) return { success: false, error: 'No scan data to snapshot' };
  const snapshot = {
    id: `snap-${Date.now()}`,
    name: name || `Snapshot ${new Date().toLocaleString()}`,
    createdAt: new Date().toISOString(),
    devices: lastScanResult.devices,
    anomalies: lastScanResult.anomalies,
  };
  snapshots.unshift(snapshot);
  if (snapshots.length > 20) snapshots = snapshots.slice(0, 20);
  saveJSON('snapshots.json', snapshots);
  return { success: true, snapshot };
});

ipcMain.handle('get-snapshots', async () => snapshots);

ipcMain.handle('delete-snapshot', async (_e, id) => {
  snapshots = snapshots.filter(s => s.id !== id);
  saveJSON('snapshots.json', snapshots);
  return { success: true };
});

// ── IPC: Diagnostic tools ─────────────────────────────────────────────────────
ipcMain.handle('ping-host', async (_e, host, count = 4) => {
  try {
    const cmd = process.platform === 'win32'
      ? `ping -n ${count} ${host}`
      : `ping -c ${count} ${host}`;
    const { stdout } = await execPromise(cmd, { timeout: 15000 });
    // Parse latency from output
    let avgMs = null;
    const winMatch = stdout.match(/Average\s*=\s*(\d+)ms/i);
    const unixMatch = stdout.match(/min\/avg\/max.*=\s*[\d.]+\/([\d.]+)/);
    if (winMatch)  avgMs = parseInt(winMatch[1]);
    if (unixMatch) avgMs = parseFloat(unixMatch[1]);
    return { success: true, output: stdout, avgMs };
  } catch (err) {
    return { success: false, output: err.stdout || '', error: err.message };
  }
});

ipcMain.handle('traceroute-host', async (_e, host) => {
  try {
    const cmd = process.platform === 'win32' ? `tracert -d ${host}` : `traceroute -n ${host}`;
    const { stdout } = await execPromise(cmd, { timeout: 30000 });
    return { success: true, output: stdout };
  } catch (err) {
    return { success: false, output: err.stdout || '', error: err.message };
  }
});

ipcMain.handle('dns-lookup', async (_e, host, type = 'A') => {
  try {
    const resolveFunc = {
      'A':    () => dnsResolve4(host),
      'PTR':  () => dnsReverse(host),
      'ANY':  () => new Promise((res, rej) => dns.resolve(host, 'ANY', (e, r) => e ? rej(e) : res(r))),
    }[type] || (() => dnsResolve4(host));
    const result = await resolveFunc();
    return { success: true, result, host, type };
  } catch (err) {
    return { success: false, error: err.message, host, type };
  }
});

ipcMain.handle('tcp-connect-test', async (_e, host, port, timeout = 3000) => {
  return new Promise(resolve => {
    const sock = new net.Socket();
    const start = Date.now();
    let done = false;
    const finish = (success, err) => {
      if (!done) {
        done = true;
        sock.destroy();
        resolve({ success, latencyMs: Date.now() - start, error: err || null });
      }
    };
    sock.setTimeout(timeout);
    sock.on('connect', () => finish(true, null));
    sock.on('timeout', () => finish(false, 'Connection timed out'));
    sock.on('error', e => finish(false, e.message));
    sock.connect(port, host);
  });
});

ipcMain.handle('http-test', async (_e, url, timeout = 8000) => {
  try {
    const start = Date.now();
    const resp = await axios.get(url, {
      timeout,
      validateStatus: () => true,
      maxRedirects: 3,
    });
    return {
      success: true,
      statusCode: resp.status,
      statusText: resp.statusText,
      latencyMs: Date.now() - start,
      headers: resp.headers,
      server: resp.headers.server || null,
    };
  } catch (err) {
    return { success: false, error: err.message };
  }
});

// ── IPC: Wi-Fi info ───────────────────────────────────────────────────────────
ipcMain.handle('get-wifi-info', async () => {
  try {
    let ssid = null, bssid = null, channel = null, band = null, signal = null, security = null, txRate = null;

    if (process.platform === 'win32') {
      const { stdout } = await execPromise('netsh wlan show interfaces', { timeout: 5000 });
      ssid     = (stdout.match(/^\s+SSID\s+:\s+(.+)$/m) || [])[1]?.trim();
      bssid    = (stdout.match(/BSSID\s+:\s+(.+)/i) || [])[1]?.trim();
      channel  = (stdout.match(/Channel\s+:\s+(\d+)/i) || [])[1]?.trim();
      signal   = (stdout.match(/Signal\s+:\s+(\d+)%/i) || [])[1]?.trim();
      security = (stdout.match(/Authentication\s+:\s+(.+)/i) || [])[1]?.trim();
      txRate   = (stdout.match(/Transmit rate.*:\s+([\d.]+)/i) || [])[1]?.trim();
    } else if (process.platform === 'darwin') {
      const { stdout } = await execPromise('/System/Library/PrivateFrameworks/Apple80211.framework/Versions/Current/Resources/airport -I', { timeout: 5000 });
      ssid    = (stdout.match(/\s+SSID:\s+(.+)/i) || [])[1]?.trim();
      bssid   = (stdout.match(/BSSID:\s+(.+)/i) || [])[1]?.trim();
      channel = (stdout.match(/channel:\s+(\d+)/i) || [])[1]?.trim();
      signal  = (stdout.match(/agrCtlRSSI:\s+(-?\d+)/i) || [])[1]?.trim();
      security = (stdout.match(/link auth:\s+(.+)/i) || [])[1]?.trim();
      txRate  = (stdout.match(/lastTxRate:\s+(\d+)/i) || [])[1]?.trim();
    } else {
      // Linux: try nmcli or iwconfig
      try {
        const { stdout } = await execPromise('nmcli -t -f active,ssid,bssid,freq,signal,security dev wifi 2>/dev/null | grep "^yes"', { timeout: 5000 });
        const parts = stdout.split(':');
        if (parts.length >= 6) {
          [, ssid, bssid, , signal, security] = parts.map(s => s.trim());
        }
      } catch {
        const { stdout } = await execPromise('iwconfig 2>/dev/null | head -20', { timeout: 5000 });
        ssid   = (stdout.match(/ESSID:"([^"]+)"/i) || [])[1];
        signal = (stdout.match(/Signal level=(-?\d+)/i) || [])[1];
      }
    }

    // Determine band from channel
    if (channel) {
      const ch = parseInt(channel);
      band = ch > 14 ? '5 GHz' : '2.4 GHz';
    }

    return { success: true, ssid, bssid, channel, band, signal, security, txRate };
  } catch (err) {
    return { success: false, error: err.message };
  }
});

// ── IPC: Gateway info ─────────────────────────────────────────────────────────
ipcMain.handle('get-gateway-info', async () => {
  try {
    let gateway = null;
    if (process.platform === 'win32') {
      const { stdout } = await execPromise('route print 0.0.0.0', { timeout: 5000 });
      const m = stdout.match(/0\.0\.0\.0\s+0\.0\.0\.0\s+([\d.]+)/);
      if (m) gateway = m[1];
    } else if (process.platform === 'darwin') {
      const { stdout } = await execPromise('netstat -nr | grep "^default"', { timeout: 5000 });
      const m = stdout.match(/default\s+([\d.]+)/);
      if (m) gateway = m[1];
    } else {
      const { stdout } = await execPromise('ip route show default', { timeout: 5000 });
      const m = stdout.match(/via\s+([\d.]+)/);
      if (m) gateway = m[1];
    }

    if (!gateway) return { success: false, error: 'Could not determine gateway' };

    // Ping gateway for latency
    const cmd = process.platform === 'win32' ? `ping -n 4 ${gateway}` : `ping -c 4 ${gateway}`;
    const { stdout: pingOut } = await execPromise(cmd, { timeout: 10000 });
    let latencyMs = null;
    const winM  = pingOut.match(/Average\s*=\s*(\d+)ms/i);
    const unixM = pingOut.match(/min\/avg\/max.*=\s*[\d.]+\/([\d.]+)/);
    if (winM)  latencyMs = parseInt(winM[1]);
    if (unixM) latencyMs = parseFloat(unixM[1]);

    // Get DNS servers
    let dnsServers = dns.getServers();

    // Test DNS resolution
    let dnsLatencyMs = null;
    try {
      const dnsStart = Date.now();
      await dnsLookup('google.com');
      dnsLatencyMs = Date.now() - dnsStart;
    } catch { /* ignore */ }

    const networks = getLocalNetworks();
    const localIp  = networks[0]?.localIp;

    return {
      success: true,
      gateway, latencyMs, dnsServers, dnsLatencyMs,
      localIp, networks: networks.map(n => ({ name: n.name, ip: n.localIp, cidr: n.cidr })),
    };
  } catch (err) {
    return { success: false, error: err.message };
  }
});

// ── IPC: Network info ─────────────────────────────────────────────────────────
ipcMain.handle('get-network-info', async () => {
  const networks = getLocalNetworks();
  return { networks, dnsServers: dns.getServers() };
});

// ── IPC: Cloud enrichment ─────────────────────────────────────────────────────
ipcMain.handle('enrich-device', async (_e, device) => {
  try   { return await cloud('/enrich', { method:'POST', data:{ device } }); }
  catch (err) { return { error:err.message, guidance:'Cloud enrichment unavailable.', riskLevel:'Unknown', services:[] }; }
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
  try   { return await cloud('/data', { method:'DELETE' }); }
  catch (err) { return { success:false, error:err.message }; }
});

ipcMain.handle('delete-device-data', async (_e, ip) => {
  try   { return await cloud(`/data/${ip}`, { method:'DELETE' }); }
  catch (err) { return { success:false, error:err.message }; }
});

ipcMain.handle('get-cloud-preview', async (_e, device) => {
  return {
    endpoint: 'POST /enrich',
    serviceUrl: `http://127.0.0.1:${CLOUD_PORT}/enrich`,
    dataFields: [
      { field:'ip',       value:device.ip,          category:'network_metadata',  description:'Device IP on local network' },
      { field:'hostname', value:device.name || '—',  category:'device_identity',   description:'DNS-resolved hostname if available' },
      { field:'ports',    value:device.ports || [],  category:'service_inventory', description:'Open TCP ports detected by scanner' },
    ],
    dataCategories:  ['network_metadata', 'device_identity', 'service_inventory'],
    retentionPolicy: 'Session only — purged when application closes.',
    purpose:         'Defensive risk assessment — returns hardening guidance. No exploit instructions.',
    thirdParties:    'None — runs locally (localhost), no internet connections.',
  };
});

// ── IPC: Local snapshot / scan state ─────────────────────────────────────────
ipcMain.handle('clear-local-snapshot', async () => {
  lastSnapshot = []; lastScanResult = null;
  return { success: true };
});

ipcMain.handle('get-local-devices', async () => lastSnapshot.map(d => d.ip));

ipcMain.handle('delete-local-device', async (_e, ip) => {
  const before = lastSnapshot.length;
  lastSnapshot = lastSnapshot.filter(d => d.ip !== ip);
  if (lastScanResult) {
    lastScanResult = { ...lastScanResult, devices: lastScanResult.devices.filter(d => d.ip !== ip) };
  }
  return { success:true, removed: before - lastSnapshot.length };
});

// ── IPC: Report export ────────────────────────────────────────────────────────
ipcMain.handle('export-report', async () => {
  try {
    const ledger = await cloud('/ledger').catch(() => []);
    return {
      reportVersion: '2.0', reportFormat: 'transparency-scan-report',
      generatedAt: new Date().toISOString(), generatedBy: 'Transparency v2.0.3',
      scan: lastScanResult || { devices:[], anomalies:[], scannedAt:null },
      cloudLedger: ledger, deviceHistory, deviceMeta, alertRules,
      summary: {
        totalDevices:   lastScanResult?.devices?.length ?? 0,
        totalAnomalies: lastScanResult?.anomalies?.length ?? 0,
        highSeverity:   lastScanResult?.anomalies?.filter(a => a.severity==='High').length ?? 0,
        mediumSeverity: lastScanResult?.anomalies?.filter(a => a.severity==='Medium').length ?? 0,
        lowSeverity:    lastScanResult?.anomalies?.filter(a => a.severity==='Low').length ?? 0,
        cloudSends:     ledger.filter(e => e.action==='SEND').length,
      }
    };
  } catch (err) { return { error: err.message }; }
});

ipcMain.handle('export-report-pdf', async () => {
  try {
    const devices = lastScanResult?.devices || [];
    const alerts  = triggeredAlerts.slice(-50);
    const summary = {
      totalDevices: devices.length,
      unknown:      devices.filter(d => !deviceMeta[d.mac || d.ip]?.trust && (!d.trust || d.trust === 'unknown')).length,
      highSeverity: lastScanResult?.anomalies?.filter(a => a.severity === 'High').length ?? 0,
      alertCount:   alerts.length,
    };
    const ts      = new Date().toLocaleString();
    const dateStr = new Date().toISOString().slice(0, 10);

    function esc(s) {
      return String(s ?? '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    }

    const deviceRows = devices.map(d => {
      const meta  = deviceMeta[d.mac || d.ip] || {};
      const name  = meta.customName || d.hostname || d.name || d.ip;
      const conf  = d.confidence ? ` (${d.confidence}%)` : '';
      const ports = (d.ports || []).slice(0, 12).join(', ') + ((d.ports?.length ?? 0) > 12 ? '…' : '');
      return `<tr><td>${esc(name)}</td><td>${esc(d.ip)}</td><td>${esc(d.mac || '—')}</td><td>${esc(d.vendor || '—')}</td><td>${esc((d.deviceType || '—') + conf)}</td><td>${esc(meta.trust || d.trust || 'unknown')}</td><td>${esc(ports || '—')}</td></tr>`;
    }).join('');

    const alertRows = alerts.map(a =>
      `<tr><td>${esc(new Date(a.timestamp).toLocaleString())}</td><td>${esc(a.type || a.title)}</td><td>${esc(a.deviceIp || '—')}</td><td>${esc(a.message)}</td></tr>`
    ).join('');

    const html = `<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>Transparency Report</title>
<style>
  body{font-family:-apple-system,Arial,sans-serif;font-size:11pt;color:#1a1a1a;margin:18mm}
  h1{font-size:20pt;margin-bottom:4px}
  h2{font-size:13pt;margin-top:22px;margin-bottom:8px;border-bottom:2px solid #4f6df5;padding-bottom:4px}
  .meta{font-size:9pt;color:#555;margin-bottom:20px}
  .stats{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:20px}
  .stat-box{border:1px solid #ddd;border-radius:6px;padding:12px 16px}
  .stat-val{font-size:24pt;font-weight:700;color:#4f6df5}
  .stat-lbl{font-size:9pt;color:#666;margin-top:2px}
  table{width:100%;border-collapse:collapse;font-size:9pt}
  th{background:#f0f2ff;text-align:left;padding:6px 8px;border-bottom:2px solid #c7cffe}
  td{padding:5px 8px;border-bottom:1px solid #eee;vertical-align:top;word-break:break-word}
  tr:nth-child(even) td{background:#fafafa}
  .footer{margin-top:28px;font-size:8pt;color:#999;text-align:center;border-top:1px solid #ddd;padding-top:10px}
</style></head><body>
<h1>Transparency</h1>
<div class="meta">Network Security Report &nbsp;|&nbsp; Generated: ${esc(ts)} &nbsp;|&nbsp; Version 2.2.0</div>
<h2>Summary</h2>
<div class="stats">
  <div class="stat-box"><div class="stat-val">${summary.totalDevices}</div><div class="stat-lbl">Total Devices</div></div>
  <div class="stat-box"><div class="stat-val">${summary.unknown}</div><div class="stat-lbl">Unknown Devices</div></div>
  <div class="stat-box"><div class="stat-val">${summary.highSeverity}</div><div class="stat-lbl">High Severity Anomalies</div></div>
  <div class="stat-box"><div class="stat-val">${summary.alertCount}</div><div class="stat-lbl">Recent Alerts</div></div>
</div>
<h2>Devices (${devices.length})</h2>
<table><thead><tr><th>Name / Hostname</th><th>IP</th><th>MAC</th><th>Vendor</th><th>Type (Conf.)</th><th>Trust</th><th>Open Ports</th></tr></thead>
<tbody>${deviceRows || '<tr><td colspan="7" style="text-align:center;color:#999">No devices in last scan</td></tr>'}</tbody></table>
<h2>Alerts — Last 50 (${alerts.length})</h2>
<table><thead><tr><th>Time</th><th>Event</th><th>Device</th><th>Details</th></tr></thead>
<tbody>${alertRows || '<tr><td colspan="4" style="text-align:center;color:#999">No alerts recorded</td></tr>'}</tbody></table>
<div class="footer">Generated by Transparency v2.2.0 — all data local, zero telemetry</div>
</body></html>`;

    const win = new BrowserWindow({ show: false, webPreferences: { offscreen: true } });
    await win.loadURL('data:text/html;charset=utf-8,' + encodeURIComponent(html));
    const pdfBuf = await win.webContents.printToPDF({ printBackground: false, pageSize: 'A4' });
    win.close();

    const { filePath, canceled } = await dialog.showSaveDialog(mainWindow, {
      defaultPath: `transparency-report-${dateStr}.pdf`,
      filters: [{ name: 'PDF', extensions: ['pdf'] }],
    });
    if (canceled || !filePath) return { cancelled: true };
    await fs.promises.writeFile(filePath, pdfBuf);
    return { success: true, filePath };
  } catch (err) { return { error: err.message }; }
});

// ── IPC: Data management ──────────────────────────────────────────────────────
ipcMain.handle('get-data-stats', async () => {
  let cloudSends = 0;
  try {
    const ledger = await cloud('/ledger');
    cloudSends = ledger.filter(e => e.action === 'SEND').length;
  } catch { /* ignore */ }
  return {
    deviceCount:   Object.keys(deviceHistory).length,
    metaCount:     Object.keys(deviceMeta).length,
    snapshotCount: snapshots.length,
    alertRuleCount: alertRules.length,
    alertCount:    triggeredAlerts.length,
    cloudSends,
  };
});

ipcMain.handle('wipe-local-data', async () => {
  deviceHistory = {};
  deviceMeta    = {};
  snapshots     = [];
  triggeredAlerts = [];
  alertDebounce   = {};
  latencyHistory  = {};
  lastSnapshot    = [];
  lastScanResult  = null;
  saveJSON('device-history.json', {});
  saveJSON('device-meta.json', {});
  saveJSON('snapshots.json', []);
  return { success: true };
});

ipcMain.handle('purge-history-older-than', async (_e, days) => {
  const cutoff = Date.now() - days * 86400000;
  let removed = 0;
  for (const key of Object.keys(deviceHistory)) {
    const h = deviceHistory[key];
    if (new Date(h.lastSeen).getTime() < cutoff) {
      delete deviceHistory[key];
      removed++;
    }
  }
  saveJSON('device-history.json', deviceHistory);
  return { success: true, removed };
});

// ── IPC: Continuous monitoring ────────────────────────────────────────────────
ipcMain.handle('start-monitoring', async (_e, config = {}) => {
  monitoringConfig = { ...monitoringConfig, ...config, enabled: true };
  saveJSON('monitoring-config.json', monitoringConfig);
  startMonitoring();
  return { success: true, config: monitoringConfig };
});

ipcMain.handle('stop-monitoring', async () => {
  stopMonitoring();
  return { success: true };
});

ipcMain.handle('get-monitor-status', async () => {
  return {
    enabled: monitoringConfig.enabled,
    intervalMinutes: monitoringConfig.intervalMinutes,
    quietHoursStart: monitoringConfig.quietHoursStart,
    quietHoursEnd:   monitoringConfig.quietHoursEnd,
    alertOnOutage:   monitoringConfig.alertOnOutage,
    alertOnGatewayMacChange: monitoringConfig.alertOnGatewayMacChange,
    alertOnDnsChange: monitoringConfig.alertOnDnsChange,
    alertOnHighLatency: monitoringConfig.alertOnHighLatency,
    highLatencyThresholdMs: monitoringConfig.highLatencyThresholdMs,
    internetStatus,
    isInQuietHours: isInQuietHours(),
    localApiPort: LOCAL_API_PORT,
  };
});

ipcMain.handle('update-monitor-config', async (_e, config) => {
  monitoringConfig = { ...monitoringConfig, ...config };
  saveJSON('monitoring-config.json', monitoringConfig);
  if (monitoringConfig.enabled && monitoringTimer) {
    // Restart with new interval
    clearInterval(monitoringTimer);
    const ms = Math.max(1, monitoringConfig.intervalMinutes) * 60 * 1000;
    monitoringTimer = setInterval(runMonitorScan, ms);
  }
  return { success: true, config: monitoringConfig };
});

ipcMain.handle('get-internet-status', async () => {
  await checkInternetConnectivity();
  return internetStatus;
});

// ── IPC: Open URL ─────────────────────────────────────────────────────────────
ipcMain.handle('open-external-url', async (_e, url) => {
  if (typeof url === 'string' && (url.startsWith('https://') || url.startsWith('http://'))) {
    await shell.openExternal(url);
    return { success: true };
  }
  return { success: false, error: 'Only HTTP/HTTPS URLs are permitted.' };
});

// ═══════════════════════════════════════════════════════════════════════
//  NEW BACKEND FEATURES — v2.2
// ═══════════════════════════════════════════════════════════════════════

// ── IPC: Port Scanner ─────────────────────────────────────────────────────────
ipcMain.handle('port-scan', async (_e, host, ports) => {
  if (!Array.isArray(ports) || ports.length === 0) return { open: [], error: 'No ports specified' };
  // Cap at 1024 ports per scan for safety
  const scanPorts = ports.slice(0, 1024);
  const BATCH = 50;
  const TIMEOUT = 1200;
  const open = [];

  for (let i = 0; i < scanPorts.length; i += BATCH) {
    const batch = scanPorts.slice(i, i + BATCH);
    const results = await Promise.allSettled(batch.map(port =>
      new Promise(resolve => {
        const sock = new net.Socket();
        let done = false;
        const finish = (isOpen) => {
          if (!done) { done = true; sock.destroy(); resolve({ port, open: isOpen }); }
        };
        sock.setTimeout(TIMEOUT);
        sock.on('connect', () => finish(true));
        sock.on('timeout', () => finish(false));
        sock.on('error', () => finish(false));
        sock.connect(port, host);
      })
    ));
    for (const r of results) {
      if (r.status === 'fulfilled' && r.value.open) open.push(r.value.port);
    }
  }
  return { open: open.sort((a,b) => a-b) };
});

// ── IPC: Wake-on-LAN ──────────────────────────────────────────────────────────
ipcMain.handle('send-wol', async (_e, mac) => {
  try {
    // Build magic packet: 6x 0xFF + 16x MAC
    const macBytes = mac.replace(/[:-]/g, '').match(/.{2}/g).map(h => parseInt(h, 16));
    if (macBytes.length !== 6) return { success: false, error: 'Invalid MAC address format' };
    const payload = Buffer.alloc(102);
    payload.fill(0xff, 0, 6);
    for (let i = 0; i < 16; i++) {
      Buffer.from(macBytes).copy(payload, 6 + i * 6);
    }
    await new Promise((resolve, reject) => {
      const dgram = require('dgram');
      const sock = dgram.createSocket('udp4');
      sock.once('error', reject);
      sock.bind(0, () => {
        sock.setBroadcast(true);
        sock.send(payload, 0, payload.length, 9, '255.255.255.255', (err) => {
          sock.close();
          err ? reject(err) : resolve();
        });
      });
    });
    return { success: true };
  } catch (err) {
    return { success: false, error: err.message };
  }
});

// ── IPC: Snapshot Import ──────────────────────────────────────────────────────
ipcMain.handle('import-snapshot', async (_e, data) => {
  try {
    const snapshot = {
      id: `snap-${Date.now()}`,
      name: data.name || `Imported ${new Date().toLocaleString()}`,
      createdAt: data.createdAt || new Date().toISOString(),
      devices: data.devices || [],
      anomalies: data.anomalies || [],
      imported: true,
    };
    snapshots.unshift(snapshot);
    if (snapshots.length > 20) snapshots = snapshots.slice(0, 20);
    saveJSON('snapshots.json', snapshots);
    return { success: true, snapshot };
  } catch (err) {
    return { success: false, error: err.message };
  }
});

// ── IPC: Schedules ────────────────────────────────────────────────────────────
let scheduledScans  = loadJSON('schedules.json', []);
let scheduleTimers  = {};

function computeNextRun(freq, time) {
  const [h, m] = time.split(':').map(Number);
  const now = new Date();
  const next = new Date(now);
  next.setHours(h, m, 0, 0);
  if (next <= now) {
    if (freq === 'hourly') next.setTime(now.getTime() + 3600000);
    else if (freq === 'daily') next.setDate(next.getDate() + 1);
    else if (freq === 'weekly') next.setDate(next.getDate() + 7);
  }
  return next.toISOString();
}

function scheduleNextRun(s) {
  if (scheduleTimers[s.id]) { clearTimeout(scheduleTimers[s.id]); delete scheduleTimers[s.id]; }
  if (!s.enabled) return;

  const nextMs = new Date(s.nextRun).getTime() - Date.now();
  if (nextMs < 0) {
    // Update nextRun
    s.nextRun = computeNextRun(s.freq, s.time);
    saveJSON('schedules.json', scheduledScans);
    return scheduleNextRun(s);
  }

  scheduleTimers[s.id] = setTimeout(async () => {
    console.log(`[schedule] Running ${s.mode} scan for "${s.name}"`);
    try {
      const devices   = await scanNetwork(msg => mainWindow?.webContents.send('scan-progress', msg), { mode: s.mode });
      const anomalies = analyzeAnomalies(devices, lastSnapshot);
      lastSnapshot    = devices;
      lastScanResult  = { devices, anomalies, scannedAt: new Date().toISOString(), mode: s.mode };
      updateDeviceHistory(devices);
      processAlertsForScan(devices, anomalies);
      runScriptHooks('scan_complete', { devices: devices.length, anomalies: anomalies.length, scheduleName: s.name });

      if (s.autoExport) {
        const exportData = {
          reportVersion: '2.2', generatedAt: new Date().toISOString(),
          scheduleName: s.name, scan: lastScanResult,
        };
        const downloadsPath = require('electron').app.getPath('downloads');
        const filename = path.join(downloadsPath, `transparency-report-${new Date().toISOString().slice(0,10)}-${s.name.replace(/\s+/g,'-')}.json`);
        fs.writeFileSync(filename, JSON.stringify(exportData, null, 2));
        console.log(`[schedule] Auto-exported to ${filename}`);
      }
    } catch (err) {
      console.error('[schedule]', err);
    }
    // Schedule next run
    s.nextRun = computeNextRun(s.freq, s.time);
    saveJSON('schedules.json', scheduledScans);
    scheduleNextRun(s);
  }, nextMs);
}

// Initialize schedule timers on startup
setTimeout(() => {
  for (const s of scheduledScans) {
    if (!s.nextRun) s.nextRun = computeNextRun(s.freq, s.time);
    scheduleNextRun(s);
  }
}, 5000);

ipcMain.handle('get-schedules', async () => scheduledScans);

ipcMain.handle('create-schedule', async (_e, data) => {
  const s = {
    id: `sched-${Date.now()}`,
    name: data.name || 'Unnamed schedule',
    mode: data.mode || 'standard',
    freq: data.freq || 'daily',
    time: data.time || '03:00',
    autoExport: data.autoExport || false,
    enabled: true,
    createdAt: new Date().toISOString(),
    nextRun: computeNextRun(data.freq || 'daily', data.time || '03:00'),
  };
  scheduledScans.push(s);
  saveJSON('schedules.json', scheduledScans);
  scheduleNextRun(s);
  return { success: true, schedule: s };
});

ipcMain.handle('update-schedule', async (_e, id, updates) => {
  const idx = scheduledScans.findIndex(s => s.id === id);
  if (idx === -1) return { success: false };
  scheduledScans[idx] = { ...scheduledScans[idx], ...updates };
  saveJSON('schedules.json', scheduledScans);
  scheduleNextRun(scheduledScans[idx]);
  return { success: true };
});

ipcMain.handle('delete-schedule', async (_e, id) => {
  if (scheduleTimers[id]) { clearTimeout(scheduleTimers[id]); delete scheduleTimers[id]; }
  scheduledScans = scheduledScans.filter(s => s.id !== id);
  saveJSON('schedules.json', scheduledScans);
  return { success: true };
});

// ── IPC: Script Hooks ─────────────────────────────────────────────────────────
let scriptHooksData = loadJSON('script-hooks.json', []);

function runScriptHooks(event, payload = {}) {
  const hooks = scriptHooksData.filter(h => h.enabled && h.event === event);
  for (const h of hooks) {
    try {
      const jsonStr = JSON.stringify(payload);
      const cmd = process.platform === 'win32'
        ? `echo ${jsonStr} | ${h.cmd}`
        : `echo '${jsonStr.replace(/'/g, "'\\''")}' | ${h.cmd}`;
      exec(cmd, { timeout: 15000 }, (err) => {
        if (err) console.error(`[hook] "${h.cmd}" failed:`, err.message);
        else console.log(`[hook] "${h.cmd}" executed for event: ${event}`);
      });
    } catch (err) {
      console.error('[hook]', err.message);
    }
  }
}

// Fire hooks after scans
const _origScanNetworkHandler = null; // hooks are called from runMonitorScan and scan-network handler

ipcMain.handle('get-hooks', async () => scriptHooksData);

ipcMain.handle('create-hook', async (_e, data) => {
  const h = {
    id: `hook-${Date.now()}`,
    event:   data.event || 'scan_complete',
    cmd:     data.cmd || '',
    desc:    data.desc || '',
    enabled: data.enabled !== false,
    createdAt: new Date().toISOString(),
  };
  scriptHooksData.push(h);
  saveJSON('script-hooks.json', scriptHooksData);
  return { success: true, hook: h };
});

ipcMain.handle('update-hook', async (_e, id, updates) => {
  const idx = scriptHooksData.findIndex(h => h.id === id);
  if (idx === -1) return { success: false };
  scriptHooksData[idx] = { ...scriptHooksData[idx], ...updates };
  saveJSON('script-hooks.json', scriptHooksData);
  return { success: true };
});

ipcMain.handle('delete-hook', async (_e, id) => {
  scriptHooksData = scriptHooksData.filter(h => h.id !== id);
  saveJSON('script-hooks.json', scriptHooksData);
  return { success: true };
});

// ── IPC: API Key management ───────────────────────────────────────────────────
let apiKeyStore = loadJSON('api-key.json', { key: null });

function getOrCreateApiKey() {
  if (!apiKeyStore.key) {
    apiKeyStore.key = require('crypto').randomBytes(24).toString('hex');
    saveJSON('api-key.json', apiKeyStore);
  }
  return apiKeyStore.key;
}

// Initialize API key
getOrCreateApiKey();

// Local REST API with API-key authentication (started via restartLocalApiWithAuth on app ready)
// Note: We add API key auth to new requests via a middleware check below
// The existing localApiServer handles all routes; we add a check in the handler

ipcMain.handle('get-api-key', async () => ({ key: getOrCreateApiKey() }));

ipcMain.handle('rotate-api-key', async () => {
  apiKeyStore.key = require('crypto').randomBytes(24).toString('hex');
  saveJSON('api-key.json', apiKeyStore);
  return { key: apiKeyStore.key };
});

// ── Enhanced local API with API key auth ──────────────────────────────────────
// Restart local API with key authentication support
function restartLocalApiWithAuth() {
  if (localApiServer) {
    localApiServer.close();
    localApiServer = null;
  }

  localApiServer = require('http').createServer((req, res) => {
    res.setHeader('Content-Type', 'application/json');
    res.setHeader('Access-Control-Allow-Origin', '127.0.0.1');

    if (req.method !== 'GET') {
      res.writeHead(405);
      return res.end(JSON.stringify({ error: 'Read-only API' }));
    }

    // API key authentication (skip for root/health)
    if (req.url !== '/' && req.url !== '/api/health') {
      const reqKey = req.headers['x-api-key'];
      if (apiKeyStore.key && reqKey !== apiKeyStore.key) {
        res.writeHead(401);
        return res.end(JSON.stringify({ error: 'Unauthorized — include X-API-Key header', hint: 'Get your key from the Privacy tab in Transparency.' }));
      }
    }

    if (req.url === '/api/devices') {
      res.writeHead(200);
      return res.end(JSON.stringify(lastSnapshot.map(d => ({
        ip: d.ip, mac: d.mac, name: d.name, hostname: d.hostname,
        type: d.deviceType, vendor: d.vendor, ports: d.ports,
        meta: getMeta(getDeviceKey(d)), history: deviceHistory[getDeviceKey(d)] || null,
      }))));
    }
    if (req.url === '/api/status') {
      res.writeHead(200);
      return res.end(JSON.stringify({
        deviceCount: lastSnapshot.length, internetStatus,
        monitoring: { enabled: monitoringConfig.enabled, intervalMinutes: monitoringConfig.intervalMinutes },
        version: '2.2',
      }));
    }
    if (req.url === '/api/alerts') {
      res.writeHead(200);
      return res.end(JSON.stringify(triggeredAlerts.slice(0, 100)));
    }
    if (req.url === '/api/snapshots') {
      res.writeHead(200);
      return res.end(JSON.stringify(snapshots.map(s => ({ id: s.id, name: s.name, createdAt: s.createdAt, deviceCount: s.devices?.length || 0 }))));
    }
    if (req.url === '/api/health') {
      res.writeHead(200);
      return res.end(JSON.stringify({ status: 'ok', version: '2.2', uptime: process.uptime() }));
    }
    res.writeHead(404);
    res.end(JSON.stringify({ error: 'Not found', routes: ['/api/devices', '/api/status', '/api/alerts', '/api/snapshots', '/api/health'] }));
  });

  localApiServer.listen(LOCAL_API_PORT, '127.0.0.1', () =>
    console.log(`[local-api] http://127.0.0.1:${LOCAL_API_PORT} (API key auth enabled)`));
}

// Script hooks are fired from the existing scan-network handler above
// and from runMonitorScan — no duplicate handler needed here.

module.exports = {
  loadJSON,
  saveJSON,
  ensureDataDir,
  dataFile
};
