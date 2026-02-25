const scanBtn = document.getElementById('scanBtn');
const fixAllBtn = document.getElementById('fixAllBtn');
const exportBtn = document.getElementById('exportBtn');
const loading = document.getElementById('loading');
const results = document.getElementById('results');

// Check if electronAPI is available
if (!window.electronAPI) {
  console.error('electronAPI not found. Make sure preload.js is loaded correctly.');
}

scanBtn.addEventListener('click', async () => {
  loading.style.display = 'block';
  results.style.display = 'none';
  
  try {
    const scanResults = await window.electronAPI.scanSystem();
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
  threatsDiv.innerHTML = '';
  data.threats.forEach((threat, idx) => {
    const threatItem = document.createElement('div');
    threatItem.className = `threat-item threat-${threat.risk.toLowerCase()}`;
    threatItem.innerHTML = `
      <strong>${threat.name}</strong><br>
      <small>Risk: ${threat.risk} | Status: ${threat.status}</small><br>
      <small>${threat.description}</small>
      <button class="btn btn-small fix-btn" style="margin-top: 0.5rem;">Fix Now</button>
    `;
    
    threatItem.addEventListener('click', () => {
      showThreatDetail(threat.name);
    });
    
    const fixBtn = threatItem.querySelector('.fix-btn');
    fixBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      fixThreat(threat.name);
    });
    
    threatsDiv.appendChild(threatItem);
  });
  
  // Devices & Connections (mock data for now)
  devicesDiv.innerHTML = '<div class="device-item">No USB/Bluetooth devices detected</div>';
  connectionsDiv.innerHTML = '<div class="connection-item">No suspicious connections detected</div>';
  
  results.style.display = 'block';
}

async function fixThreat(threatName) {
  alert(`Applying fixes for ${threatName}...`);
  try {
    await window.electronAPI.fixThreat(threatName);
  } catch (error) {
    console.error('Fix error:', error);
  }
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
