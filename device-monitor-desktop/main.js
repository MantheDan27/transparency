'use strict';

const { app, BrowserWindow, ipcMain, shell } = require('electron');
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
  startLocalApi();
  createWindow();
  // Resume monitoring if it was enabled before
  if (monitoringConfig.enabled) {
    // Delay slightly to let window load first
    setTimeout(() => startMonitoring(), 3000);
  }
  // Initialize DNS baseline
  lastDnsServers = dns.getServers().sort().join(',');
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
  if (typeof url === 'string' && url.startsWith('https://')) {
    await shell.openExternal(url);
    return { success: true };
  }
  return { success: false, error: 'Only HTTPS URLs are permitted.' };
});
