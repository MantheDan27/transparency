const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  // ── Scanning ────────────────────────────────────────────────────────────────
  scanNetwork:        (opts)     => ipcRenderer.invoke('scan-network', opts),
  enrichDevice:       (device)   => ipcRenderer.invoke('enrich-device', device),

  // ── Device metadata (trust state, tags, custom name, notes) ─────────────────
  getDeviceMeta:      (key)      => ipcRenderer.invoke('get-device-meta', key),
  setDeviceMeta:      (key, upd) => ipcRenderer.invoke('set-device-meta', key, upd),
  getAllDeviceMeta:    ()         => ipcRenderer.invoke('get-all-device-meta'),

  // ── Device history ───────────────────────────────────────────────────────────
  getDeviceHistory:   (key)      => ipcRenderer.invoke('get-device-history', key),
  getAllDeviceHistory: ()         => ipcRenderer.invoke('get-all-device-history'),
  getLatencyHistory:  (ip)       => ipcRenderer.invoke('get-latency-history', ip),

  // ── Alerts ──────────────────────────────────────────────────────────────────
  getAlerts:          ()         => ipcRenderer.invoke('get-alerts'),
  acknowledgeAlert:   (id)       => ipcRenderer.invoke('acknowledge-alert', id),
  dismissAlert:       (id)       => ipcRenderer.invoke('dismiss-alert', id),
  clearAllAlerts:     ()         => ipcRenderer.invoke('clear-all-alerts'),
  getAlertRules:      ()         => ipcRenderer.invoke('get-alert-rules'),
  createAlertRule:    (rule)     => ipcRenderer.invoke('create-alert-rule', rule),
  updateAlertRule:    (id, upd)  => ipcRenderer.invoke('update-alert-rule', id, upd),
  deleteAlertRule:    (id)       => ipcRenderer.invoke('delete-alert-rule', id),

  // ── Snapshots ────────────────────────────────────────────────────────────────
  saveSnapshot:       (name)     => ipcRenderer.invoke('save-snapshot', name),
  getSnapshots:       ()         => ipcRenderer.invoke('get-snapshots'),
  deleteSnapshot:     (id)       => ipcRenderer.invoke('delete-snapshot', id),

  // ── Diagnostic tools ────────────────────────────────────────────────────────
  pingHost:           (h, n)     => ipcRenderer.invoke('ping-host', h, n),
  tracerouteHost:     (h)        => ipcRenderer.invoke('traceroute-host', h),
  dnsLookup:          (h, t)     => ipcRenderer.invoke('dns-lookup', h, t),
  tcpConnectTest:     (h, p, t)  => ipcRenderer.invoke('tcp-connect-test', h, p, t),
  httpTest:           (url, t)   => ipcRenderer.invoke('http-test', url, t),

  // ── Network info ────────────────────────────────────────────────────────────
  getWifiInfo:        ()         => ipcRenderer.invoke('get-wifi-info'),
  getGatewayInfo:     ()         => ipcRenderer.invoke('get-gateway-info'),
  getNetworkInfo:     ()         => ipcRenderer.invoke('get-network-info'),

  // ── Cloud ledger & enrichment ────────────────────────────────────────────────
  getCloudLedger:     ()         => ipcRenderer.invoke('get-cloud-ledger'),
  getCloudDevices:    ()         => ipcRenderer.invoke('get-cloud-devices'),
  deleteCloudData:    ()         => ipcRenderer.invoke('delete-cloud-data'),
  deleteDeviceData:   (ip)       => ipcRenderer.invoke('delete-device-data', ip),
  getCloudPreview:    (device)   => ipcRenderer.invoke('get-cloud-preview', device),

  // ── Local snapshot management ────────────────────────────────────────────────
  clearLocalSnapshot: ()         => ipcRenderer.invoke('clear-local-snapshot'),
  getLocalDevices:    ()         => ipcRenderer.invoke('get-local-devices'),
  deleteLocalDevice:  (ip)       => ipcRenderer.invoke('delete-local-device', ip),

  // ── Data management ──────────────────────────────────────────────────────────
  getDataStats:       ()         => ipcRenderer.invoke('get-data-stats'),
  wipeLocalData:      ()         => ipcRenderer.invoke('wipe-local-data'),
  purgeHistoryOlderThan: (days)  => ipcRenderer.invoke('purge-history-older-than', days),
  exportReport:       ()         => ipcRenderer.invoke('export-report'),
  exportReportPDF:    ()         => ipcRenderer.invoke('export-report-pdf'),

  // ── Continuous monitoring ────────────────────────────────────────────────────
  startMonitoring:    (cfg)      => ipcRenderer.invoke('start-monitoring', cfg),
  stopMonitoring:     ()         => ipcRenderer.invoke('stop-monitoring'),
  getMonitorStatus:   ()         => ipcRenderer.invoke('get-monitor-status'),
  updateMonitorConfig: (cfg)     => ipcRenderer.invoke('update-monitor-config', cfg),
  getInternetStatus:  ()         => ipcRenderer.invoke('get-internet-status'),

  // ── Open external URL ────────────────────────────────────────────────────────
  openExternalUrl:    (url)      => ipcRenderer.invoke('open-external-url', url),

  // ── Port scanner & WOL ───────────────────────────────────────────────────────
  portScan:           (h, ports) => ipcRenderer.invoke('port-scan', h, ports),
  sendWOL:            (mac)      => ipcRenderer.invoke('send-wol', mac),

  // ── Snapshot import ──────────────────────────────────────────────────────────
  importSnapshot:     (data)     => ipcRenderer.invoke('import-snapshot', data),

  // ── Scheduled scans ──────────────────────────────────────────────────────────
  getSchedules:       ()         => ipcRenderer.invoke('get-schedules'),
  createSchedule:     (data)     => ipcRenderer.invoke('create-schedule', data),
  updateSchedule:     (id, upd)  => ipcRenderer.invoke('update-schedule', id, upd),
  deleteSchedule:     (id)       => ipcRenderer.invoke('delete-schedule', id),

  // ── Script hooks ─────────────────────────────────────────────────────────────
  getHooks:           ()         => ipcRenderer.invoke('get-hooks'),
  createHook:         (data)     => ipcRenderer.invoke('create-hook', data),
  updateHook:         (id, upd)  => ipcRenderer.invoke('update-hook', id, upd),
  deleteHook:         (id)       => ipcRenderer.invoke('delete-hook', id),

  // ── API key management ────────────────────────────────────────────────────────
  getApiKey:          ()         => ipcRenderer.invoke('get-api-key'),
  rotateApiKey:       ()         => ipcRenderer.invoke('rotate-api-key'),

  // ── Events (renderer ← main) ─────────────────────────────────────────────────
  onScanProgress: cb => {
    const h = (_e, msg) => cb(msg);
    ipcRenderer.on('scan-progress', h);
    return () => ipcRenderer.removeListener('scan-progress', h);
  },
  onNewAlert: cb => {
    const h = (_e, alert) => cb(alert);
    ipcRenderer.on('new-alert', h);
    return () => ipcRenderer.removeListener('new-alert', h);
  },
  onMonitorScanComplete: cb => {
    const h = (_e, data) => cb(data);
    ipcRenderer.on('monitor-scan-complete', h);
    return () => ipcRenderer.removeListener('monitor-scan-complete', h);
  },
  onMonitorStatus: cb => {
    const h = (_e, data) => cb(data);
    ipcRenderer.on('monitor-status', h);
    return () => ipcRenderer.removeListener('monitor-status', h);
  },
  onInternetStatus: cb => {
    const h = (_e, data) => cb(data);
    ipcRenderer.on('internet-status', h);
    return () => ipcRenderer.removeListener('internet-status', h);
  },
});
