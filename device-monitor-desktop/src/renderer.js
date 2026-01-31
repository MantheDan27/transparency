const { ipcRenderer } = require('electron');

const scanBtn = document.getElementById('scanBtn');
const fixAllBtn = document.getElementById('fixAllBtn');
const exportBtn = document.getElementById('exportBtn');
const loading = document.getElementById('loading');
const results = document.getElementById('results');

scanBtn.addEventListener('click', async () => {
  loading.style.display = 'block';
  results.style.display = 'none';
  
  try {
    const scanResults = await ipcRenderer.invoke('scan-system');
    displayResults(scanResults);
  } catch (error) {
    console.error('Scan error:', error);
  } finally {
    loading.style.display = 'none';
  }
});

function displayResults(data) {
  const systemDetailsDiv = document.getElementById('systemDetails');
  const threatsDiv = document.getElementById('threatsList');
  const devicesDiv = document.getElementById('devicesList');
  const connectionsDiv = document.getElementById('connectionsList');
  
  // System Info
  systemDetailsDiv.innerHTML = `
    <div class="info-item"><strong>Platform:</strong> ${data.platform}</div>
    <div class="info-item"><strong>OS Version:</strong> ${data.osVersion}</div>
    <div class="info-item"><strong>CPU Cores:</strong> ${data.cpus}</div>
    <div class="info-item"><strong>Architecture:</strong> ${data.arch}</div>
    <div class="info-item"><strong>Total Memory:</strong> ${data.totalMemory}</div>
    <div class="info-item"><strong>Free Memory:</strong> ${data.freeMemory}</div>
    <div class="info-item"><strong>Hostname:</strong> ${data.hostname}</div>
  `;
  
  // Threats
  threatsDiv.innerHTML = data.threats.map((threat, idx) => `
    <div class="threat-item threat-${threat.risk.toLowerCase()}" onclick="showThreatDetail('${threat.name}')">
      <strong>${threat.name}</strong><br>
      <small>Risk: ${threat.risk} | Status: ${threat.status}</small><br>
      <small>${threat.description}</small>
      <button class="btn btn-small" onclick="fixThreat('${threat.name}', event)" style="margin-top: 0.5rem;">Fix Now</button>
    </div>
  `).join('');
  
  // Devices & Connections (mock data for now)
  devicesDiv.innerHTML = '<div class="device-item">No USB/Bluetooth devices detected</div>';
  connectionsDiv.innerHTML = '<div class="connection-item">No suspicious connections detected</div>';
  
  results.style.display = 'block';
}

function fixThreat(threatName, event) {
  event.stopPropagation();
  alert(`Applying fixes for ${threatName}...`);
  ipcRenderer.invoke('fix-threat', threatName);
}

function showThreatDetail(threatName) {
  alert(`Details for ${threatName}\n\nThis application monitors system processes and network activity for security threats.`);
}

fixAllBtn.addEventListener('click', async () => {
  alert('Fixing all detected issues...');
});

exportBtn.addEventListener('click', () => {
  alert('Exporting report...');
});
