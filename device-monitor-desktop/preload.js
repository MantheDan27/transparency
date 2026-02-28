const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  // Core scan
  scanNetwork:        ()         => ipcRenderer.invoke('scan-network'),
  enrichDevice:       (device)   => ipcRenderer.invoke('enrich-device', device),

  // Cloud ledger & devices
  getCloudLedger:     ()         => ipcRenderer.invoke('get-cloud-ledger'),
  getCloudDevices:    ()         => ipcRenderer.invoke('get-cloud-devices'),
  deleteCloudData:    ()         => ipcRenderer.invoke('delete-cloud-data'),
  deleteDeviceData:   (ip)       => ipcRenderer.invoke('delete-device-data', ip),

  // Local snapshot management
  clearLocalSnapshot: ()         => ipcRenderer.invoke('clear-local-snapshot'),
  getLocalDevices:    ()         => ipcRenderer.invoke('get-local-devices'),
  deleteLocalDevice:  (ip)       => ipcRenderer.invoke('delete-local-device', ip),

  // Cloud data preview (before enrichment)
  getCloudPreview:    (device)   => ipcRenderer.invoke('get-cloud-preview', device),

  // Report export
  exportReport:       ()         => ipcRenderer.invoke('export-report'),

  // Open external URL in OS browser
  openExternalUrl:    (url)      => ipcRenderer.invoke('open-external-url', url),

  // Scan progress events
  onScanProgress: (cb) => {
    const handler = (_e, msg) => cb(msg);
    ipcRenderer.on('scan-progress', handler);
    return () => ipcRenderer.removeListener('scan-progress', handler);
  }
});
