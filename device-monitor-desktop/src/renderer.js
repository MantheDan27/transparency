document.addEventListener('DOMContentLoaded', () => {
    const navItems = document.querySelectorAll('.nav-item');
    const tabContents = document.querySelectorAll('.tab-content');
    const scanBtn = document.getElementById('scanBtn');
    const deviceListBody = document.getElementById('deviceListBody');
    const anomaliesList = document.getElementById('anomaliesList');
    const ledgerList = document.getElementById('ledgerList');
    const refreshLedgerBtn = document.getElementById('refreshLedgerBtn');
    const deleteDataBtn = document.getElementById('deleteDataBtn');
    const totalDevicesCount = document.getElementById('totalDevicesCount');
    const activeThreatsCount = document.getElementById('activeThreatsCount');
    const cloudSyncCount = document.getElementById('cloudSyncCount');

    // Tab Switching
    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const targetTab = item.getAttribute('data-tab');
            
            navItems.forEach(ni => ni.classList.remove('active'));
            tabContents.forEach(tc => tc.classList.remove('active'));
            
            item.classList.add('active');
            document.getElementById(targetTab).classList.add('active');
            
            if (targetTab === 'ledger') {
                updateLedger();
            }
        });
    });

    // Scanning Logic
    scanBtn.addEventListener('click', async () => {
        scanBtn.disabled = true;
        scanBtn.textContent = 'Scanning...';
        
        try {
            const result = await window.electronAPI.scanNetwork();
            if (result.error) {
                alert('Scan Error: ' + result.error);
                return;
            }
            
            displayDevices(result.devices);
            displayAnomalies(result.anomalies);
            
            totalDevicesCount.textContent = result.devices.length;
            activeThreatsCount.textContent = result.anomalies.length;
            
        } catch (error) {
            console.error('Scan UI Error:', error);
        } finally {
            scanBtn.disabled = false;
            scanBtn.textContent = '🔍 Start Network Scan';
        }
    });

    function displayDevices(devices) {
        deviceListBody.innerHTML = '';
        devices.forEach(device => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${device.ip}</td>
                <td>${device.mac}</td>
                <td>${device.name}</td>
                <td>${device.ports.join(', ')}</td>
                <td><button class="btn btn-secondary btn-small enrich-btn" data-ip="${device.ip}">🛡️ Enrich</button></td>
            `;
            
            row.querySelector('.enrich-btn').addEventListener('click', () => enrichDevice(device));
            deviceListBody.appendChild(row);
        });
    }

    function displayAnomalies(anomalies) {
        anomaliesList.innerHTML = '';
        if (anomalies.length === 0) {
            anomaliesList.innerHTML = '<p class="empty-msg">No anomalies detected. Your network looks safe!</p>';
            return;
        }

        anomalies.forEach(anomaly => {
            const card = document.createElement('div');
            card.className = 'anomaly-card';
            const sevClass = `sev-${anomaly.severity.toLowerCase()}`;
            
            card.innerHTML = `
                <div class="anomaly-severity ${sevClass}">${anomaly.severity[0]}</div>
                <div class="anomaly-info">
                    <h3>${anomaly.type} on ${anomaly.device}</h3>
                    <p>${anomaly.description}</p>
                </div>
            `;
            anomaliesList.appendChild(card);
        });
    }

    // Enrichment (Cloud Interaction)
    async function enrichDevice(device) {
        const result = await window.electronAPI.enrichDevice(device);
        
        const modal = document.getElementById('enrichModal');
        const modalBody = document.getElementById('modalBody');
        const modalTitle = document.getElementById('modalTitle');
        
        modalTitle.textContent = `Security Guidance for ${device.ip}`;
        modalBody.innerHTML = `
            <div style="background:rgba(0,212,255,0.05); padding:1.5rem; border-radius:8px; border-left:4px solid #00d4ff;">
                <p style="font-size:1.1rem; color:#fff; margin-bottom:1rem;">Risk Level: <strong>${result.riskLevel || 'Unknown'}</strong></p>
                <p>${result.guidance}</p>
            </div>
            <p style="margin-top:1.5rem; font-size:0.85rem; color:#888;">This analysis was performed by our mock cloud transparency service.</p>
        `;
        
        modal.style.display = 'block';
        updateSyncCount();
    }

    // Ledger Management
    async function updateLedger() {
        const ledger = await window.electronAPI.getCloudLedger();
        ledgerList.innerHTML = '';
        
        if (ledger.length === 0) {
            ledgerList.innerHTML = '<li>No activity recorded.</li>';
            return;
        }

        ledger.forEach(entry => {
            const li = document.createElement('li');
            const date = new Date(entry.timestamp).toLocaleTimeString();
            let msg = `[${date}] `;
            
            if (entry.action === 'enrich') {
                msg += `SENT: Metadata for device ${entry.device.ip} sent for analysis.`;
            } else if (entry.action === 'delete_all') {
                msg += `DELETED: All user history purged from cloud storage.`;
            }
            
            li.textContent = msg;
            ledgerList.appendChild(li);
        });
    }

    refreshLedgerBtn.addEventListener('click', updateLedger);

    // Settings / Data Deletion
    deleteDataBtn.addEventListener('click', async () => {
        if (confirm('Are you sure you want to delete all your history from the cloud?')) {
            const result = await window.electronAPI.deleteCloudData();
            if (result.success) {
                alert('Success: All cloud data has been deleted.');
                updateLedger();
            } else {
                alert('Error deleting data: ' + result.error);
            }
        }
    });

    // KPI Helpers
    async function updateSyncCount() {
        const ledger = await window.electronAPI.getCloudLedger();
        const syncs = ledger.filter(e => e.action === 'enrich').length;
        cloudSyncCount.textContent = syncs;
    }

    // Modal Close
    document.querySelector('.close').onclick = function() {
        document.getElementById('enrichModal').style.display = 'none';
    }

    window.onclick = function(event) {
        const modal = document.getElementById('enrichModal');
        if (event.target == modal) {
            modal.style.display = 'none';
        }
    }
    
    // Initial data
    updateSyncCount();
});
