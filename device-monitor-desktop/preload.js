const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  scanNetwork: () => ipcRenderer.invoke('scan-network'),
  getCloudLedger: () => ipcRenderer.invoke('get-cloud-ledger'),
  deleteCloudData: () => ipcRenderer.invoke('delete-cloud-data'),
  enrichDevice: (device) => ipcRenderer.invoke('enrich-device', device)
});
