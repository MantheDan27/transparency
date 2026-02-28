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

// Human-readable drift type labels
const DRIFT_LABELS = {
  new_device:      'New Device',
  ports_changed:   'Ports Changed',
  risk_detected:   'Risk Detected',
  risk_increased:  'Risk Increased',
  persistent_risk: 'Persistent Risk',
  identity_gap:    'Identity Gap',
};

// ── State ─────────────────────────────────────────────────────────────────────
let allAnomalies    = [];
let currentFilter   = 'all';
let deviceAnomalyMap = {};   // ip -> highest severity string
let cloudEnrichEnabled = true;
let pendingEnrichDevice = null;  // device waiting for preview confirm

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

  // ── Cloud enrichment toggle ──────────────────────────────────────────────────
  $('cloudEnrichToggle').addEventListener('change', () => {
    cloudEnrichEnabled = $('cloudEnrichToggle').checked;
    $('cloudToggleLabel').textContent = cloudEnrichEnabled ? 'Enabled' : 'Disabled';
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
      $('kpiLastScan').textContent   = now.toLocaleTimeString();
      $('lastScanLabel').textContent = `Last scan: ${now.toLocaleTimeString()} — ${result.devices.length} device(s) found`;

      // Enable export button
      $('exportReportBtn').disabled = false;
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
            return `<span class="port-badge${risky ? ' risky' : ''}" title="Port ${p}${PORT_NAMES[p] ? ' — ' + PORT_NAMES[p] : ''}">${p}</span>`;
          }).join('')
        : '<span style="color:var(--text-muted);font-size:0.8rem">None</span>';

      const enrichBtn = cloudEnrichEnabled
        ? `<button class="btn btn-secondary btn-sm enrich-btn" data-ip="${dev.ip}">Enrich</button>`
        : `<button class="btn btn-secondary btn-sm enrich-btn" data-ip="${dev.ip}" title="Enable Cloud Enrichment to use this feature" disabled>Enrich</button>`;

      return `
        <tr>
          <td style="font-family:monospace;font-size:0.84rem;color:var(--accent-light)">${dev.ip}</td>
          <td>${macCell}</td>
          <td style="color:var(--text-secondary);font-size:0.85rem">${dev.name}</td>
          <td>${portBadges}</td>
          <td>${riskBadge}</td>
          <td>${enrichBtn}</td>
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
        header.closest('.anomaly-card').classList.toggle('expanded');
      });
    });

    // Bind per-anomaly cloud delete buttons
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

    // Bind runbook link buttons
    container.querySelectorAll('.runbook-link').forEach(btn => {
      btn.addEventListener('click', () => {
        const url = btn.dataset.url;
        if (url) window.electronAPI.openExternalUrl(url);
      });
    });
  }

  function buildAnomalyCard(a) {
    const driftLabel = a.driftType ? (DRIFT_LABELS[a.driftType] || a.driftType) : null;
    const driftBadge = driftLabel
      ? `<span class="drift-badge drift-${a.driftType}">${escHtml(driftLabel)}</span>`
      : '';

    const steps = (a.steps || []).map(s => `<li>${escHtml(s)}</li>`).join('');

    const impactHtml = a.impact ? `
      <div class="explain-section">
        <h4>Impact</h4>
        <div class="impact-panel impact-${a.severity}">${escHtml(a.impact)}</div>
      </div>` : '';

    const runbookHtml = (a.runbookLinks && a.runbookLinks.length > 0) ? `
      <div class="explain-section">
        <h4>References &amp; Runbooks</h4>
        <div class="runbook-links">
          ${a.runbookLinks.map(r => `
            <button class="runbook-link" data-url="${escHtml(r.url)}">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="12" height="12">
                <path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6"/>
                <polyline points="15 3 21 3 21 9"/><line x1="10" y1="14" x2="21" y2="3"/>
              </svg>
              ${escHtml(r.label)}
            </button>`).join('')}
        </div>
      </div>` : '';

    // Drift / baseline timeline
    let driftTimelineHtml = '';
    if (a.driftDetails) {
      const rows = [];
      if (a.driftType === 'new_device') {
        rows.push(`<div class="drift-timeline-row"><span class="drift-dot new"></span><span>Device first appeared — not present in previous scan baseline</span></div>`);
        if (a.driftDetails.firstSeen) rows.push(`<div class="drift-timeline-row"><span class="drift-dot new"></span><span>First seen: ${new Date(a.driftDetails.firstSeen).toLocaleString()}</span></div>`);
      } else if (a.driftType === 'ports_changed' || a.driftType === 'risk_increased') {
        if (a.driftDetails.added?.length)   rows.push(`<div class="drift-timeline-row"><span class="drift-dot changed"></span><span>Ports opened since last scan: <strong>${a.driftDetails.added.map(p => PORT_NAMES[p] ? `${p} (${PORT_NAMES[p]})` : p).join(', ')}</strong></span></div>`);
        if (a.driftDetails.removed?.length) rows.push(`<div class="drift-timeline-row"><span class="drift-dot changed"></span><span>Ports closed since last scan: ${a.driftDetails.removed.map(p => PORT_NAMES[p] ? `${p} (${PORT_NAMES[p]})` : p).join(', ')}</span></div>`);
        if (a.driftDetails.previousPorts)   rows.push(`<div class="drift-timeline-row"><span class="drift-dot changed" style="background:var(--text-muted)"></span><span>Previous baseline: ${a.driftDetails.previousPorts.join(', ') || 'No ports'}</span></div>`);
      } else if (a.driftType === 'persistent_risk') {
        rows.push(`<div class="drift-timeline-row"><span class="drift-dot persistent"></span><span>Risk present across multiple consecutive scans — remediation still pending</span></div>`);
        if (a.driftDetails.previousPorts) rows.push(`<div class="drift-timeline-row"><span class="drift-dot risk"></span><span>Risky ports confirmed in previous baseline: ${a.driftDetails.previousPorts.filter(p => RISKY_PORTS.has(p)).map(p => PORT_NAMES[p] || p).join(', ')}</span></div>`);
      } else if (a.driftType === 'risk_increased') {
        rows.push(`<div class="drift-timeline-row"><span class="drift-dot risk"></span><span>Risk exposure increased — new dangerous services appeared since last scan</span></div>`);
      }
      if (rows.length > 0) {
        driftTimelineHtml = `
          <div class="explain-section">
            <h4>Baseline &amp; Drift</h4>
            <div class="drift-timeline">${rows.join('')}</div>
          </div>`;
      }
    }

    return `
      <div class="anomaly-card sev-${a.severity}" data-id="${escHtml(a.id)}">
        <div class="anomaly-header">
          <span class="sev-badge sev-${a.severity}">${a.severity.toUpperCase()}</span>
          ${driftBadge}
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
            ${impactHtml}
            ${steps ? `
            <div class="explain-section">
              <h4>Recommended actions</h4>
              <ol class="explain-steps">${steps}</ol>
            </div>` : ''}
            ${driftTimelineHtml}
            ${runbookHtml}
            <div class="anomaly-actions">
              <button class="btn btn-danger btn-sm delete-device-data-btn" data-ip="${escHtml(a.device)}">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="13" height="13">
                  <path d="M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z"/>
                </svg>
                Request cloud delete for ${escHtml(a.device)}
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

  // ── Enrichment flow with data preview ────────────────────────────────────────
  async function enrichDevice(device) {
    if (!cloudEnrichEnabled) return;

    // Show preview first
    const preview = await window.electronAPI.getCloudPreview(device);
    showPreviewModal(device, preview);
  }

  function showPreviewModal(device, preview) {
    const fieldsHtml = (preview.dataFields || []).map(f => {
      const valStr = Array.isArray(f.value)
        ? (f.value.length ? f.value.map(p => PORT_NAMES[p] ? `${p} (${PORT_NAMES[p]})` : p).join(', ') : 'None')
        : escHtml(String(f.value));
      return `
        <div class="preview-field">
          <span class="preview-field-key">${escHtml(f.field)}</span>
          <div class="preview-field-body">
            <div class="preview-field-val">${valStr}</div>
            <div class="preview-cat-row"><span class="preview-cat-label">${escHtml(f.category)} — ${escHtml(f.description)}</span></div>
          </div>
        </div>`;
    }).join('');

    $('previewBody').innerHTML = `
      <div class="preview-meta-row">
        <div class="preview-meta-item"><span class="preview-meta-key">Endpoint</span><span class="preview-meta-val" style="font-family:monospace;font-size:0.82rem">${escHtml(preview.serviceUrl || preview.endpoint)}</span></div>
        <div class="preview-meta-item"><span class="preview-meta-key">Purpose</span><span class="preview-meta-val">${escHtml(preview.purpose)}</span></div>
        <div class="preview-meta-item"><span class="preview-meta-key">Retention</span><span class="preview-meta-val">${escHtml(preview.retentionPolicy)}</span></div>
        <div class="preview-meta-item"><span class="preview-meta-key">Third Parties</span><span class="preview-meta-val">${escHtml(preview.thirdParties)}</span></div>
      </div>
      <p style="font-size:0.78rem;color:var(--text-muted);margin-bottom:0.6rem;text-transform:uppercase;letter-spacing:0.05em;font-weight:600">Data to be transmitted for ${escHtml(device.ip)}</p>
      <div class="preview-what">${fieldsHtml}</div>
      <div class="preview-notice">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="14" height="14"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/></svg>
        This enrichment runs on a local service (localhost). No data is sent to the internet. Every transmission is logged in the Data Ledger.
      </div>`;

    pendingEnrichDevice = device;
    $('previewModal').classList.remove('hidden');
  }

  $('previewConfirmBtn').addEventListener('click', async () => {
    $('previewModal').classList.add('hidden');
    if (!pendingEnrichDevice) return;
    const device = pendingEnrichDevice;
    pendingEnrichDevice = null;
    await doEnrich(device);
  });

  $('previewCancelBtn').addEventListener('click', () => {
    $('previewModal').classList.add('hidden');
    pendingEnrichDevice = null;
    showToast('Cloud enrichment cancelled.', 'info');
  });

  $('previewClose').addEventListener('click', () => {
    $('previewModal').classList.add('hidden');
    pendingEnrichDevice = null;
  });

  $('previewModal').addEventListener('click', e => {
    if (e.target === $('previewModal')) {
      $('previewModal').classList.add('hidden');
      pendingEnrichDevice = null;
    }
  });

  async function doEnrich(device) {
    $('modalTitle').textContent = `Cloud Guidance — ${device.ip}`;
    $('modalBody').innerHTML = `<p style="color:var(--text-muted);text-align:center;padding:2rem">Fetching guidance…</p>`;
    $('enrichModal').classList.remove('hidden');

    const result = await window.electronAPI.enrichDevice(device);
    refreshSyncCount();

    const riskClass = { Critical: 'crit', High: 'high', Informational: 'safe' }[result.riskLevel] || 'info';
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
      <p class="modal-note">This transmission has been logged in the Data Ledger. Analysis is provided by the local Transparency service — no external network connections were made.</p>
    `;
  }

  $('modalClose').addEventListener('click', () => $('enrichModal').classList.add('hidden'));
  $('enrichModal').addEventListener('click', e => {
    if (e.target === $('enrichModal')) $('enrichModal').classList.add('hidden');
  });
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape') {
      $('enrichModal').classList.add('hidden');
      $('previewModal').classList.add('hidden');
      pendingEnrichDevice = null;
    }
  });

  // ── Data Ledger ──────────────────────────────────────────────────────────────
  async function refreshLedger() {
    const entries = await window.electronAPI.getCloudLedger();
    updateLedgerStats(entries);
    await populateLedgerDeviceSelect();
    renderLedgerList(entries);
  }

  function updateLedgerStats(entries) {
    const sends   = entries.filter(e => e.action === 'SEND').length;
    const deletes = entries.filter(e => e.action.startsWith('DELETE')).length;
    const devices = new Set(entries.filter(e => e.deviceIp).map(e => e.deviceIp)).size;
    $('lstatSend').textContent    = sends;
    $('lstatDelete').textContent  = deletes;
    $('lstatDevices').textContent = devices;
  }

  function renderLedgerList(entries) {
    const list = $('ledgerList');

    if (entries.length === 0) {
      list.innerHTML = '<li class="ledger-empty">No activity recorded yet.</li>';
      return;
    }

    list.innerHTML = entries.map(e => {
      const time  = new Date(e.timestamp).toLocaleString();
      const label = { SEND: 'SEND', DELETE_ALL: 'DELETE ALL', DELETE_DEVICE: 'DELETE DEVICE' }[e.action] || e.action;
      const dotClass = e.action.toLowerCase().replace(/_/g, '');

      // Data categories pills
      const catPills = (e.dataCategories || []).map(c =>
        `<span class="category-pill">${escHtml(c)}</span>`
      ).join('');

      // Data preview summary
      let previewSummary = '';
      if (e.dataPreview) {
        const parts = [];
        if (e.dataPreview.ip)       parts.push(`ip: ${e.dataPreview.ip}`);
        if (e.dataPreview.hostname) parts.push(`host: ${e.dataPreview.hostname}`);
        if (e.dataPreview.ports?.length) parts.push(`ports: [${e.dataPreview.ports.join(', ')}]`);
        previewSummary = parts.join(' · ');
      }

      return `
        <li class="ledger-entry" data-id="${escHtml(e.id)}">
          <div class="ledger-entry-top">
            <span class="ledger-dot ${dotClass}"></span>
            <span class="ledger-time">${time}</span>
            <span class="ledger-detail">
              <strong style="margin-right:.35rem">${escHtml(label)}</strong>${escHtml(e.details)}
              ${catPills ? `<span style="margin-left:.35rem">${catPills}</span>` : ''}
            </span>
            <span class="ledger-expand-btn">
              Details
              <svg class="ledger-expand-chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="12" height="12">
                <polyline points="6 9 12 15 18 9"/>
              </svg>
            </span>
          </div>
          <div class="ledger-detail-panel">
            <div class="ledger-detail-inner">
              <div class="ldr-field">
                <span class="ldr-label">Endpoint</span>
                <span class="ldr-value"><code>${escHtml(e.endpoint || '—')}</code></span>
              </div>
              <div class="ldr-field">
                <span class="ldr-label">Device IP</span>
                <span class="ldr-value"><code>${escHtml(e.deviceIp || 'N/A')}</code></span>
              </div>
              <div class="ldr-field">
                <span class="ldr-label">Data Categories</span>
                <span class="ldr-value">${catPills || '<span style="color:var(--text-muted)">—</span>'}</span>
              </div>
              <div class="ldr-field">
                <span class="ldr-label">Timestamp</span>
                <span class="ldr-value">${escHtml(e.timestamp)}</span>
              </div>
              ${previewSummary ? `
              <div class="ldr-field full">
                <span class="ldr-label">Data Transmitted</span>
                <span class="ldr-value"><code>${escHtml(previewSummary)}</code></span>
              </div>` : ''}
              <div class="ldr-field full">
                <span class="ldr-label">Reason</span>
                <span class="ldr-value">${escHtml(e.reason || 'No reason recorded')}</span>
              </div>
            </div>
          </div>
        </li>`;
    }).join('');

    // Bind expand toggles
    list.querySelectorAll('.ledger-entry').forEach(li => {
      li.querySelector('.ledger-entry-top').addEventListener('click', () => {
        li.classList.toggle('open');
      });
    });
  }

  $('refreshLedgerBtn').addEventListener('click', refreshLedger);

  // ── Ledger export ────────────────────────────────────────────────────────────
  $('exportLedgerBtn').addEventListener('click', async () => {
    const entries = await window.electronAPI.getCloudLedger();
    const payload = {
      exportedAt: new Date().toISOString(),
      format: 'transparency-ledger-export',
      totalEntries: entries.length,
      entries
    };
    downloadJSON(payload, `transparency-ledger-${dateStamp()}.json`);
    showToast('Ledger exported as JSON.', 'success');
  });

  // ── Ledger per-device controls ────────────────────────────────────────────────
  async function populateLedgerDeviceSelect() {
    const localDevices = await window.electronAPI.getLocalDevices();
    const sel = $('ledgerDeviceSelect');
    const prev = sel.value;
    sel.innerHTML = '<option value="">Select a device…</option>';
    localDevices.forEach(ip => {
      const opt = document.createElement('option');
      opt.value = ip; opt.textContent = ip;
      sel.appendChild(opt);
    });
    if (prev && localDevices.includes(prev)) sel.value = prev;
    const hasVal = !!sel.value;
    $('deleteLocalCacheBtn').disabled = !hasVal;
    $('requestCloudDeleteBtn').disabled = !hasVal;
  }

  $('ledgerDeviceSelect').addEventListener('change', () => {
    const hasVal = !!$('ledgerDeviceSelect').value;
    $('deleteLocalCacheBtn').disabled = !hasVal;
    $('requestCloudDeleteBtn').disabled = !hasVal;
  });

  $('deleteLocalCacheBtn').addEventListener('click', async () => {
    const ip = $('ledgerDeviceSelect').value;
    if (!ip) return;
    if (!confirm(`Remove ${ip} from the local scan snapshot? This clears the baseline for drift detection on this device.`)) return;
    const res = await window.electronAPI.deleteLocalDevice(ip);
    if (res.success) {
      showToast(`Local cache cleared for ${ip}.`, 'success');
      await populateLedgerDeviceSelect();
    } else {
      showToast('Error: ' + (res.error || 'unknown'), 'error');
    }
  });

  $('requestCloudDeleteBtn').addEventListener('click', async () => {
    const ip = $('ledgerDeviceSelect').value;
    if (!ip) return;
    if (!confirm(`Permanently delete all cloud enrichment records for ${ip}? This will be logged.`)) return;
    const res = await window.electronAPI.deleteDeviceData(ip);
    if (res.success) {
      showToast(`Cloud records deleted for ${ip} (${res.deleted} record(s)).`, 'success');
      refreshSyncCount();
      await refreshLedger();
    } else {
      showToast('Error: ' + (res.error || 'unknown'), 'error');
    }
  });

  // ── Report export ────────────────────────────────────────────────────────────
  $('exportReportBtn').addEventListener('click', async () => {
    const report = await window.electronAPI.exportReport();
    if (report.error) {
      showToast('Export failed: ' + report.error, 'error');
      return;
    }
    downloadJSON(report, `transparency-report-${dateStamp()}.json`);
    showToast('Report exported as JSON.', 'success');
  });

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

  function dateStamp() {
    return new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
  }

  function downloadJSON(obj, filename) {
    const blob = new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' });
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    a.href = url; a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  // ── Init: verify cloud connection ─────────────────────────────────────────────
  (async () => {
    try {
      await window.electronAPI.getCloudLedger();
      $('cloudStatus').textContent = 'Connected';
      $('cloudStatus').classList.add('online');
    } catch {
      $('cloudStatus').textContent = 'Offline';
      $('cloudStatus').classList.remove('online');
    }
    refreshSyncCount();
  })();

});
