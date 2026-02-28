const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  scanNetwork:        ()       => ipcRenderer.invoke('scan-network'),
  enrichDevice:       (device) => ipcRenderer.invoke('enrich-device', device),
  getCloudLedger:     ()       => ipcRenderer.invoke('get-cloud-ledger'),
  getCloudDevices:    ()       => ipcRenderer.invoke('get-cloud-devices'),
  deleteCloudData:    ()       => ipcRenderer.invoke('delete-cloud-data'),
  deleteDeviceData:   (ip)     => ipcRenderer.invoke('delete-device-data', ip),
  clearLocalSnapshot: ()       => ipcRenderer.invoke('clear-local-snapshot'),

  onScanProgress: (cb) => {
    const handler = (_e, msg) => cb(msg);
    ipcRenderer.on('scan-progress', handler);
    return () => ipcRenderer.removeListener('scan-progress', handler);
  }
});
