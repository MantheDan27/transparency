'use strict';

/* ═══════════════════════════════════════════════════════════════════════════
   Transparency — Renderer Process
   ═══════════════════════════════════════════════════════════════════════════ */

// Risky ports (must match scanner.js)
const RISKY_PORTS = new Set([23, 135, 139, 445, 3389, 5900]);
const PORT_NAMES  = {
  21:'FTP', 22:'SSH', 23:'Telnet', 25:'SMTP', 53:'DNS',
  80:'HTTP', 110:'POP3', 135:'MS-RPC', 139:'NetBIOS',
  443:'HTTPS', 445:'SMB', 3306:'MySQL', 3389:'RDP',
  5900:'VNC', 8080:'HTTP-Alt', 8443:'HTTPS-Alt'
};

// ── State ─────────────────────────────────────────────────────────────────────
let allAnomalies   = [];
let currentFilter  = 'all';
let deviceAnomalyMap = {};  // ip -> highest severity string

// ── DOM refs ──────────────────────────────────────────────────────────────────
const $ = id => document.getElementById(id);

document.addEventListener('DOMContentLoaded', () => {

  // ── Tab navigation ──────────────────────────────────────────────────────────
  document.querySelectorAll('.nav-item').forEach(btn => {
    btn.addEventListener('click', () => {
      const tab = btn.dataset.tab;
      document.querySelectorAll('.nav-item').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab').forEach(s => s.classList.remove('active'));
      btn.classList.add('active');
      $(`tab-${tab}`).classList.add('active');
      if (tab === 'ledger')   refreshLedger();
      if (tab === 'settings') populateDeviceSelect();
    });
  });

  // ── Scan ────────────────────────────────────────────────────────────────────
  $('scanBtn').addEventListener('click', runScan);

  async function runScan() {
    const btn = $('scanBtn');
    btn.disabled = true;
    btn.innerHTML = `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="16" height="16">
      <circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>
    </svg> Scanning…`;

    $('scanProgressBar').classList.remove('hidden');

    // Listen for progress events
    const removeListener = window.electronAPI.onScanProgress(msg => {
      $('progressText').textContent = msg;
    });

    try {
      const result = await window.electronAPI.scanNetwork();
      removeListener();

      if (result.error) {
        showToast('Scan failed: ' + result.error, 'error');
        return;
      }

      renderDevices(result.devices, result.anomalies);
      renderAnomalies(result.anomalies);
      updateKPIs(result.devices, result.anomalies);

      const now = new Date();
      $('kpiLastScan').textContent  = now.toLocaleTimeString();
      $('lastScanLabel').textContent = `Last scan: ${now.toLocaleTimeString()} — ${result.devices.length} device(s) found`;
    } catch (err) {
      removeListener();
      showToast('Scan error: ' + err.message, 'error');
    } finally {
      $('scanProgressBar').classList.add('hidden');
      btn.disabled = false;
      btn.innerHTML = `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="16" height="16">
        <circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>
      </svg> Start Scan`;
    }
  }

  // ── KPIs ────────────────────────────────────────────────────────────────────
  function updateKPIs(devices, anomalies) {
    $('kpiDevices').textContent   = devices.length;
    $('kpiAnomalies').textContent = anomalies.length;
    $('deviceCountTag').textContent = `${devices.length} device${devices.length !== 1 ? 's' : ''}`;

    const badge = $('navAnomalyCount');
    if (anomalies.length > 0) {
      badge.textContent = anomalies.length;
      badge.hidden = false;
    } else {
      badge.hidden = true;
    }

    refreshSyncCount();
  }

  async function refreshSyncCount() {
    const ledger = await window.electronAPI.getCloudLedger();
    $('kpiSyncs').textContent = ledger.filter(e => e.action === 'SEND').length;
  }

  // ── Device table ─────────────────────────────────────────────────────────────
  function renderDevices(devices, anomalies) {
    // Build ip → risk map
    deviceAnomalyMap = {};
    const sevOrder = { High: 3, Medium: 2, Low: 1 };
    for (const a of anomalies) {
      const cur = deviceAnomalyMap[a.device];
      if (!cur || (sevOrder[a.severity] || 0) > (sevOrder[cur] || 0)) {
        deviceAnomalyMap[a.device] = a.severity;
      }
    }

    const tbody = $('deviceBody');
    if (devices.length === 0) {
      tbody.innerHTML = '<tr class="empty-row"><td colspan="6">No devices found. Try running the scan from a device connected to a LAN.</td></tr>';
      return;
    }

    tbody.innerHTML = devices.map(dev => {
      const sev     = deviceAnomalyMap[dev.ip];
      const riskBadge = sev
        ? `<span class="risk-badge risk-${sev.toLowerCase()}">${sev}</span>`
        : `<span class="risk-badge risk-safe">Safe</span>`;

      const macCell = dev.mac && dev.mac !== 'Unknown'
        ? `<span style="font-family:monospace;font-size:0.82rem">${dev.mac}</span>`
        : `<span class="mac-unknown">Unknown</span>`;

      const portBadges = dev.ports.length
        ? dev.ports.map(p => {
            const risky = RISKY_PORTS.has(p);
            const label = PORT_NAMES[p] ? `${p}<small style="opacity:.65"> ${PORT_NAMES[p]}</small>` : p;
            return `<span class="port-badge${risky ? ' risky' : ''}" title="Port ${p}${PORT_NAMES[p] ? ' — ' + PORT_NAMES[p] : ''}">${p}</span>`;
          }).join('')
        : '<span style="color:var(--text-muted);font-size:0.8rem">None</span>';

      return `
        <tr>
          <td style="font-family:monospace;font-size:0.84rem;color:var(--accent-light)">${dev.ip}</td>
          <td>${macCell}</td>
          <td style="color:var(--text-secondary);font-size:0.85rem">${dev.name}</td>
          <td>${portBadges}</td>
          <td>${riskBadge}</td>
          <td>
            <button class="btn btn-secondary btn-sm enrich-btn" data-ip="${dev.ip}">
              Enrich
            </button>
          </td>
        </tr>`;
    }).join('');

    // Bind enrich buttons
    tbody.querySelectorAll('.enrich-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const dev = devices.find(d => d.ip === btn.dataset.ip);
        if (dev) enrichDevice(dev);
      });
    });
  }

  // ── Anomalies ─────────────────────────────────────────────────────────────
  function renderAnomalies(anomalies) {
    allAnomalies = anomalies;

    // Update filter counts
    const counts = { High: 0, Medium: 0, Low: 0 };
    for (const a of anomalies) counts[a.severity] = (counts[a.severity] || 0) + 1;
    $('fAll').textContent    = anomalies.length;
    $('fHigh').textContent   = counts.High   || 0;
    $('fMedium').textContent = counts.Medium || 0;
    $('fLow').textContent    = counts.Low    || 0;

    $('anomalySubtitle').textContent = anomalies.length > 0
      ? `${anomalies.length} anomal${anomalies.length !== 1 ? 'ies' : 'y'} detected — click any card to view details and guidance.`
      : 'No anomalies detected. Your network appears healthy.';

    applyAnomalyFilter();
  }

  function applyAnomalyFilter() {
    const filtered = currentFilter === 'all'
      ? allAnomalies
      : allAnomalies.filter(a => a.severity === currentFilter);

    const container = $('anomaliesContainer');

    if (filtered.length === 0) {
      container.innerHTML = `
        <div class="empty-state">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" width="48" height="48">
            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
          </svg>
          <p>${allAnomalies.length === 0 ? 'No anomalies detected. Run a scan to check your network.' : 'No anomalies match this filter.'}</p>
        </div>`;
      return;
    }

    container.innerHTML = filtered.map((a, i) => buildAnomalyCard(a, i)).join('');

    // Bind expand toggles
    container.querySelectorAll('.anomaly-header').forEach(header => {
      header.addEventListener('click', () => {
        const card = header.closest('.anomaly-card');
        card.classList.toggle('expanded');
      });
    });

    // Bind per-anomaly delete buttons
    container.querySelectorAll('.delete-device-data-btn').forEach(btn => {
      btn.addEventListener('click', async () => {
        const ip = btn.dataset.ip;
        if (!confirm(`Delete all cloud data for ${ip}?`)) return;
        const res = await window.electronAPI.deleteDeviceData(ip);
        if (res.success) {
          showToast(`Deleted ${res.deleted} record(s) for ${ip}.`, 'success');
          refreshSyncCount();
        } else {
          showToast('Delete failed: ' + (res.error || 'unknown error'), 'error');
        }
      });
    });
  }

  function buildAnomalyCard(a, idx) {
    const steps = (a.steps || []).map(s => `<li>${escHtml(s)}</li>`).join('');
    return `
      <div class="anomaly-card sev-${a.severity}" data-id="${escHtml(a.id)}">
        <div class="anomaly-header">
          <span class="sev-badge sev-${a.severity}">${a.severity.toUpperCase()}</span>
          <div class="anomaly-summary">
            <div class="anomaly-type">${escHtml(a.type)}</div>
            <div class="anomaly-device">${escHtml(a.device)}</div>
            <div class="anomaly-brief">${escHtml(a.description)}</div>
          </div>
          <svg class="anomaly-chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="14" height="14">
            <polyline points="6 9 12 15 18 9"/>
          </svg>
        </div>
        <div class="anomaly-body">
          <div class="anomaly-body-inner">
            <div class="explain-section">
              <h4>What was detected</h4>
              <p>${escHtml(a.what || a.description)}</p>
            </div>
            <div class="explain-section">
              <h4>Why this matters</h4>
              <p>${escHtml(a.risk || 'Review this device carefully.')}</p>
            </div>
            ${steps ? `
            <div class="explain-section">
              <h4>Recommended actions</h4>
              <ol class="explain-steps">${steps}</ol>
            </div>` : ''}
            <div class="anomaly-actions">
              <button class="btn btn-danger btn-sm delete-device-data-btn" data-ip="${escHtml(a.device)}">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="13" height="13">
                  <polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6"/>
                </svg>
                Delete cloud data for ${escHtml(a.device)}
              </button>
            </div>
          </div>
        </div>
      </div>`;
  }

  // ── Anomaly filters ──────────────────────────────────────────────────────────
  document.querySelectorAll('.filter-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      currentFilter = btn.dataset.filter;
      applyAnomalyFilter();
    });
  });

  // ── Enrichment modal ─────────────────────────────────────────────────────────
  async function enrichDevice(device) {
    $('modalTitle').textContent = `Cloud Guidance — ${device.ip}`;
    $('modalBody').innerHTML = `<p style="color:var(--text-muted);text-align:center;padding:2rem">Fetching guidance…</p>`;
    $('enrichModal').classList.remove('hidden');

    const result = await window.electronAPI.enrichDevice(device);
    refreshSyncCount();

    const riskClass = {
      Critical: 'crit', High: 'high', Informational: 'safe'
    }[result.riskLevel] || 'info';

    const riskColor = {
      Critical: 'var(--danger)', High: 'var(--warning)',
      Informational: 'var(--success)', Unknown: 'var(--text-muted)'
    }[result.riskLevel] || 'var(--text-secondary)';

    const servicesHtml = (result.services || []).map(s => `
      <div class="service-block">
        <div class="service-block-header">
          <span class="service-port">:${s.port}</span>
          <span class="service-name">${escHtml(s.service)}</span>
          <span class="service-risk-pill ${s.riskLevel}">${s.riskLevel}</span>
        </div>
        <p class="service-guidance">${escHtml(s.guidance)}</p>
      </div>`).join('');

    $('modalBody').innerHTML = `
      <div class="modal-risk-header ${riskClass}">
        <div>
          <div class="modal-risk-label">Risk Assessment</div>
          <div class="modal-risk-value" style="color:${riskColor}">${escHtml(result.riskLevel || 'Unknown')}</div>
        </div>
      </div>
      <p class="modal-summary">${escHtml(result.summary || result.guidance)}</p>
      ${servicesHtml}
      <p class="modal-note">Analysis provided by the local Transparency cloud mock service. No data was sent to external servers. All guidance is defensive in nature.</p>
    `;
  }

  $('modalClose').addEventListener('click', () => $('enrichModal').classList.add('hidden'));
  $('enrichModal').addEventListener('click', e => {
    if (e.target === $('enrichModal')) $('enrichModal').classList.add('hidden');
  });
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape') $('enrichModal').classList.add('hidden');
  });

  // ── Data Ledger ──────────────────────────────────────────────────────────────
  async function refreshLedger() {
    const entries = await window.electronAPI.getCloudLedger();
    const list    = $('ledgerList');

    if (entries.length === 0) {
      list.innerHTML = '<li class="ledger-empty">No activity recorded yet.</li>';
      return;
    }

    list.innerHTML = entries.map(e => {
      const time  = new Date(e.timestamp).toLocaleTimeString();
      const dot   = e.action.toLowerCase().replace('_', '');
      const label = { SEND: 'SEND', DELETE_ALL: 'DELETE ALL', DELETE_DEVICE: 'DELETE DEVICE' }[e.action] || e.action;
      return `
        <li class="ledger-entry">
          <span class="ledger-dot ${e.action.toLowerCase()}"></span>
          <span class="ledger-time">${time}</span>
          <span class="ledger-detail">
            <strong style="margin-right:.35rem">${escHtml(label)}</strong>${escHtml(e.details)}
          </span>
        </li>`;
    }).join('');
  }

  $('refreshLedgerBtn').addEventListener('click', refreshLedger);

  // ── Settings ──────────────────────────────────────────────────────────────────
  async function populateDeviceSelect() {
    const devices = await window.electronAPI.getCloudDevices();
    const sel     = $('deviceSelect');
    sel.innerHTML = '<option value="">Select a device…</option>';
    devices.forEach(ip => {
      const opt = document.createElement('option');
      opt.value = ip; opt.textContent = ip;
      sel.appendChild(opt);
    });
    $('deleteDeviceBtn').disabled = (devices.length === 0);
  }

  $('deviceSelect').addEventListener('change', () => {
    $('deleteDeviceBtn').disabled = !$('deviceSelect').value;
  });

  $('deleteDeviceBtn').addEventListener('click', async () => {
    const ip = $('deviceSelect').value;
    if (!ip) return;
    if (!confirm(`Permanently delete all cloud data for ${ip}?`)) return;
    const res = await window.electronAPI.deleteDeviceData(ip);
    if (res.success) {
      showToast(`Deleted ${res.deleted} record(s) for ${ip}.`, 'success');
      await populateDeviceSelect();
      refreshSyncCount();
    } else {
      showToast('Error: ' + (res.error || 'unknown'), 'error');
    }
  });

  $('deleteAllBtn').addEventListener('click', async () => {
    if (!confirm('Permanently delete ALL cloud enrichment data? This cannot be undone.')) return;
    const res = await window.electronAPI.deleteCloudData();
    if (res.success) {
      showToast(`All cloud data deleted (${res.deleted} record(s)).`, 'success');
      await populateDeviceSelect();
      refreshSyncCount();
    } else {
      showToast('Error: ' + (res.error || 'unknown'), 'error');
    }
  });

  $('clearSnapshotBtn').addEventListener('click', async () => {
    if (!confirm('Clear the local scan snapshot? The next scan will not compare against previous results.')) return;
    await window.electronAPI.clearLocalSnapshot();
    showToast('Local scan snapshot cleared.', 'success');
  });

  // ── Utilities ─────────────────────────────────────────────────────────────────
  let toastTimer;
  function showToast(msg, type = 'info') {
    const t = $('toast');
    t.textContent = msg;
    t.className   = `toast ${type}`;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => t.classList.add('hidden'), 4000);
  }

  function escHtml(str) {
    if (typeof str !== 'string') return String(str ?? '');
    return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
  }

  // ── Init: verify cloud connection ─────────────────────────────────────────────
  (async () => {
    try {
      await window.electronAPI.getCloudLedger();
      $('cloudStatus').textContent = 'Connected';
      $('cloudStatus').classList.add('online');
    } catch {
      $('cloudStatus').textContent = 'Offline';
    }
    refreshSyncCount();
  })();

});
