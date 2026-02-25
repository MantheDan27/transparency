const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  scanSystem: () => ipcRenderer.invoke('scan-system'),
  fixThreat: (threatId) => ipcRenderer.invoke('fix-threat', threatId)
});
