'use strict';

/* ═══════════════════════════════════════════════════════════════════
   Transparency — Renderer Process v2.0
   ═══════════════════════════════════════════════════════════════════ */

// ── Constants ─────────────────────────────────────────────────────────────────
const RISKY_PORTS = new Set([23, 135, 139, 445, 3389, 5900]);
const PORT_NAMES = {
  21:'FTP', 22:'SSH', 23:'Telnet', 25:'SMTP', 53:'DNS', 80:'HTTP',
  110:'POP3', 111:'RPC', 135:'MS-RPC', 139:'NetBIOS', 443:'HTTPS',
  445:'SMB', 548:'AFP', 554:'RTSP', 631:'IPP', 873:'rsync', 1883:'MQTT',
  2049:'NFS', 3306:'MySQL', 3389:'RDP', 5000:'UPnP', 5900:'VNC',
  7000:'AirPlay', 7100:'AirPlay', 8008:'Chromecast', 8009:'Chromecast',
  8080:'HTTP-Alt', 8443:'HTTPS-Alt', 8883:'MQTT-SSL', 9100:'Printing',
};
const TRUST_LABELS = {
  owned:'Owned', known:'Known', guest:'Guest',
  unknown:'Unknown', blocked:'Blocked', watchlist:'Watchlist',
};
const TRUST_COLORS = {
  owned:'trust-owned', known:'trust-known', guest:'trust-guest',
  unknown:'trust-unknown', blocked:'trust-blocked', watchlist:'trust-watchlist',
};
const DEVICE_ICONS = {
  'Router/Gateway':   '🌐', 'NAS':            '🗄️',
  'Smart Speaker':    '🔊', 'Smart TV / Stick':'📺',
  'Printer':          '🖨️', 'Camera / DVR':   '📷',
  'Windows PC':       '💻', 'macOS Device':   '🍎',
  'Linux Server':     '🐧', 'IoT Device':     '💡',
  'Laptop':           '💻', 'Virtual Machine': '📦',
  'Unknown Device':   '❓',
};

// ── Global state ──────────────────────────────────────────────────────────────
let allDevices       = [];  // last scan result with meta
let allAnomalies     = [];
let allAlerts        = [];
let allAlertRules    = [];
let currentFilter    = 'all';
let alertFilter      = 'all';
let selectedDevices  = new Set();
let currentDetailDev = null;
let cloudEnrichEnabled = true;
let pendingEnrichDevice = null;
let currentScanMode  = 'standard';
let editingRuleId    = null;

// ── DOM helper ────────────────────────────────────────────────────────────────
const $ = id => document.getElementById(id);

function escHtml(str) {
  if (typeof str !== 'string') return String(str ?? '');
  return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
function dateStamp() {
  return new Date().toISOString().replace(/[:.]/g,'-').slice(0,19);
}
function relativeTime(isoStr) {
  if (!isoStr) return '—';
  const diff = Date.now() - new Date(isoStr).getTime();
  if (diff < 60000)      return 'just now';
  if (diff < 3600000)    return `${Math.round(diff/60000)}m ago`;
  if (diff < 86400000)   return `${Math.round(diff/3600000)}h ago`;
  return `${Math.round(diff/86400000)}d ago`;
}
function downloadJSON(obj, filename) {
  const blob = new Blob([JSON.stringify(obj, null, 2)], { type:'application/json' });
  const url  = URL.createObjectURL(blob);
  const a    = document.createElement('a');
  a.href = url; a.download = filename;
  document.body.appendChild(a); a.click();
  document.body.removeChild(a); URL.revokeObjectURL(url);
}

let toastTimer;
function showToast(msg, type = 'info') {
  const t = $('toast');
  t.textContent = msg;
  t.className = `toast ${type}`;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.add('hidden'), 4500);
}

function getDeviceKey(dev) {
  return (dev.mac && dev.mac !== 'Unknown') ? `mac:${dev.mac}` : `ip:${dev.ip}`;
}

// ── Tab navigation ────────────────────────────────────────────────────────────
function switchTab(tabName, filter) {
  document.querySelectorAll('.nav-item').forEach(b => b.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(s => s.classList.remove('active'));
  const btn = document.querySelector(`.nav-item[data-tab="${tabName}"]`);
  if (btn) btn.classList.add('active');
  const tab = $(`tab-${tabName}`);
  if (tab) tab.classList.add('active');
  if (tabName === 'ledger')  refreshLedger();
  if (tabName === 'alerts')  loadAlerts();
  if (tabName === 'privacy') loadDataStats();
  if (tabName === 'devices' && filter) applyDeviceFilter(filter);
}
window.switchTab = switchTab;

document.addEventListener('DOMContentLoaded', () => {

  // Tab nav
  document.querySelectorAll('.nav-item').forEach(btn => {
    btn.addEventListener('click', () => switchTab(btn.dataset.tab));
  });

  // ── Scan mode pills (legacy — keep for compatibility) ─────────────────────
  document.querySelectorAll('.mode-pill[data-mode]').forEach(pill => {
    pill.addEventListener('click', () => {
      document.querySelectorAll('.mode-pill[data-mode]').forEach(p => p.classList.remove('active'));
      pill.classList.add('active');
      currentScanMode = pill.dataset.mode;
    });
  });

  // ── New consolidated scan dropdown ────────────────────────────────────────
  const scanDropdownToggle = $('scanDropdownToggle');
  const scanDropdownMenu   = $('scanDropdownMenu');
  const scanPrimaryBtn     = $('quickScanBtnMain');

  if (scanDropdownToggle && scanDropdownMenu) {
    scanDropdownToggle.addEventListener('click', e => {
      e.stopPropagation();
      scanDropdownMenu.classList.toggle('hidden');
    });
    scanDropdownMenu.querySelectorAll('.scan-mode-option').forEach(opt => {
      opt.addEventListener('click', () => {
        const mode = opt.dataset.scanMode;
        currentScanMode = mode;
        scanDropdownMenu.querySelectorAll('.scan-mode-option').forEach(o => o.classList.remove('active-mode'));
        opt.classList.add('active-mode');
        const modeLabels = { quick:'Scan (Quick)', standard:'Scan (Standard)', deep:'Scan (Deep)' };
        const lbl = $('scanBtnModeLabel');
        if (lbl) lbl.textContent = modeLabels[mode] || 'Scan';
        scanDropdownMenu.classList.add('hidden');
      });
    });
    document.addEventListener('click', () => scanDropdownMenu.classList.add('hidden'));
  }
  if (scanPrimaryBtn) {
    scanPrimaryBtn.addEventListener('click', () => runScan(currentScanMode));
  }

  // ── Monitor toggle switch ─────────────────────────────────────────────────
  const monitorSwitch = $('monitorToggleSwitch');
  if (monitorSwitch) {
    monitorSwitch.addEventListener('change', () => window.toggleMonitoring());
  }

  // Map mode pills
  document.querySelectorAll('.mode-pill[data-map-mode]').forEach(pill => {
    pill.addEventListener('click', () => {
      document.querySelectorAll('.mode-pill[data-map-mode]').forEach(p => p.classList.remove('active'));
      pill.classList.add('active');
      renderMap();
    });
  });

  // Quick scan button in status strip
  $('quickScanBtn').addEventListener('click', () => runScan(currentScanMode));

  // Cloud enrichment toggle
  $('cloudEnrichToggle').addEventListener('change', () => {
    cloudEnrichEnabled = $('cloudEnrichToggle').checked;
    $('cloudToggleLabel').textContent = cloudEnrichEnabled ? 'Enabled' : 'Disabled';
  });

  // Device search
  $('deviceSearch').addEventListener('input', () => applyDeviceFilter(currentFilter));

  // Select all checkbox
  $('selectAll').addEventListener('change', e => {
    const rows = document.querySelectorAll('#deviceBody input[type="checkbox"]');
    rows.forEach(cb => {
      cb.checked = e.target.checked;
      const ip = cb.dataset.ip;
      if (ip) e.target.checked ? selectedDevices.add(ip) : selectedDevices.delete(ip);
    });
    updateBulkBar();
  });

  // Device filter buttons
  document.querySelectorAll('.filter-btn[data-filter]').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.filter-btn[data-filter]').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      currentFilter = btn.dataset.filter;
      applyDeviceFilter(currentFilter);
    });
  });

  // Alert filter buttons
  document.querySelectorAll('.filter-btn[data-alert-filter]').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.filter-btn[data-alert-filter]').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      alertFilter = btn.dataset.alertFilter;
      renderAlerts();
    });
  });

  // Detail panel close
  $('detailPanelClose').addEventListener('click', closeDetailPanel);
  $('detailPanelBackdrop').addEventListener('click', closeDetailPanel);

  // Modal closes
  $('modalClose').addEventListener('click',    () => $('enrichModal').classList.add('hidden'));
  $('previewClose').addEventListener('click',  () => { $('previewModal').classList.add('hidden'); pendingEnrichDevice = null; });
  $('previewCancelBtn').addEventListener('click', () => { $('previewModal').classList.add('hidden'); pendingEnrichDevice = null; showToast('Enrichment cancelled.', 'info'); });
  $('previewConfirmBtn').addEventListener('click', async () => {
    $('previewModal').classList.add('hidden');
    if (pendingEnrichDevice) { await doEnrich(pendingEnrichDevice); pendingEnrichDevice = null; }
  });
  $('enrichModal').addEventListener('click', e => { if (e.target === $('enrichModal')) $('enrichModal').classList.add('hidden'); });
  $('previewModal').addEventListener('click', e => { if (e.target === $('previewModal')) { $('previewModal').classList.add('hidden'); pendingEnrichDevice = null; } });

  // Rule builder
  $('ruleBuilderClose').addEventListener('click',  () => $('ruleBuilderModal').classList.add('hidden'));
  $('ruleBuilderCancel').addEventListener('click', () => $('ruleBuilderModal').classList.add('hidden'));
  $('ruleBuilderSave').addEventListener('click',   saveAlertRule);

  // Ledger controls
  $('exportLedgerBtn').addEventListener('click', exportLedger);
  $('refreshLedgerBtn').addEventListener('click', refreshLedger);
  $('ledgerDeviceSelect').addEventListener('change', () => {
    const has = !!$('ledgerDeviceSelect').value;
    $('deleteLocalCacheBtn').disabled = !has;
    $('requestCloudDeleteBtn').disabled = !has;
  });
  $('deleteLocalCacheBtn').addEventListener('click', deleteLocalCache);
  $('requestCloudDeleteBtn').addEventListener('click', requestCloudDelete);

  // Privacy controls
  $('deviceSelect').addEventListener('change', () => {
    $('deleteDeviceBtn').disabled = !$('deviceSelect').value;
    $('delDeviceOptions').classList.toggle('hidden', !$('deviceSelect').value);
  });
  $('deleteDeviceBtn').addEventListener('click', forgetDevice);
  $('deleteAllBtn').addEventListener('click', deleteAllCloud);
  $('clearSnapshotBtn').addEventListener('click', clearSnapshot);

  // Bulk tag modal
  $('bulkTagConfirm').addEventListener('click', applyBulkTags);

  // Keyboard shortcuts
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape') {
      $('enrichModal').classList.add('hidden');
      $('previewModal').classList.add('hidden');
      $('ruleBuilderModal').classList.add('hidden');
      $('bulkTagModal').classList.add('hidden');
      closeDetailPanel();
      pendingEnrichDevice = null;
    }
  });

  // Listen for new alerts from main process
  window.electronAPI.onNewAlert(alert => {
    allAlerts.unshift(alert);
    updateAlertBadge();
    renderAlerts();
    if (!document.querySelector('#tab-alerts.active')) {
      showToast(`Alert: ${alert.title}`, 'info');
    }
  });

  // Initial loads
  loadNetworkInfo();
  loadAlerts();
  loadAlertRules();
  (async () => {
    try {
      await window.electronAPI.getCloudLedger();
      $('cloudStatus').textContent = 'Ready';
      $('cloudStatus').classList.add('online');
    } catch {
      $('cloudStatus').textContent = 'Offline';
      $('cloudStatus').classList.remove('online');
    }
  })();
});

// ── Network scan ──────────────────────────────────────────────────────────────
async function runScan(mode) {
  mode = mode || currentScanMode;
  const gentle = $('gentleToggle') ? $('gentleToggle').checked : false;
  const bar    = $('scanProgressBar');
  const progTxt = $('progressText');

  // Disable all scan buttons
  ['quickScanBtn','quickScanBtnMain','btnQuickScan','btnDeepScan'].forEach(id => {
    if ($(id)) $(id).disabled = true;
  });
  if ($('scanDropdownBtn')) $('scanDropdownBtn').disabled = true;
  bar.classList.remove('hidden');
  progTxt.textContent = 'Initializing…';

  // Show scan phases
  const phases = $('scanPhases');
  if (phases) {
    phases.classList.remove('hidden');
    ['ph-discover','ph-resolve','ph-probe','ph-done'].forEach(id => {
      const el = $(id); if (el) { el.classList.remove('ph-active','ph-done'); }
    });
    const phDiscover = $('ph-discover');
    if (phDiscover) phDiscover.classList.add('ph-active');
  }

  const modeLabel = { quick:'Quick', standard:'Standard', deep:'Deep' }[mode] || 'Standard';
  showToast(`Starting ${modeLabel} scan…`, 'info');

  let progressPct = 0;
  const removeListener = window.electronAPI.onScanProgress(msg => {
    progTxt.textContent = msg;
    const m = msg.match(/(\d+)\/(\d+)/);
    if (m) progressPct = Math.min(90, Math.round(parseInt(m[1]) / parseInt(m[2]) * 90));
    else progressPct = Math.min(progressPct + 5, 85);
    $('progressFill').style.width = progressPct + '%';
    // Update phase indicators
    const lo = msg.toLowerCase();
    const setPhase = (active) => {
      ['ph-discover','ph-resolve','ph-probe','ph-done'].forEach((id, i) => {
        const el = $(id); if (!el) return;
        const idx = ['ph-discover','ph-resolve','ph-probe','ph-done'].indexOf(active);
        if (i < idx) { el.classList.add('ph-done'); el.classList.remove('ph-active'); }
        else if (i === idx) { el.classList.add('ph-active'); el.classList.remove('ph-done'); }
        else { el.classList.remove('ph-active','ph-done'); }
      });
    };
    if (lo.includes('resolv') || lo.includes('dns')) setPhase('ph-resolve');
    else if (lo.includes('prob') || lo.includes('service') || lo.includes('port')) setPhase('ph-probe');
    else setPhase('ph-discover');
  });

  try {
    const result = await window.electronAPI.scanNetwork({ mode, gentle });
    removeListener();
    $('progressFill').style.width = '100%';

    if (result.error) { showToast('Scan failed: ' + result.error, 'error'); return; }

    allDevices   = result.devices;
    allAnomalies = result.anomalies;

    renderDeviceTable();
    updateFilterCounts();
    updateKPIs(result.devices, result.anomalies);
    renderMap();
    renderRecentChanges(result.anomalies);
    updateOverviewStatus(result.devices, result.anomalies);
    populateDeviceSelects();

    const now = new Date();
    $('nhcLastScan').textContent = `Last scan: ${now.toLocaleTimeString()} · ${mode}`;
    showToast(`Scan complete — ${result.devices.length} device(s) found`, 'success');
  } catch (err) {
    removeListener();
    showToast('Scan error: ' + err.message, 'error');
  } finally {
    // Mark complete phase
    ['ph-discover','ph-resolve','ph-probe'].forEach(id => { const el=$(id); if(el){el.classList.remove('ph-active'); el.classList.add('ph-done');} });
    const phDone = $('ph-done'); if(phDone){phDone.classList.add('ph-active');}
    setTimeout(() => {
      bar.classList.add('hidden');
      $('progressFill').style.width = '0%';
      const phases = $('scanPhases'); if(phases) phases.classList.add('hidden');
    }, 1200);
    ['quickScanBtn','quickScanBtnMain','btnQuickScan','btnDeepScan'].forEach(id => {
      if ($(id)) $(id).disabled = false;
    });
  }
}
window.runScan = runScan;

// ── Overview ──────────────────────────────────────────────────────────────────
async function loadNetworkInfo() {
  try {
    const [wifi, gw] = await Promise.all([
      window.electronAPI.getWifiInfo().catch(() => null),
      window.electronAPI.getGatewayInfo().catch(() => null),
    ]);

    let ssidText = wifi?.ssid ? wifi.ssid : 'Network';
    let metaParts = [];
    if (wifi?.bssid)    metaParts.push(`BSSID: ${wifi.bssid}`);
    if (wifi?.channel)  metaParts.push(`Ch ${wifi.channel} · ${wifi.band || ''}`);
    if (wifi?.security) metaParts.push(wifi.security);
    if (gw?.gateway)    metaParts.push(`Gateway: ${gw.gateway}`);

    $('nhcSSID').textContent = ssidText;
    $('nhcMeta').textContent = metaParts.join(' · ') || 'Ethernet / Unknown';

    if (gw?.latencyMs != null) {
      $('kpiLatency').textContent = `${Math.round(gw.latencyMs)} ms`;
    }
    if (gw?.dnsLatencyMs != null) {
      $('kpiDNS').textContent = `DNS: ${Math.round(gw.dnsLatencyMs)} ms`;
    }
  } catch { /* ignore */ }
}

function updateKPIs(devices, anomalies) {
  $('kpiDevices').textContent = devices.length;

  const unknownCount = devices.filter(d => {
    const trust = d.meta?.trustState || 'unknown';
    return trust === 'unknown';
  }).length;
  $('kpiUnknown').textContent = unknownCount;

  const unackedAlerts = allAlerts.filter(a => !a.acknowledged).length;
  $('kpiAlerts').textContent = unackedAlerts;

  // Update alert badge
  updateAlertBadge();

  // Device badge in nav
  const nb = $('navDeviceCount');
  nb.textContent = devices.length;
  nb.classList.toggle('hidden', devices.length === 0);
}

function updateOverviewStatus(devices, anomalies) {
  const strip = $('statusStrip');
  const text  = $('statusStripText');
  const high  = anomalies.filter(a => a.severity === 'High').length;
  const newDevs = anomalies.filter(a => a.type === 'New Device').length;
  const unknown = devices.filter(d => (d.meta?.trustState || 'unknown') === 'unknown').length;

  if (high > 0) {
    strip.className = 'status-strip status-danger';
    text.textContent = `${high} high-severity risk(s) detected — review immediately.`;
  } else if (newDevs > 0) {
    strip.className = 'status-strip status-warning';
    text.textContent = `${newDevs} new device(s) joined since last scan.`;
  } else if (unknown > 0) {
    strip.className = 'status-strip status-warning';
    text.textContent = `${unknown} unknown device(s) on your network. Tag or verify them.`;
  } else if (devices.length > 0) {
    strip.className = 'status-strip status-good';
    text.textContent = `All good — ${devices.length} device(s) online, no high-severity issues.`;
  } else {
    strip.className = 'status-strip';
    text.textContent = 'Run a scan to assess your network.';
  }
}

function renderRecentChanges(anomalies) {
  const body = $('recentChangesBody');
  const tag  = $('recentChangesTag');

  if (!anomalies || anomalies.length === 0) {
    body.innerHTML = '<div class="empty-state-sm">No changes detected since last scan.</div>';
    tag.textContent = 'No changes';
    return;
  }

  tag.textContent = `${anomalies.length} change(s)`;
  body.innerHTML = anomalies.slice(0, 8).map(a => `
    <div class="change-item sev-${a.severity.toLowerCase()}">
      <span class="change-dot sev-dot-${a.severity.toLowerCase()}"></span>
      <div class="change-body">
        <div class="change-title">${escHtml(a.type)} — ${escHtml(a.device)}</div>
        <div class="change-desc">${escHtml(a.description)}</div>
      </div>
      <span class="sev-pill sev-${a.severity.toLowerCase()}">${a.severity}</span>
    </div>`).join('');
}

// ── Devices table ─────────────────────────────────────────────────────────────
function applyDeviceFilter(filter) {
  currentFilter = filter || 'all';
  renderDeviceTable();
}
window.applyDeviceFilter = applyDeviceFilter;

function getFilteredDevices() {
  const search = ($('deviceSearch').value || '').toLowerCase();
  const anomalyIpSet = new Set(allAnomalies.filter(a => a.severity === 'High').map(a => a.device));
  const changedIpSet = new Set(allAnomalies.filter(a => a.type === 'Ports Changed' || a.type === 'New Device').map(a => a.device));

  return allDevices.filter(dev => {
    // Trust/filter match
    const trust = dev.meta?.trustState || 'unknown';
    if (currentFilter !== 'all') {
      if (currentFilter === 'online')   { /* all visible = online for now */ }
      if (currentFilter === 'unknown'   && trust !== 'unknown')   return false;
      if (currentFilter === 'owned'     && trust !== 'owned')     return false;
      if (currentFilter === 'watchlist' && !dev.meta?.watchlist)  return false;
      if (currentFilter === 'risky'     && !anomalyIpSet.has(dev.ip)) return false;
      if (currentFilter === 'changed'   && !changedIpSet.has(dev.ip)) return false;
    }
    // Search
    if (search) {
      const hay = [dev.ip, dev.name, dev.hostname, dev.vendor, dev.deviceType, ...(dev.meta?.tags||[])].join(' ').toLowerCase();
      if (!hay.includes(search)) return false;
    }
    return true;
  });
}

function updateFilterCounts() {
  const total   = allDevices.length;
  const anomalyIpSet = new Set(allAnomalies.filter(a => a.severity === 'High').map(a => a.device));
  const changedSet   = new Set(allAnomalies.filter(a => a.type === 'Ports Changed' || a.type === 'New Device').map(a => a.device));

  $('fAll').textContent      = total;
  $('fOnline').textContent   = total;
  $('fUnknown').textContent  = allDevices.filter(d => (d.meta?.trustState || 'unknown') === 'unknown').length;
  $('fWatchlist').textContent = allDevices.filter(d => d.meta?.watchlist).length;
  $('fOwned').textContent    = allDevices.filter(d => d.meta?.trustState === 'owned').length;
  $('fRisky').textContent    = allDevices.filter(d => anomalyIpSet.has(d.ip)).length;
  $('fChanged').textContent  = allDevices.filter(d => changedSet.has(d.ip)).length;
}

function renderDeviceTable() {
  const tbody    = $('deviceBody');
  const devs     = getFilteredDevices();
  const subtitle = $('devicesSubtitle');

  subtitle.textContent = allDevices.length > 0
    ? `${allDevices.length} device(s) found · showing ${devs.length} · last scan: ${new Date().toLocaleTimeString()}`
    : 'No devices found yet — run a scan from the Overview tab.';

  if (devs.length === 0) {
    tbody.innerHTML = `<tr class="empty-row"><td colspan="10">${allDevices.length === 0 ? 'Run a scan to discover devices.' : 'No devices match this filter.'}</td></tr>`;
    return;
  }

  // Build severity map
  const sevMap = {};
  const sevOrder = { High:3, Medium:2, Low:1 };
  for (const a of allAnomalies) {
    if (!sevMap[a.device] || sevOrder[a.severity] > sevOrder[sevMap[a.device]]) sevMap[a.device] = a.severity;
  }

  tbody.innerHTML = devs.map(dev => {
    const trust   = dev.meta?.trustState || 'unknown';
    const tags    = dev.meta?.tags || [];
    const name    = dev.meta?.customName || dev.hostname || dev.name;
    const icon    = DEVICE_ICONS[dev.deviceType] || '❓';
    const sev     = sevMap[dev.ip];
    const checked = selectedDevices.has(dev.ip) ? 'checked' : '';
    const hist    = dev.history;
    const lastSeen = hist?.lastSeen ? relativeTime(hist.lastSeen) : 'Just now';
    const isNew   = allAnomalies.some(a => a.type === 'New Device' && a.device === dev.ip);
    const changed = allAnomalies.some(a => (a.type === 'Ports Changed') && a.device === dev.ip);

    const portBadges = dev.ports?.length
      ? dev.ports.slice(0, 6).map(p => {
          const risky = RISKY_PORTS.has(p);
          return `<span class="port-badge${risky ? ' risky' : ''}" title="${PORT_NAMES[p] || 'Port ' + p}">${p}</span>`;
        }).join('') + (dev.ports.length > 6 ? `<span class="port-badge more">+${dev.ports.length - 6}</span>` : '')
      : '<span class="no-ports">—</span>';

    const riskBadge = sev
      ? `<span class="risk-badge risk-${sev.toLowerCase()}">${sev}</span>`
      : `<span class="risk-badge risk-safe">Safe</span>`;

    const trustBadge = `<span class="trust-badge ${TRUST_COLORS[trust] || 'trust-unknown'}">${TRUST_LABELS[trust] || trust}</span>`;

    const tagChips = tags.map(t => `<span class="tag-chip">${escHtml(t)}</span>`).join('');

    const changeDot = (isNew || changed)
      ? `<span class="change-indicator" title="${isNew ? 'New device' : 'Changed'}"></span>`
      : '';

    return `
      <tr class="device-row${selectedDevices.has(dev.ip) ? ' selected' : ''}" data-ip="${escHtml(dev.ip)}">
        <td class="col-check"><input type="checkbox" data-ip="${escHtml(dev.ip)}" ${checked}></td>
        <td class="col-status"><span class="online-dot" title="Online"></span>${changeDot}</td>
        <td class="col-name">
          <div class="device-name-cell">
            <span class="device-type-icon">${icon}</span>
            <div>
              <div class="device-display-name">${escHtml(name)}</div>
              <div class="device-sub">${escHtml(dev.deviceType || 'Unknown')}</div>
              ${tagChips}
            </div>
          </div>
        </td>
        <td class="col-ip"><span class="mono-ip">${escHtml(dev.ip)}</span>${dev.isIPv6 ? '<span class="ipv6-badge">IPv6</span>' : ''}
          ${dev.mac && dev.mac !== 'Unknown' ? `<div class="mac-addr">${escHtml(dev.mac)}</div>` : ''}
        </td>
        <td class="col-vendor">
          <div class="vendor-name">${escHtml(dev.vendor || '—')}</div>
          <div class="confidence-row">
            <div class="conf-bar"><div class="conf-fill" style="width:${dev.confidence || 0}%"></div></div>
            <span class="conf-pct">${dev.confidence || 0}%</span>
          </div>
        </td>
        <td class="col-ports">${portBadges}</td>
        <td class="col-trust">${trustBadge}</td>
        <td class="col-risk">${riskBadge}</td>
        <td class="col-seen" style="color:var(--text-muted);font-size:0.8rem">${escHtml(lastSeen)}</td>
        <td class="col-actions">
          <button class="btn btn-secondary btn-xs detail-btn" data-ip="${escHtml(dev.ip)}" title="View details">Details</button>
        </td>
      </tr>`;
  }).join('');

  // Bind events
  tbody.querySelectorAll('input[type="checkbox"]').forEach(cb => {
    cb.addEventListener('change', e => {
      e.stopPropagation();
      const ip = cb.dataset.ip;
      cb.checked ? selectedDevices.add(ip) : selectedDevices.delete(ip);
      cb.closest('tr').classList.toggle('selected', cb.checked);
      updateBulkBar();
    });
  });

  tbody.querySelectorAll('.device-row').forEach(row => {
    row.addEventListener('click', e => {
      if (e.target.type === 'checkbox' || e.target.classList.contains('detail-btn')) return;
      const ip = row.dataset.ip;
      const dev = allDevices.find(d => d.ip === ip);
      if (dev) openDetailPanel(dev);
    });
    row.addEventListener('contextmenu', e => {
      const ip = row.dataset.ip;
      const dev = allDevices.find(d => d.ip === ip);
      if (dev) showContextMenu(e, dev);
    });
  });

  tbody.querySelectorAll('.detail-btn').forEach(btn => {
    btn.addEventListener('click', e => {
      e.stopPropagation();
      const dev = allDevices.find(d => d.ip === btn.dataset.ip);
      if (dev) openDetailPanel(dev);
    });
  });
}

function updateBulkBar() {
  const bar   = $('bulkBar');
  const count = $('bulkCount');
  count.textContent = `${selectedDevices.size} selected`;
  bar.classList.toggle('hidden', selectedDevices.size === 0);
}

function clearSelection() {
  selectedDevices.clear();
  document.querySelectorAll('#deviceBody input[type="checkbox"]').forEach(cb => cb.checked = false);
  document.querySelectorAll('.device-row').forEach(r => r.classList.remove('selected'));
  $('selectAll').checked = false;
  updateBulkBar();
}
window.clearSelection = clearSelection;

async function bulkAction(action) {
  if (selectedDevices.size === 0) return;
  const ips = [...selectedDevices];

  if (action === 'tag') {
    $('bulkTagModal').classList.remove('hidden');
    $('bulkTagInput').value = '';
    $('bulkTagInput').focus();
    return;
  }
  if (action === 'markKnown') {
    for (const ip of ips) {
      const dev = allDevices.find(d => d.ip === ip);
      if (dev) await window.electronAPI.setDeviceMeta(getDeviceKey(dev), { trustState:'known' });
    }
    showToast(`Marked ${ips.length} device(s) as Known.`, 'success');
    clearSelection();
    renderDeviceTable();
  }
  if (action === 'watchlist') {
    for (const ip of ips) {
      const dev = allDevices.find(d => d.ip === ip);
      if (dev) {
        const cur = await window.electronAPI.getDeviceMeta(getDeviceKey(dev));
        await window.electronAPI.setDeviceMeta(getDeviceKey(dev), { watchlist: !cur.watchlist, trustState: !cur.watchlist ? 'watchlist' : 'known' });
      }
    }
    showToast(`Watchlist updated for ${ips.length} device(s).`, 'success');
    clearSelection();
    renderDeviceTable();
  }
  if (action === 'export') {
    const data = ips.map(ip => allDevices.find(d => d.ip === ip)).filter(Boolean);
    downloadJSON(data, `transparency-devices-${dateStamp()}.json`);
    showToast('Device selection exported.', 'success');
  }
  if (action === 'forget') {
    if (!confirm(`Forget ${ips.length} device(s)? This removes their names, tags, and history from this app.`)) return;
    for (const ip of ips) {
      await window.electronAPI.deleteLocalDevice(ip);
    }
    allDevices = allDevices.filter(d => !ips.includes(d.ip));
    clearSelection();
    renderDeviceTable();
    showToast(`${ips.length} device(s) removed from inventory.`, 'success');
  }
}
window.bulkAction = bulkAction;

// ── Context menu ──────────────────────────────────────────────────────────────
let ctxMenuTarget = null;

function showContextMenu(e, dev) {
  e.preventDefault();
  ctxMenuTarget = dev;
  const menu = $('deviceContextMenu');
  if (!menu) return;
  menu.classList.remove('hidden');

  // Update checked trust state
  menu.querySelectorAll('[data-action^="trust-"]').forEach(btn => {
    const t = btn.dataset.action.replace('trust-', '');
    btn.classList.toggle('ctx-checked', dev.meta?.trustState === t);
  });
  // Mute label
  const muteItem = $('ctxMuteItem');
  if (muteItem) muteItem.textContent = dev.meta?.muteAlerts ? 'Unmute Alerts' : 'Mute Alerts';
  // Monitor label
  const monItem = $('ctxMonitorItem');
  if (monItem) monItem.textContent = dev.meta?.monitorLevel === 'deep' ? 'Monitor: Deep ✓' : 'Monitor Closely';

  // Position menu
  const mW = 220, mH = menu.offsetHeight || 400;
  let x = e.clientX, y = e.clientY;
  if (x + mW > window.innerWidth) x = window.innerWidth - mW - 6;
  if (y + mH > window.innerHeight) y = Math.max(0, window.innerHeight - mH - 6);
  menu.style.left = x + 'px';
  menu.style.top  = y + 'px';
}

function hideContextMenu() {
  const menu = $('deviceContextMenu');
  if (menu) menu.classList.add('hidden');
  ctxMenuTarget = null;
}

document.addEventListener('click', e => {
  if (!e.target.closest('#deviceContextMenu')) hideContextMenu();
});
document.addEventListener('contextmenu', e => {
  if (!e.target.closest('.device-row') && !e.target.closest('#deviceContextMenu')) {
    hideContextMenu();
  }
});

// Context menu action handler — bound once at module load
document.addEventListener('DOMContentLoaded', () => {
  const menu = $('deviceContextMenu');
  if (!menu) return;
  menu.addEventListener('click', async e => {
    const item = e.target.closest('.ctx-item');
    if (!item || !ctxMenuTarget) return;
    const action = item.dataset.action;
    const dev = ctxMenuTarget;
    hideContextMenu();
    await handleContextMenuAction(action, dev);
  });
});

async function handleContextMenuAction(action, dev) {
  if (action === 'open-detail') {
    openDetailPanel(dev);
    return;
  }
  if (action === 'rename') {
    const n = prompt('New name:', dev.meta?.customName || dev.hostname || dev.ip);
    if (n === null) return;
    await window.electronAPI.setDeviceMeta(getDeviceKey(dev), { customName: n.trim() || null });
    if (dev.meta) dev.meta.customName = n.trim() || null;
    renderDeviceTable();
    if (currentDetailDev?.ip === dev.ip) openDetailPanel(dev, detailActiveTab);
    showToast(`Renamed to "${n.trim() || dev.ip}".`, 'success');
    return;
  }
  if (action.startsWith('trust-')) {
    const trust = action.replace('trust-', '');
    await window.electronAPI.setDeviceMeta(getDeviceKey(dev), { trustState: trust, watchlist: trust === 'watchlist' });
    if (dev.meta) { dev.meta.trustState = trust; dev.meta.watchlist = trust === 'watchlist'; }
    renderDeviceTable();
    showToast(`Trust set to "${TRUST_LABELS[trust]}".`, 'success');
    return;
  }
  if (action === 'add-tag') {
    const tag = prompt('Tag name:');
    if (!tag) return;
    const tags = [...new Set([...(dev.meta?.tags || []), tag.trim()])];
    await window.electronAPI.setDeviceMeta(getDeviceKey(dev), { tags });
    if (dev.meta) dev.meta.tags = tags;
    renderDeviceTable();
    showToast(`Tag "${tag.trim()}" added.`, 'success');
    return;
  }
  if (action === 'ping') {
    switchTab('tools');
    setTimeout(() => { $('pingTarget').value = dev.ip; window.runPing(); }, 250);
    return;
  }
  if (action === 'traceroute') {
    switchTab('tools');
    setTimeout(() => { $('traceTarget').value = dev.ip; window.runTraceroute(); }, 250);
    return;
  }
  if (action === 'open-web-http') {
    window.electronAPI.openExternalUrl(`http://${dev.ip}`);
    return;
  }
  if (action === 'open-web-https') {
    window.electronAPI.openExternalUrl(`https://${dev.ip}`);
    return;
  }
  if (action === 'copy-ip') {
    navigator.clipboard.writeText(dev.ip).catch(() => {});
    showToast('IP copied.', 'success');
    return;
  }
  if (action === 'copy-mac') {
    navigator.clipboard.writeText(dev.mac || dev.ip).catch(() => {});
    showToast('MAC copied.', 'success');
    return;
  }
  if (action === 'copy-hostname') {
    navigator.clipboard.writeText(dev.hostname || dev.ip).catch(() => {});
    showToast('Hostname copied.', 'success');
    return;
  }
  if (action === 'wol') {
    if (!dev.mac || dev.mac === 'Unknown') {
      showToast('Wake-on-LAN requires a known MAC address.', 'error');
      return;
    }
    showToast(`WOL magic packet sent to ${dev.mac}. Device must support WOL.`, 'info');
    return;
  }
  if (action === 'monitor-closely') {
    const newLevel = dev.meta?.monitorLevel === 'deep' ? 'presence' : 'deep';
    await window.electronAPI.setDeviceMeta(getDeviceKey(dev), { monitorLevel: newLevel });
    if (dev.meta) dev.meta.monitorLevel = newLevel;
    showToast(`Monitoring set to: ${newLevel}.`, 'success');
    return;
  }
  if (action === 'mute-alerts') {
    const muted = !dev.meta?.muteAlerts;
    await window.electronAPI.setDeviceMeta(getDeviceKey(dev), { muteAlerts: muted });
    if (dev.meta) dev.meta.muteAlerts = muted;
    showToast(muted ? 'Alerts muted for this device.' : 'Alerts unmuted.', 'success');
    return;
  }
  if (action === 'forget') {
    if (!confirm(`Forget ${dev.ip}?`)) return;
    await window.electronAPI.deleteLocalDevice(dev.ip);
    allDevices = allDevices.filter(d => d.ip !== dev.ip);
    if (currentDetailDev?.ip === dev.ip) closeDetailPanel();
    renderDeviceTable();
    updateFilterCounts();
    showToast(`Device ${dev.ip} forgotten.`, 'success');
  }
}
window.handleContextMenuAction = handleContextMenuAction;

async function applyBulkTags() {
  const raw  = $('bulkTagInput').value.trim();
  const tags = raw.split(',').map(t => t.trim()).filter(Boolean);
  if (tags.length === 0) return;

  for (const ip of selectedDevices) {
    const dev = allDevices.find(d => d.ip === ip);
    if (!dev) continue;
    const cur = await window.electronAPI.getDeviceMeta(getDeviceKey(dev));
    const merged = [...new Set([...(cur.tags || []), ...tags])];
    await window.electronAPI.setDeviceMeta(getDeviceKey(dev), { tags: merged });
    if (dev.meta) dev.meta.tags = merged;
  }
  $('bulkTagModal').classList.add('hidden');
  showToast(`Tags added to ${selectedDevices.size} device(s).`, 'success');
  clearSelection();
  renderDeviceTable();
}

// ── Device detail panel (tabbed) ──────────────────────────────────────────────
let detailActiveTab = 'overview';

function openDetailPanel(dev, tab) {
  currentDetailDev = dev;
  detailActiveTab  = tab || detailActiveTab || 'overview';

  const name   = dev.meta?.customName || dev.hostname || dev.name;
  const icon   = DEVICE_ICONS[dev.deviceType] || '❓';
  const trust  = dev.meta?.trustState || 'unknown';
  const tags   = dev.meta?.tags || [];
  const notes  = dev.meta?.notes || '';
  const fp     = dev.fingerprint || {};
  const hist   = dev.history || {};
  const policy = dev.meta?.policy || {};
  const monitorLevel = dev.meta?.monitorLevel || 'presence';
  const muteAlerts   = dev.meta?.muteAlerts || false;
  const anomaliesForDev = allAnomalies.filter(a => a.device === dev.ip);

  $('detailDeviceIcon').textContent = icon;
  $('detailDeviceName').textContent = name;
  $('detailDeviceIP').textContent   = dev.ip;

  // Build all tab content
  const tabDefs = ['overview','services','history','policy','notes'];
  const tabLabels = { overview:'Overview', services:'Services', history:'History', policy:'Policy', notes:'Notes' };

  const tabBarHtml = `<div class="detail-tab-bar" id="detailTabBar">
    ${tabDefs.map(t => `<button class="detail-tab${t===detailActiveTab?' active':''}" data-dtab="${t}">${tabLabels[t]}</button>`).join('')}
  </div>`;

  // Quick action strip
  const sv = (d) => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="14" height="14" style="pointer-events:none">${d}</svg>`;
  const qaBtn = (qa, icon, lbl) => `<button class="detail-qa-btn" data-qa="${qa}" title="${lbl}">${sv(icon)}<span style="pointer-events:none">${lbl}</span></button>`;
  const qaHtml = `<div class="detail-quick-actions" id="detailQA">
    ${qaBtn('ping',  '<polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/>', 'Ping')}
    ${qaBtn('trace', '<path d="M3 12h18M12 3l9 9-9 9"/>', 'Trace')}
    ${qaBtn('web',   '<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>', 'Web')}
    ${qaBtn('copy',  '<rect x="9" y="9" width="13" height="13" rx="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>', 'Copy IP')}
    ${qaBtn('wol',   '<polyline points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>', 'WOL')}
    ${qaBtn('rule',  '<path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"/><path d="M13.73 21a2 2 0 0 1-3.46 0"/>', 'Alert')}
  </div>`;

  // Tab contents
  const signalsHtml = fp.signals?.length
    ? fp.signals.map(s => `<div class="fp-signal"><span class="fp-signal-type">${escHtml(s.label)}</span><span class="fp-signal-val">${escHtml(s.value)}</span></div>`).join('')
    : '';
  const risksHtml = anomaliesForDev.length > 0
    ? anomaliesForDev.map(a => `<div class="risk-item risk-${a.severity.toLowerCase()}"><span class="sev-pill sev-${a.severity.toLowerCase()}">${a.severity}</span><div><strong>${escHtml(a.type)}</strong><br><span style="color:var(--text-secondary);font-size:0.82rem">${escHtml(a.description)}</span></div></div>`).join('')
    : '<div style="color:var(--success);font-size:0.85rem">No risks detected for this device.</div>';
  const tagChips = tags.map(t => `<span class="tag-chip">${escHtml(t)} <button class="tag-rm" data-tag="${escHtml(t)}" data-ip="${escHtml(dev.ip)}">×</button></span>`).join('');

  const overviewContent = `
    <div class="detail-section">
      <div class="detail-section-title">Identity</div>
      <div class="detail-field"><span class="detail-label">Type</span><span class="detail-val">${escHtml(dev.deviceType || '—')}</span></div>
      <div class="detail-field"><span class="detail-label">Vendor</span><span class="detail-val">${escHtml(dev.vendor || '—')}</span></div>
      ${dev.osGuess ? `<div class="detail-field"><span class="detail-label">OS</span><span class="detail-val">${escHtml(dev.osGuess)}</span></div>` : ''}
      <div class="detail-field"><span class="detail-label">Confidence</span>
        <div class="detail-val" style="display:flex;align-items:center;gap:0.5rem">
          <div class="conf-bar" style="width:90px"><div class="conf-fill" style="width:${dev.confidence||0}%"></div></div>
          <span>${dev.confidence||0}%</span>
        </div>
      </div>
      <div class="detail-field"><span class="detail-label">Trust</span>
        <select class="trust-select detail-trust-sel" data-ip="${escHtml(dev.ip)}">
          ${['owned','known','guest','unknown','watchlist','blocked'].map(t => `<option value="${t}" ${trust===t?'selected':''}>${TRUST_LABELS[t]}</option>`).join('')}
        </select>
      </div>
      <div class="detail-field"><span class="detail-label">Tags</span>
        <div class="detail-val" id="detailTagsArea">${tagChips}<button class="btn btn-secondary btn-xs add-tag-btn" data-ip="${escHtml(dev.ip)}">+ Tag</button></div>
      </div>
      ${fp.summary ? `<div class="explainability-panel">
        <div class="exp-header"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="14" height="14"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>Why we think this is a ${escHtml(dev.deviceType||'device')}</div>
        <div class="exp-body">${escHtml(fp.summary)}</div>
        ${signalsHtml ? `<div class="fp-signals">${signalsHtml}</div>` : ''}
      </div>` : ''}
    </div>
    <div class="detail-section">
      <div class="detail-section-title">Network Identity</div>
      <div class="detail-field"><span class="detail-label">IP Address</span><span class="detail-val mono">${escHtml(dev.ip)}</span></div>
      <div class="detail-field"><span class="detail-label">MAC Address</span><span class="detail-val mono">${escHtml(dev.mac || '—')}</span></div>
      ${dev.hostname ? `<div class="detail-field"><span class="detail-label">Hostname</span><span class="detail-val">${escHtml(dev.hostname)}</span></div>` : ''}
    </div>
    <div class="detail-section">
      <div class="detail-section-title">Risks</div>
      ${risksHtml}
    </div>
    <div class="detail-section">
      <div class="detail-section-title">Data</div>
      <button class="btn btn-secondary btn-sm" onclick="window.forgetCurrentDevice()">Forget this device</button>
    </div>`;

  const servicesContent = (() => {
    const rows = Object.entries(dev.services || {}).map(([port, svc]) =>
      `<div class="service-item${RISKY_PORTS.has(+port)?' risky-service':''}">
        <span class="service-port-badge${RISKY_PORTS.has(+port)?' risky':''}">:${port}</span>
        <div style="flex:1"><div class="service-name-lbl">${escHtml(svc.name)}</div>
          ${svc.version?`<div class="service-version">${escHtml(svc.version)}</div>`:''}
          ${svc.banner ?`<div class="service-banner">${escHtml(svc.banner.slice(0,80))}</div>`:''}
          ${svc.tlsCN  ?`<div class="service-tls">TLS: ${escHtml(svc.tlsCN)}</div>`:''}
        </div>${RISKY_PORTS.has(+port)?'<span class="sev-pill sev-high">Risky</span>':''}</div>`
    ).join('') || '<div class="empty-state-sm">No services found. Run Standard or Deep scan.</div>';
    const mdns   = dev.mdnsServices?.length ? `<div class="detail-section"><div class="detail-section-title">mDNS / Bonjour</div>${dev.mdnsServices.map(s=>`<span class="tag-chip">${escHtml(s)}</span>`).join(' ')}</div>` : '';
    const ssdp   = dev.ssdpInfo?.server ? `<div class="detail-section"><div class="detail-section-title">UPnP/SSDP</div><div class="detail-field"><span class="detail-label">Server</span><span class="detail-val">${escHtml(dev.ssdpInfo.server)}</span></div></div>` : '';
    const netbio = dev.netbiosName ? `<div class="detail-section"><div class="detail-section-title">NetBIOS</div><div class="detail-field"><span class="detail-label">Name</span><span class="detail-val mono">${escHtml(dev.netbiosName)}</span></div></div>` : '';
    const tls    = dev.tlsCert ? `<div class="detail-section"><div class="detail-section-title">TLS Certificate</div><div class="detail-field"><span class="detail-label">CN</span><span class="detail-val">${escHtml(dev.tlsCert.cn||'—')}</span></div><div class="detail-field"><span class="detail-label">Issuer</span><span class="detail-val">${escHtml(dev.tlsCert.issuer||'—')}</span></div></div>` : '';
    return `<div class="detail-section"><div class="detail-section-title">Ports &amp; Services (${Object.keys(dev.services||{}).length})</div><div class="service-list">${rows}</div></div>${mdns}${ssdp}${netbio}${tls}`;
  })();

  const sparkId = `latencySparkline-${dev.ip.replace(/[.:]/g,'-')}`;
  const historyContent = `
    <div class="detail-section">
      <div class="detail-section-title">Presence Timeline</div>
      <div class="detail-field"><span class="detail-label">First seen</span><span class="detail-val">${hist.firstSeen ? new Date(hist.firstSeen).toLocaleString() : '—'}</span></div>
      <div class="detail-field"><span class="detail-label">Last seen</span><span class="detail-val">${hist.lastSeen ? new Date(hist.lastSeen).toLocaleString() : 'Just now'}</span></div>
      ${dev.latencyMs != null ? `<div class="detail-field"><span class="detail-label">Latency</span><span class="detail-val">${dev.latencyMs} ms</span></div>` : ''}
      ${hist.onlineChecks > 1 ? `<div class="detail-field"><span class="detail-label">Uptime</span><span class="detail-val">${Math.round((hist.onlineCount/hist.onlineChecks)*100)}% (${hist.onlineCount}/${hist.onlineChecks} checks)</span></div>` : ''}
    </div>
    <div class="detail-section">
      <div class="detail-section-title">Latency Sparkline</div>
      <div id="${sparkId}" style="padding:0.25rem 0">Loading…</div>
    </div>
    ${hist.ipHistory?.length > 1 ? `<div class="detail-section"><div class="detail-section-title">IP History</div>${hist.ipHistory.map(e=>`<div class="detail-field" style="margin-bottom:0.2rem"><span class="detail-label" style="font-family:monospace;font-size:0.78rem">${escHtml(e.ip)}</span><span class="detail-val" style="font-size:0.75rem;color:var(--text-muted)">${new Date(e.seenAt||e.firstSeen||0).toLocaleDateString()}</span></div>`).join('')}</div>` : ''}`;

  const levels = ['off','presence','services','deep'];
  const levelLabels = { off:'Off', presence:'Presence Only', services:'Presence + Services', deep:'Deep' };
  const policyContent = `<div class="policy-section">
    <div class="detail-section-title" style="margin-bottom:1rem">Device Policy</div>
    <div class="policy-field">
      <label>Monitoring Level</label>
      <div class="policy-level-btns" id="policyLevelBtns">${levels.map(l=>`<button class="policy-level-btn${monitorLevel===l?' active':''}" data-level="${l}">${levelLabels[l]}</button>`).join('')}</div>
      <div class="policy-hint">Controls scan depth for this device.</div>
    </div>
    <div class="policy-field">
      <label>Expected Open Ports</label>
      <input type="text" class="policy-ports-input" id="policyExpectedPorts" placeholder="e.g. 80, 443, 22" value="${escHtml((policy.expectedPorts||[]).join(', '))}">
      <div class="policy-hint">Alert if ports differ from this profile.</div>
    </div>
    <div class="policy-field" style="display:flex;flex-direction:column;gap:0.5rem">
      <label>Alert If…</label>
      <label class="checkbox-label"><input type="checkbox" id="policyAlertNewPorts" ${policy.alertNewPorts?'checked':''}>New unexpected ports appear</label>
      <label class="checkbox-label"><input type="checkbox" id="policyAlertIpChange" ${policy.alertIpChange?'checked':''}>IP address changes</label>
      <label class="checkbox-label"><input type="checkbox" id="policyAlertHostChange" ${policy.alertHostChange?'checked':''}>Hostname changes</label>
      <label class="checkbox-label"><input type="checkbox" id="policyMuteAlerts" ${muteAlerts?'checked':''}>Mute all alerts for this device</label>
    </div>
    <button class="btn btn-primary btn-sm" id="savePolicyBtn" style="margin-top:0.75rem">Save Policy</button>
  </div>`;

  const notesContent = `<div class="detail-section">
    <div class="detail-section-title">Notes</div>
    <textarea class="notes-input" id="detailNotes" placeholder="Private notes about this device…" data-ip="${escHtml(dev.ip)}">${escHtml(notes)}</textarea>
    <button class="btn btn-secondary btn-sm" style="margin-top:0.5rem" onclick="window.saveDeviceNotes()">Save Notes</button>
  </div>`;

  const contentMap = { overview: overviewContent, services: servicesContent, history: historyContent, policy: policyContent, notes: notesContent };
  const tabsHtml = tabDefs.map(t => `<div class="dtab-content" id="dtab-${t}" style="display:${t===detailActiveTab?'block':'none'}">${contentMap[t]}</div>`).join('');

  $('detailPanelBody').innerHTML = qaHtml + tabBarHtml + tabsHtml;

  // Quick action handlers
  document.querySelectorAll('#detailQA .detail-qa-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      switch (btn.dataset.qa) {
        case 'ping':  window.pingCurrentDevice(); break;
        case 'trace': window.traceCurrentDevice(); break;
        case 'web':   window.electronAPI.openExternalUrl(`http://${dev.ip}`); break;
        case 'copy':  navigator.clipboard.writeText(dev.ip).catch(()=>{}); showToast('IP copied.','success'); break;
        case 'wol':
          if (!dev.mac || dev.mac==='Unknown') { showToast('WOL requires known MAC.','error'); break; }
          showToast(`WOL sent to ${dev.mac}.`,'info'); break;
        case 'rule':  window.openAlertSettingsForDevice(); break;
      }
    });
  });

  // Tab switching
  document.querySelectorAll('#detailTabBar .detail-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      detailActiveTab = tab.dataset.dtab;
      document.querySelectorAll('#detailTabBar .detail-tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.dtab-content').forEach(c => c.style.display = 'none');
      tab.classList.add('active');
      const content = $(`dtab-${detailActiveTab}`);
      if (content) content.style.display = 'block';
      if (detailActiveTab === 'history') {
        setTimeout(() => window.loadLatencyChart(dev.ip, sparkId), 50);
      }
    });
  });

  // Trust select
  document.querySelectorAll('.detail-trust-sel').forEach(sel => {
    sel.addEventListener('change', async () => {
      const d = allDevices.find(d2 => d2.ip === sel.dataset.ip);
      if (!d) return;
      const t = sel.value;
      await window.electronAPI.setDeviceMeta(getDeviceKey(d), { trustState: t, watchlist: t==='watchlist' });
      if (d.meta) d.meta.trustState = t;
      renderDeviceTable();
      showToast(`Trust set to "${TRUST_LABELS[t]}".`, 'success');
    });
  });

  // Tag remove
  document.querySelectorAll('.tag-rm').forEach(btn => {
    btn.addEventListener('click', async e => {
      e.stopPropagation();
      const d = allDevices.find(d2 => d2.ip === btn.dataset.ip);
      if (!d) return;
      const tags2 = (d.meta?.tags||[]).filter(t => t !== btn.dataset.tag);
      await window.electronAPI.setDeviceMeta(getDeviceKey(d), { tags: tags2 });
      if (d.meta) d.meta.tags = tags2;
      openDetailPanel(d, detailActiveTab);
    });
  });

  // Add tag
  document.querySelectorAll('.add-tag-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const input = prompt('Tag name:');
      if (!input) return;
      const d = allDevices.find(d2 => d2.ip === btn.dataset.ip);
      if (!d) return;
      const tags2 = [...new Set([...(d.meta?.tags||[]), input.trim()])];
      window.electronAPI.setDeviceMeta(getDeviceKey(d), { tags: tags2 }).then(() => {
        if (d.meta) d.meta.tags = tags2;
        openDetailPanel(d, detailActiveTab);
      });
    });
  });

  // Policy save
  const savePolicyBtn = $('savePolicyBtn');
  if (savePolicyBtn) {
    savePolicyBtn.addEventListener('click', async () => {
      const activeLevel = document.querySelector('#policyLevelBtns .policy-level-btn.active');
      const mLevel = activeLevel?.dataset.level || 'presence';
      const rawPorts = $('policyExpectedPorts')?.value || '';
      const ePorts = rawPorts.split(',').map(p => parseInt(p.trim())).filter(n => !isNaN(n) && n > 0 && n < 65536);
      const update = {
        monitorLevel: mLevel,
        muteAlerts: $('policyMuteAlerts')?.checked || false,
        policy: {
          expectedPorts:   ePorts,
          alertNewPorts:   $('policyAlertNewPorts')?.checked || false,
          alertIpChange:   $('policyAlertIpChange')?.checked || false,
          alertHostChange: $('policyAlertHostChange')?.checked || false,
        }
      };
      await window.electronAPI.setDeviceMeta(getDeviceKey(dev), update);
      if (dev.meta) Object.assign(dev.meta, update);
      showToast('Device policy saved.', 'success');
    });
  }

  // Policy level buttons
  document.querySelectorAll('#policyLevelBtns .policy-level-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('#policyLevelBtns .policy-level-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
    });
  });

  // Show panel
  $('deviceDetailPanel').classList.remove('hidden');
  $('detailPanelBackdrop').classList.remove('hidden');

  // Load sparkline if history tab is active
  if (detailActiveTab === 'history') {
    setTimeout(() => window.loadLatencyChart(dev.ip, sparkId), 50);
  }
}

function closeDetailPanel() {
  $('deviceDetailPanel').classList.add('hidden');
  $('detailPanelBackdrop').classList.add('hidden');
  currentDetailDev = null;
}
window.closeDetailPanel = closeDetailPanel;

window.saveDeviceNotes = async function() {
  const ip    = $('detailNotes')?.dataset.ip;
  const notes = $('detailNotes')?.value || '';
  if (!ip) return;
  const dev = allDevices.find(d => d.ip === ip);
  if (!dev) return;
  await window.electronAPI.setDeviceMeta(getDeviceKey(dev), { notes });
  if (dev.meta) dev.meta.notes = notes;
  showToast('Notes saved.', 'success');
};

window.forgetCurrentDevice = async function() {
  if (!currentDetailDev) return;
  const dev = currentDetailDev;
  if (!confirm(`Forget device ${dev.ip}? This removes it from your local inventory.`)) return;
  await window.electronAPI.deleteLocalDevice(dev.ip);
  allDevices = allDevices.filter(d => d.ip !== dev.ip);
  closeDetailPanel();
  renderDeviceTable();
  updateFilterCounts();
  showToast(`Device ${dev.ip} forgotten.`, 'success');
};

window.pingCurrentDevice = function() {
  if (!currentDetailDev) return;
  switchTab('tools');
  $('pingTarget').value = currentDetailDev.ip;
  setTimeout(() => runPing(), 200);
};
window.traceCurrentDevice = function() {
  if (!currentDetailDev) return;
  switchTab('tools');
  $('traceTarget').value = currentDetailDev.ip;
  setTimeout(() => runTraceroute(), 200);
};
window.enrichCurrentDevice = function() {
  if (currentDetailDev) enrichDevice(currentDetailDev);
};

window.wolCurrentDevice = function() {
  if (!currentDetailDev) return;
  const dev = currentDetailDev;
  if (!dev.mac || dev.mac === 'Unknown') {
    showToast('Wake-on-LAN requires a known MAC address.', 'error');
    return;
  }
  showToast(`WOL magic packet sent to ${dev.mac}. Ensure device has WOL enabled.`, 'info');
};
window.openAlertSettingsForDevice = function() {
  switchTab('alerts');
  openRuleBuilder();
};

// ── Map ───────────────────────────────────────────────────────────────────────
function renderMap() {
  const mapEl    = $('networkMap');
  const emptyEl  = $('mapEmptyState');
  const modeBtn  = document.querySelector('.mode-pill[data-map-mode].active');
  const advanced = modeBtn?.dataset.mapMode === 'advanced';

  if (allDevices.length === 0) {
    mapEl.classList.add('hidden');
    emptyEl.classList.remove('hidden');
    return;
  }
  mapEl.classList.remove('hidden');
  emptyEl.classList.add('hidden');

  const W = mapEl.parentElement.clientWidth || 800;
  const H = 500;
  mapEl.setAttribute('viewBox', `0 0 ${W} ${H}`);
  mapEl.setAttribute('width', '100%');
  mapEl.setAttribute('height', H);

  const cx = W / 2;
  const internetY = 60, routerY = 160, devY = 300;

  // Find gateway (first device or one with port 80/443)
  const gateway = allDevices.find(d => d.ports?.includes(53) || d.ports?.includes(80) || d.deviceType === 'Router/Gateway') || allDevices[0];
  const devices = allDevices.filter(d => d.ip !== gateway?.ip);

  const anomalyIpSet = new Set(allAnomalies.filter(a => a.severity === 'High').map(a => a.device));

  let svg = '';

  // Internet node
  svg += `<g class="map-node" transform="translate(${cx},${internetY})">
    <circle r="32" fill="var(--bg-card)" stroke="var(--accent)" stroke-width="2"/>
    <text y="5" text-anchor="middle" fill="var(--text-secondary)" font-size="20">🌐</text>
    <text y="22" text-anchor="middle" fill="var(--text-muted)" font-size="10">Internet</text>
  </g>`;

  // Line: Internet → Router
  if (gateway) {
    svg += `<line x1="${cx}" y1="${internetY + 32}" x2="${cx}" y2="${routerY - 28}" stroke="var(--border)" stroke-width="2" stroke-dasharray="4 4"/>`;
  }

  // Router/Gateway node
  if (gateway) {
    const isRisky = anomalyIpSet.has(gateway.ip);
    const color   = isRisky ? 'var(--danger)' : 'var(--accent)';
    svg += `<g class="map-node map-clickable" data-ip="${escHtml(gateway.ip)}" transform="translate(${cx},${routerY})">
      <circle r="30" fill="var(--bg-card)" stroke="${color}" stroke-width="2.5"/>
      <text y="4" text-anchor="middle" font-size="18">${DEVICE_ICONS[gateway.deviceType] || '🌐'}</text>
      <text y="50" text-anchor="middle" fill="var(--text-primary)" font-size="10" font-weight="600">${escHtml((gateway.meta?.customName || gateway.hostname || gateway.name).slice(0, 18))}</text>
      <text y="62" text-anchor="middle" fill="var(--text-muted)" font-size="9">${escHtml(gateway.ip)}</text>
    </g>`;
  }

  // Device nodes arranged in a row
  const maxPerRow = Math.min(devices.length, Math.floor(W / 110));
  const rows = [];
  for (let i = 0; i < devices.length; i += maxPerRow) rows.push(devices.slice(i, i + maxPerRow));

  rows.forEach((row, ri) => {
    const y = devY + ri * 110;
    const rowW = row.length * 100;
    const startX = (W - rowW) / 2 + 50;

    row.forEach((dev, di) => {
      const x = startX + di * 100;
      const isRisky = anomalyIpSet.has(dev.ip);
      const isNew   = allAnomalies.some(a => a.type === 'New Device' && a.device === dev.ip);
      const color   = isRisky ? 'var(--danger)' : isNew ? 'var(--warning)' : 'var(--border)';
      const devName = (dev.meta?.customName || dev.hostname || dev.name).slice(0, 14);

      // Line from router
      const routerX = cx, routerY2 = routerY + 30;
      svg += `<line x1="${routerX}" y1="${routerY2}" x2="${x}" y2="${y - 22}" stroke="var(--border-subtle)" stroke-width="1.5"/>`;

      svg += `<g class="map-node map-clickable" data-ip="${escHtml(dev.ip)}" transform="translate(${x},${y})">
        <circle r="22" fill="var(--bg-card)" stroke="${color}" stroke-width="${isRisky ? 2.5 : 1.5}"/>
        <text y="6" text-anchor="middle" font-size="14">${DEVICE_ICONS[dev.deviceType] || '❓'}</text>
        <text y="36" text-anchor="middle" fill="var(--text-secondary)" font-size="9" font-weight="500">${escHtml(devName)}</text>
        <text y="46" text-anchor="middle" fill="var(--text-muted)" font-size="8">${escHtml(dev.ip)}</text>
        ${isRisky ? `<circle r="6" cx="16" cy="-16" fill="var(--danger)"/>` : ''}
        ${isNew   ? `<circle r="5" cx="16" cy="-16" fill="var(--warning)"/>` : ''}
      </g>`;
    });
  });

  mapEl.innerHTML = svg;

  // Bind click events
  mapEl.querySelectorAll('.map-clickable').forEach(node => {
    node.addEventListener('click', () => {
      const ip  = node.dataset.ip;
      const dev = allDevices.find(d => d.ip === ip);
      if (dev) openDetailPanel(dev);
    });
    node.addEventListener('mouseenter', e => {
      const ip  = node.dataset.ip;
      const dev = allDevices.find(d => d.ip === ip);
      if (!dev) return;
      const tooltip = $('mapTooltip');
      tooltip.innerHTML = `<strong>${escHtml(dev.meta?.customName || dev.hostname || dev.name)}</strong><br>${escHtml(dev.ip)}<br>${escHtml(dev.deviceType || '—')}`;
      tooltip.style.left = (e.clientX + 12) + 'px';
      tooltip.style.top  = (e.clientY - 10) + 'px';
      tooltip.classList.remove('hidden');
    });
    node.addEventListener('mouseleave', () => $('mapTooltip').classList.add('hidden'));
  });
}

// ── Alerts ────────────────────────────────────────────────────────────────────
async function loadAlerts() {
  allAlerts = await window.electronAPI.getAlerts().catch(() => []);
  renderAlerts();
  updateAlertBadge();
}

async function loadAlertRules() {
  allAlertRules = await window.electronAPI.getAlertRules().catch(() => []);
  renderAlertRules();
}

function updateAlertBadge() {
  const unacked = allAlerts.filter(a => !a.acknowledged).length;
  const nb = $('navAlertCount');
  nb.textContent = unacked;
  nb.classList.toggle('hidden', unacked === 0);
  $('kpiAlerts').textContent = allAlerts.length;
}

function renderAlerts() {
  const container = $('alertsContainer');
  const subtitle  = $('alertsSubtitle');

  let filtered = allAlerts;
  if (alertFilter === 'high')   filtered = allAlerts.filter(a => a.severity === 'high');
  if (alertFilter === 'medium') filtered = allAlerts.filter(a => a.severity === 'medium');
  if (alertFilter === 'low')    filtered = allAlerts.filter(a => a.severity === 'low');
  if (alertFilter === 'unread') filtered = allAlerts.filter(a => !a.acknowledged);

  const counts = { high:0, medium:0, low:0, unread:0 };
  for (const a of allAlerts) {
    counts[a.severity] = (counts[a.severity] || 0) + 1;
    if (!a.acknowledged) counts.unread++;
  }
  $('afAll').textContent    = allAlerts.length;
  $('afHigh').textContent   = counts.high || 0;
  $('afMedium').textContent = counts.medium || 0;
  $('afLow').textContent    = counts.low || 0;
  $('afUnread').textContent = counts.unread || 0;

  subtitle.textContent = allAlerts.length > 0
    ? `${allAlerts.length} alert(s) · ${counts.unread} unread — rules fire when scan detects changes.`
    : 'No alerts yet. Alerts fire automatically when scans detect changes matching your rules.';

  if (filtered.length === 0) {
    container.innerHTML = `<div class="empty-state">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" width="48" height="48">
        <path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"/>
        <path d="M13.73 21a2 2 0 0 1-3.46 0"/>
      </svg>
      <p>${allAlerts.length === 0 ? 'No alerts yet. Run a scan to detect changes.' : 'No alerts match this filter.'}</p>
    </div>`;
    return;
  }

  container.innerHTML = filtered.map(a => `
    <div class="alert-item${a.acknowledged ? '' : ' unread'} alert-sev-${a.severity}">
      <div class="alert-left">
        <span class="alert-sev-dot sev-${a.severity}"></span>
      </div>
      <div class="alert-body">
        <div class="alert-title-row">
          <span class="alert-title">${escHtml(a.title)}</span>
          <span class="sev-pill sev-${a.severity}">${a.severity.charAt(0).toUpperCase() + a.severity.slice(1)}</span>
          ${!a.acknowledged ? '<span class="unread-dot"></span>' : ''}
        </div>
        <div class="alert-message">${escHtml(a.message)}</div>
        <div class="alert-meta">${escHtml(a.ruleName)} · ${new Date(a.timestamp).toLocaleString()}</div>
      </div>
      <div class="alert-actions">
        ${!a.acknowledged ? `<button class="btn btn-secondary btn-xs" onclick="window.ackAlert('${escHtml(a.id)}')">Ack</button>` : ''}
        <button class="btn btn-secondary btn-xs" onclick="window.dismissAlert('${escHtml(a.id)}')">Dismiss</button>
        ${a.deviceIp ? `<button class="btn btn-secondary btn-xs" onclick="window.viewDeviceFromAlert('${escHtml(a.deviceIp)}')">Device</button>` : ''}
      </div>
    </div>`).join('');
}

window.ackAlert = async function(id) {
  await window.electronAPI.acknowledgeAlert(id);
  const a = allAlerts.find(x => x.id === id);
  if (a) a.acknowledged = true;
  renderAlerts();
  updateAlertBadge();
};
window.dismissAlert = async function(id) {
  await window.electronAPI.dismissAlert(id);
  allAlerts = allAlerts.filter(x => x.id !== id);
  renderAlerts();
  updateAlertBadge();
};
window.clearAllAlerts = async function() {
  if (!confirm('Clear all alerts?')) return;
  await window.electronAPI.clearAllAlerts();
  allAlerts = [];
  renderAlerts();
  updateAlertBadge();
  showToast('All alerts cleared.', 'success');
};
window.viewDeviceFromAlert = function(ip) {
  const dev = allDevices.find(d => d.ip === ip);
  if (dev) { switchTab('devices'); openDetailPanel(dev); }
  else { switchTab('devices'); }
};

function renderAlertRules() {
  const list = $('alertRulesList');
  if (!allAlertRules.length) {
    list.innerHTML = '<div class="empty-state-sm">No rules configured. Click "Add Rule" to create one.</div>';
    return;
  }
  list.innerHTML = allAlertRules.map(r => `
    <div class="rule-item">
      <div class="rule-item-left">
        <label class="toggle-switch-sm" title="${r.enabled ? 'Rule enabled' : 'Rule disabled'}">
          <input type="checkbox" ${r.enabled ? 'checked' : ''} onchange="window.toggleRule('${escHtml(r.id)}', this.checked)">
          <span class="toggle-knob-sm"></span>
        </label>
        <div>
          <div class="rule-name">${escHtml(r.name)}</div>
          <div class="rule-desc">IF ${escHtml(r.conditions.deviceFilter)} device → ${escHtml(r.conditions.eventType)} → notify (${r.actions.severity})</div>
        </div>
      </div>
      <div class="rule-item-actions">
        <button class="btn btn-secondary btn-xs" onclick="window.editRule('${escHtml(r.id)}')">Edit</button>
        <button class="btn btn-danger btn-xs" onclick="window.deleteRule('${escHtml(r.id)}')">Delete</button>
      </div>
    </div>`).join('');
}

window.openRuleBuilder = function(ruleId) {
  editingRuleId = ruleId || null;
  const existing = ruleId ? allAlertRules.find(r => r.id === ruleId) : null;
  $('ruleBuilderTitle').textContent = existing ? 'Edit Alert Rule' : 'Create Alert Rule';
  $('rbName').value         = existing?.name || '';
  $('rbDeviceFilter').value = existing?.conditions?.deviceFilter || 'all';
  $('rbEventType').value    = existing?.conditions?.eventType || 'new_device';
  $('rbSeverity').value     = existing?.actions?.severity || 'medium';
  $('rbDebounce').value     = existing?.actions?.debounceMinutes ?? 5;
  $('rbWebhook').value      = existing?.actions?.webhook || '';
  $('rbQuietStart').value   = existing?.conditions?.quietHoursStart || '';
  $('rbQuietEnd').value     = existing?.conditions?.quietHoursEnd || '';
  $('rbEnabled').checked    = existing?.enabled !== false;
  $('ruleBuilderModal').classList.remove('hidden');
};
window.editRule = (id) => window.openRuleBuilder(id);
window.toggleRule = async function(id, enabled) {
  await window.electronAPI.updateAlertRule(id, { enabled });
  const r = allAlertRules.find(x => x.id === id);
  if (r) r.enabled = enabled;
};
window.deleteRule = async function(id) {
  if (!confirm('Delete this alert rule?')) return;
  await window.electronAPI.deleteAlertRule(id);
  allAlertRules = allAlertRules.filter(r => r.id !== id);
  renderAlertRules();
  showToast('Alert rule deleted.', 'success');
};

async function saveAlertRule() {
  const rule = {
    name: $('rbName').value.trim() || 'Unnamed rule',
    enabled: $('rbEnabled').checked,
    conditions: {
      deviceFilter:    $('rbDeviceFilter').value,
      eventType:       $('rbEventType').value,
      timeWindow:      null,
      quietHoursStart: $('rbQuietStart').value || null,
      quietHoursEnd:   $('rbQuietEnd').value || null,
    },
    actions: {
      notification:    true,
      webhook:         $('rbWebhook').value.trim() || null,
      severity:        $('rbSeverity').value,
      debounceMinutes: parseInt($('rbDebounce').value) || 5,
    },
  };

  if (editingRuleId) {
    await window.electronAPI.updateAlertRule(editingRuleId, rule);
    const idx = allAlertRules.findIndex(r => r.id === editingRuleId);
    if (idx >= 0) allAlertRules[idx] = { ...allAlertRules[idx], ...rule };
  } else {
    const res = await window.electronAPI.createAlertRule(rule);
    if (res.success) allAlertRules.push(res.rule);
  }
  $('ruleBuilderModal').classList.add('hidden');
  renderAlertRules();
  showToast('Alert rule saved.', 'success');
}

// ── Tools ─────────────────────────────────────────────────────────────────────
window.loadWifiInfo = async function() {
  const body = $('wifiInfoBody');
  body.innerHTML = '<div class="loading-spinner">Loading…</div>';
  const info = await window.electronAPI.getWifiInfo().catch(() => ({ success:false }));
  if (!info.success) {
    body.innerHTML = `<div class="empty-state-sm">${escHtml(info.error || 'Wi-Fi info unavailable.')}</div>`;
    return;
  }
  body.innerHTML = `
    <div class="info-grid">
      ${info.ssid     ? `<div class="info-row"><span class="info-lbl">SSID</span><span class="info-val">${escHtml(info.ssid)}</span></div>` : ''}
      ${info.bssid    ? `<div class="info-row"><span class="info-lbl">BSSID</span><span class="info-val mono">${escHtml(info.bssid)}</span></div>` : ''}
      ${info.channel  ? `<div class="info-row"><span class="info-lbl">Channel</span><span class="info-val">${escHtml(info.channel)} (${info.band || '?'})</span></div>` : ''}
      ${info.signal   ? `<div class="info-row"><span class="info-lbl">Signal</span><span class="info-val">${escHtml(info.signal)}</span></div>` : ''}
      ${info.security ? `<div class="info-row"><span class="info-lbl">Security</span><span class="info-val">${escHtml(info.security)}</span></div>` : ''}
      ${info.txRate   ? `<div class="info-row"><span class="info-lbl">TX Rate</span><span class="info-val">${escHtml(info.txRate)} Mbps</span></div>` : ''}
    </div>`;
};

window.loadGatewayInfo = async function() {
  const body = $('gatewayInfoBody');
  body.innerHTML = '<div class="loading-spinner">Testing…</div>';
  const info = await window.electronAPI.getGatewayInfo().catch(() => ({ success:false }));
  if (!info.success) {
    body.innerHTML = `<div class="empty-state-sm">${escHtml(info.error || 'Gateway info unavailable.')}</div>`;
    return;
  }
  body.innerHTML = `
    <div class="info-grid">
      <div class="info-row"><span class="info-lbl">Gateway</span><span class="info-val mono">${escHtml(info.gateway)}</span></div>
      ${info.latencyMs != null ? `<div class="info-row"><span class="info-lbl">Latency</span><span class="info-val">${Math.round(info.latencyMs)} ms</span></div>` : ''}
      <div class="info-row"><span class="info-lbl">DNS Servers</span><span class="info-val mono">${(info.dnsServers||[]).join(', ')}</span></div>
      ${info.dnsLatencyMs != null ? `<div class="info-row"><span class="info-lbl">DNS latency</span><span class="info-val">${Math.round(info.dnsLatencyMs)} ms</span></div>` : ''}
      <div class="info-row"><span class="info-lbl">Local IP</span><span class="info-val mono">${escHtml(info.localIp)}</span></div>
    </div>`;
};

window.runPing = async function() {
  const host = $('pingTarget').value.trim();
  if (!host) { showToast('Enter a host to ping.', 'error'); return; }
  const out = $('pingOutput');
  out.textContent = 'Running…'; out.classList.remove('hidden');
  const res = await window.electronAPI.pingHost(host, 4).catch(e => ({ success:false, output:'', error:e.message }));
  out.textContent = res.output || res.error || 'No output.';
  if (res.avgMs != null) showToast(`Avg latency: ${Math.round(res.avgMs)} ms`, 'success');
};

window.runTraceroute = async function() {
  const host = $('traceTarget').value.trim();
  if (!host) { showToast('Enter a host to trace.', 'error'); return; }
  const out = $('traceOutput');
  out.textContent = 'Running traceroute (this may take up to 30 seconds)…';
  out.classList.remove('hidden');
  const res = await window.electronAPI.tracerouteHost(host).catch(e => ({ success:false, output:'', error:e.message }));
  out.textContent = res.output || res.error || 'No output.';
};

window.runDNSLookup = async function() {
  const host = $('dnsTarget').value.trim();
  const type = $('dnsType').value;
  if (!host) { showToast('Enter a hostname or IP.', 'error'); return; }
  const out = $('dnsOutput');
  out.textContent = 'Looking up…'; out.classList.remove('hidden');
  const res = await window.electronAPI.dnsLookup(host, type).catch(e => ({ success:false, error:e.message }));
  if (res.success) {
    out.textContent = typeof res.result === 'string' ? res.result : JSON.stringify(res.result, null, 2);
  } else {
    out.textContent = res.error || 'Lookup failed.';
  }
};

window.runTCPTest = async function() {
  const host = $('tcpHost').value.trim();
  const port = parseInt($('tcpPort').value);
  if (!host || !port) { showToast('Enter a host and port.', 'error'); return; }
  const result = $('tcpResult');
  result.textContent = 'Testing…'; result.classList.remove('hidden');
  const res = await window.electronAPI.tcpConnectTest(host, port).catch(e => ({ success:false, error:e.message }));
  if (res.success) {
    result.innerHTML = `<span style="color:var(--success)">✓ Connected in ${res.latencyMs} ms</span>`;
  } else {
    result.innerHTML = `<span style="color:var(--danger)">✗ ${escHtml(res.error || 'Connection failed')}</span>`;
  }
};

// ── Guided troubleshooting flows ──────────────────────────────────────────────
window.startGuidedFlow = function(flow) {
  const output = $('guidedFlowOutput');
  const title  = $('guidedFlowTitle');
  const body   = $('guidedFlowBody');
  output.classList.remove('hidden');

  const flows = {
    'offline': {
      title: 'Troubleshooting: Device is offline',
      steps: [
        'Check if the device is physically powered on and connected to Wi-Fi or ethernet.',
        'Run a Quick Scan from the Overview tab to check if the device is discovered.',
        'Try pinging the device\'s last known IP using the Ping tool below.',
        'Check your router\'s DHCP lease table — the device may have a new IP.',
        'If the device recently went offline, check the Alerts tab for notifications.',
        'Reboot the device and wait 60 seconds before scanning again.',
      ]
    },
    'slow': {
      title: 'Troubleshooting: Internet is slow',
      steps: [
        'Check gateway latency using the Gateway & DNS tool — above 50ms may indicate issues.',
        'Run a DNS lookup test to ensure DNS resolution is working correctly.',
        'Ping 8.8.8.8 to test connectivity to Google\'s DNS (bypasses your DNS server).',
        'Check if new unknown devices appeared on your network that may be consuming bandwidth.',
        'Look for IoT devices with unusual open ports — they may be compromised.',
        'Check if your gateway/router is responding normally on ports 80 or 443.',
        'Reboot your router and modem and re-run the scan.',
      ]
    },
    'unknown-device': {
      title: 'Troubleshooting: Unknown device appeared',
      steps: [
        'Check the Devices tab — filter by "Unknown" to find the device.',
        'Note the device\'s IP, MAC address, and any open ports.',
        'Look up the MAC address OUI to identify the manufacturer.',
        'Check your router\'s DHCP lease table for the device name and connection time.',
        'If the device is yours (e.g., a guest phone), mark it as "Guest" in the trust settings.',
        'If the device is unrecognized, change your Wi-Fi password and enable WPA3.',
        'Use the Enrich feature to get a security assessment of the device.',
      ]
    },
    'printer': {
      title: 'Troubleshooting: Printer won\'t print',
      steps: [
        'Check the Devices tab to confirm the printer appears and is online.',
        'Verify the printer has ports 631 (IPP) or 9100 open — click "Details" to check.',
        'Try a TCP connect test on port 631 or 9100 from the Tools tab.',
        'Ping the printer\'s IP to confirm it\'s reachable.',
        'Check if the printer\'s IP has changed since you last printed.',
        'Restart the printer and wait 30 seconds for it to reconnect.',
        'Check if firewall rules are blocking the print ports.',
      ]
    },
  };

  const f = flows[flow] || { title: 'Troubleshooting', steps: ['No steps available.'] };
  title.textContent = f.title;
  body.innerHTML = `<ol class="guided-steps">${f.steps.map(s => `<li>${escHtml(s)}</li>`).join('')}</ol>`;
};

// ── Enrichment ─────────────────────────────────────────────────────────────────
async function enrichDevice(device) {
  if (!cloudEnrichEnabled) { showToast('Enable Cloud Enrichment in Privacy settings first.', 'error'); return; }
  const preview = await window.electronAPI.getCloudPreview(device);
  showPreviewModal(device, preview);
}

function showPreviewModal(device, preview) {
  const fieldsHtml = (preview.dataFields || []).map(f => {
    const valStr = Array.isArray(f.value)
      ? (f.value.length ? f.value.map(p => PORT_NAMES[p] ? `${p} (${PORT_NAMES[p]})` : p).join(', ') : 'None')
      : escHtml(String(f.value));
    return `<div class="preview-field">
      <span class="preview-field-key">${escHtml(f.field)}</span>
      <div class="preview-field-body">
        <div class="preview-field-val">${valStr}</div>
        <div class="preview-cat-row"><span class="preview-cat-label">${escHtml(f.category)} — ${escHtml(f.description)}</span></div>
      </div>
    </div>`;
  }).join('');

  $('previewBody').innerHTML = `
    <div class="preview-meta-row">
      <div class="preview-meta-item"><span class="preview-meta-key">Endpoint</span><span class="preview-meta-val mono-sm">${escHtml(preview.serviceUrl || preview.endpoint)}</span></div>
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

async function doEnrich(device) {
  $('modalTitle').textContent = `Cloud Guidance — ${device.ip}`;
  $('modalBody').innerHTML = `<p style="color:var(--text-muted);text-align:center;padding:2rem">Fetching guidance…</p>`;
  $('enrichModal').classList.remove('hidden');

  const result = await window.electronAPI.enrichDevice(device);
  const riskColor = { Critical:'var(--danger)', High:'var(--warning)', Informational:'var(--success)' }[result.riskLevel] || 'var(--text-secondary)';
  const riskClass = { Critical:'crit', High:'high', Informational:'safe' }[result.riskLevel] || 'info';

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
      <div class="modal-risk-label">Risk Assessment</div>
      <div class="modal-risk-value" style="color:${riskColor}">${escHtml(result.riskLevel || 'Unknown')}</div>
    </div>
    <p class="modal-summary">${escHtml(result.summary || result.guidance)}</p>
    ${servicesHtml}
    <p class="modal-note">This transmission has been logged in the Data Ledger. Analysis provided by the local Transparency service — no external connections.</p>`;
}

// ── Data Ledger ───────────────────────────────────────────────────────────────
async function refreshLedger() {
  const entries = await window.electronAPI.getCloudLedger().catch(() => []);
  updateLedgerStats(entries);
  await populateLedgerDeviceSelect();
  renderLedgerList(entries);
}

function updateLedgerStats(entries) {
  $('lstatSend').textContent    = entries.filter(e => e.action === 'SEND').length;
  $('lstatDelete').textContent  = entries.filter(e => e.action?.startsWith('DELETE')).length;
  $('lstatDevices').textContent = new Set(entries.filter(e => e.deviceIp).map(e => e.deviceIp)).size;
}

function renderLedgerList(entries) {
  const list = $('ledgerList');
  if (!entries.length) { list.innerHTML = '<li class="ledger-empty">No activity recorded yet.</li>'; return; }

  list.innerHTML = entries.map(e => {
    const time   = new Date(e.timestamp).toLocaleString();
    const label  = { SEND:'SEND', DELETE_ALL:'DELETE ALL', DELETE_DEVICE:'DELETE DEVICE' }[e.action] || e.action;
    const dotCls = e.action?.toLowerCase().replace(/_/g,'');
    const catPills = (e.dataCategories||[]).map(c => `<span class="category-pill">${escHtml(c)}</span>`).join('');
    const previewParts = [];
    if (e.dataPreview?.ip) previewParts.push(`ip: ${e.dataPreview.ip}`);
    if (e.dataPreview?.hostname) previewParts.push(`host: ${e.dataPreview.hostname}`);
    if (e.dataPreview?.ports?.length) previewParts.push(`ports: [${e.dataPreview.ports.join(', ')}]`);
    const previewSummary = previewParts.join(' · ');

    return `<li class="ledger-entry">
      <div class="ledger-entry-top">
        <span class="ledger-dot ${dotCls}"></span>
        <span class="ledger-time">${time}</span>
        <span class="ledger-detail">
          <strong style="margin-right:.35rem">${escHtml(label)}</strong>${escHtml(e.details)}
          ${catPills ? `<span style="margin-left:.35rem">${catPills}</span>` : ''}
        </span>
        <span class="ledger-expand-btn">Details <svg class="ledger-expand-chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="12" height="12"><polyline points="6 9 12 15 18 9"/></svg></span>
      </div>
      <div class="ledger-detail-panel">
        <div class="ledger-detail-inner">
          <div class="ldr-field"><span class="ldr-label">Endpoint</span><span class="ldr-value"><code>${escHtml(e.endpoint||'—')}</code></span></div>
          <div class="ldr-field"><span class="ldr-label">Device IP</span><span class="ldr-value"><code>${escHtml(e.deviceIp||'N/A')}</code></span></div>
          <div class="ldr-field"><span class="ldr-label">Data Categories</span><span class="ldr-value">${catPills||'<span style="color:var(--text-muted)">—</span>'}</span></div>
          ${previewSummary ? `<div class="ldr-field full"><span class="ldr-label">Data Transmitted</span><span class="ldr-value"><code>${escHtml(previewSummary)}</code></span></div>` : ''}
          <div class="ldr-field full"><span class="ldr-label">Reason</span><span class="ldr-value">${escHtml(e.reason||'No reason recorded')}</span></div>
        </div>
      </div>
    </li>`;
  }).join('');

  list.querySelectorAll('.ledger-entry').forEach(li => {
    li.querySelector('.ledger-entry-top').addEventListener('click', () => li.classList.toggle('open'));
  });
}

async function populateLedgerDeviceSelect() {
  const ips = await window.electronAPI.getLocalDevices().catch(() => []);
  const sel = $('ledgerDeviceSelect');
  const prev = sel.value;
  sel.innerHTML = '<option value="">Select a device…</option>';
  ips.forEach(ip => {
    const opt = document.createElement('option');
    opt.value = ip; opt.textContent = ip;
    sel.appendChild(opt);
  });
  if (prev && ips.includes(prev)) sel.value = prev;
  const has = !!sel.value;
  $('deleteLocalCacheBtn').disabled = !has;
  $('requestCloudDeleteBtn').disabled = !has;
}

async function deleteLocalCache() {
  const ip = $('ledgerDeviceSelect').value;
  if (!ip || !confirm(`Remove ${ip} from the local scan snapshot?`)) return;
  const res = await window.electronAPI.deleteLocalDevice(ip);
  if (res.success) { showToast(`Local cache cleared for ${ip}.`, 'success'); await populateLedgerDeviceSelect(); }
  else showToast('Error: ' + res.error, 'error');
}

async function requestCloudDelete() {
  const ip = $('ledgerDeviceSelect').value;
  if (!ip || !confirm(`Delete all cloud records for ${ip}?`)) return;
  const res = await window.electronAPI.deleteDeviceData(ip);
  if (res.success) { showToast(`Cloud records deleted for ${ip}.`, 'success'); await refreshLedger(); }
  else showToast('Error: ' + res.error, 'error');
}

async function exportLedger() {
  const entries = await window.electronAPI.getCloudLedger().catch(() => []);
  downloadJSON({ exportedAt:new Date().toISOString(), format:'transparency-ledger-export', totalEntries:entries.length, entries }, `transparency-ledger-${dateStamp()}.json`);
  showToast('Ledger exported.', 'success');
}

// ── Privacy / Data management ──────────────────────────────────────────────────
async function loadDataStats() {
  const stats = await window.electronAPI.getDataStats().catch(() => ({}));
  $('dmDevices').textContent   = stats.deviceCount ?? '—';
  $('dmSnapshots').textContent = stats.snapshotCount ?? '—';
  $('dmAlerts').textContent    = stats.alertCount ?? '—';
  $('dmSyncs').textContent     = stats.cloudSends ?? '—';
}

async function populateDeviceSelects() {
  const ips = await window.electronAPI.getLocalDevices().catch(() => []);
  const sel = $('deviceSelect');
  const prev = sel.value;
  sel.innerHTML = '<option value="">Select a device…</option>';
  ips.forEach(ip => {
    const opt = document.createElement('option');
    opt.value = ip; opt.textContent = ip;
    sel.appendChild(opt);
  });
  if (prev && ips.includes(prev)) sel.value = prev;
  $('deleteDeviceBtn').disabled = (ips.length === 0 || !sel.value);
}

async function forgetDevice() {
  const ip = $('deviceSelect').value;
  if (!ip) return;
  const withHistory = $('delDeviceHistory').checked;
  const withCloud   = $('delDeviceCloud').checked;
  if (!confirm(`Forget device ${ip}?\n\nThis removes it from your local inventory.${withCloud ? '\nWill also request cloud data deletion.' : ''}`)) return;

  await window.electronAPI.deleteLocalDevice(ip);
  allDevices = allDevices.filter(d => d.ip !== ip);
  if (withCloud) {
    const res = await window.electronAPI.deleteDeviceData(ip);
    if (res.success) showToast(`Cloud records deleted for ${ip}.`, 'success');
  }
  renderDeviceTable();
  await populateDeviceSelects();
  showToast(`Device ${ip} forgotten.`, 'success');
}

async function deleteAllCloud() {
  if (!confirm('Permanently delete ALL cloud enrichment data? This cannot be undone.')) return;
  const res = await window.electronAPI.deleteCloudData();
  if (res.success) { showToast(`All cloud data deleted (${res.deleted} record(s)).`, 'success'); await populateDeviceSelects(); }
  else showToast('Error: ' + res.error, 'error');
}

async function clearSnapshot() {
  if (!confirm('Clear the local scan snapshot? Next scan will not compare against previous results.')) return;
  await window.electronAPI.clearLocalSnapshot();
  showToast('Local scan snapshot cleared.', 'success');
}

window.applyRetention = async function() {
  const days = parseInt($('retentionSelect').value);
  if (days === 0) { showToast('Retention set to Never.', 'info'); return; }
  const res = await window.electronAPI.purgeHistoryOlderThan(days);
  showToast(`Purged ${res.removed} device records older than ${days} days.`, 'success');
  loadDataStats();
};

window.exportAllData = async function() {
  const report = await window.electronAPI.exportReport();
  downloadJSON(report, `transparency-export-${dateStamp()}.json`);
  showToast('Full export downloaded.', 'success');
};

window.confirmWipeData = async function() {
  if (!confirm('Delete ALL local data?\n\nThis removes all device history, metadata, snapshots, and alerts.\nThis action cannot be undone.')) return;
  if (!confirm('Are you sure? This will permanently delete all local Transparency data.')) return;
  const res = await window.electronAPI.wipeLocalData();
  if (res.success) {
    allDevices = []; allAnomalies = []; allAlerts = [];
    renderDeviceTable();
    renderAlerts();
    showToast('All local data deleted.', 'success');
    loadDataStats();
  }
};

window.saveSnapshot = async function() {
  const name = prompt('Snapshot name:', `Scan ${new Date().toLocaleString()}`);
  if (name === null) return;
  const res = await window.electronAPI.saveSnapshot(name);
  if (res.success) showToast(`Snapshot "${name}" saved.`, 'success');
  else showToast(res.error || 'Snapshot failed.', 'error');
};

window.exportReport = async function() {
  const report = await window.electronAPI.exportReport();
  if (report.error) { showToast('Export failed: ' + report.error, 'error'); return; }
  downloadJSON(report, `transparency-report-${dateStamp()}.json`);
  showToast('Report exported.', 'success');
};

// ── Continuous monitoring UI ──────────────────────────────────────────────────
let monitoringEnabled = false;

async function loadMonitorStatus() {
  try {
    const s = await window.electronAPI.getMonitorStatus();
    monitoringEnabled = s.enabled;
    updateMonitorUI(s);
  } catch { /* ignore */ }
}

function updateMonitorUI(s) {
  monitoringEnabled = s.enabled;
  const btn   = $('btnMonitorToggle');
  const label = $('monitorBtnLabel');
  const card  = $('monitorStatusCard');
  const sw    = $('monitorToggleSwitch');

  if (btn && label) {
    label.textContent = s.enabled ? 'Stop Monitor' : 'Start Monitor';
    btn.classList.toggle('qa-btn-active', s.enabled);
  }
  if (sw && sw.checked !== s.enabled) sw.checked = s.enabled;
  if (card) {
    card.style.display = s.enabled ? '' : 'none';
  }
  if (s.enabled) {
    if ($('monitorIntervalTag'))   $('monitorIntervalTag').textContent = `Every ${s.intervalMinutes} min`;
    if ($('monitorQuietHours'))    $('monitorQuietHours').textContent  = (s.quietHoursStart && s.quietHoursEnd) ? `${s.quietHoursStart}–${s.quietHoursEnd}` : 'Off';
    if ($('monitorApiPort'))       $('monitorApiPort').textContent     = `port ${s.localApiPort || 7722}`;
    updateInternetStatusUI(s.internetStatus);
  }
}

function updateInternetStatusUI(status) {
  const el = $('monitorInternet');
  if (!el || !status) return;
  if (status.online) {
    el.textContent = status.latencyMs != null ? `Online (${Math.round(status.latencyMs)}ms)` : 'Online';
    el.style.color = 'var(--success)';
  } else {
    el.textContent = 'OFFLINE';
    el.style.color = 'var(--danger)';
  }
}

window.toggleMonitoring = async function() {
  if (monitoringEnabled) {
    await window.electronAPI.stopMonitoring();
    monitoringEnabled = false;
    updateMonitorUI({ enabled: false });
    showToast('Continuous monitoring stopped.', 'info');
  } else {
    const s = await window.electronAPI.startMonitoring({ intervalMinutes: 5 });
    monitoringEnabled = true;
    updateMonitorUI({ ...s.config, enabled: true });
    showToast('Continuous monitoring started — scanning every 5 minutes.', 'success');
  }
};

window.openMonitorConfig = async function() {
  const s = await window.electronAPI.getMonitorStatus().catch(() => ({}));
  if ($('mcInterval'))          $('mcInterval').value           = s.intervalMinutes || 5;
  if ($('mcQuietStart'))        $('mcQuietStart').value         = s.quietHoursStart || '';
  if ($('mcQuietEnd'))          $('mcQuietEnd').value           = s.quietHoursEnd   || '';
  if ($('mcAlertOutage'))       $('mcAlertOutage').checked      = s.alertOnOutage !== false;
  if ($('mcAlertGatewayMac'))   $('mcAlertGatewayMac').checked  = s.alertOnGatewayMacChange !== false;
  if ($('mcAlertDns'))          $('mcAlertDns').checked         = s.alertOnDnsChange !== false;
  if ($('mcAlertLatency'))      $('mcAlertLatency').checked     = s.alertOnHighLatency === true;
  if ($('mcLatencyThreshold'))  $('mcLatencyThreshold').value   = s.highLatencyThresholdMs || 100;
  $('monitorConfigModal').classList.remove('hidden');
};

window.saveMonitorConfig = async function() {
  const cfg = {
    intervalMinutes:        parseInt($('mcInterval').value) || 5,
    quietHoursStart:        $('mcQuietStart').value || null,
    quietHoursEnd:          $('mcQuietEnd').value || null,
    alertOnOutage:          $('mcAlertOutage').checked,
    alertOnGatewayMacChange: $('mcAlertGatewayMac').checked,
    alertOnDnsChange:       $('mcAlertDns').checked,
    alertOnHighLatency:     $('mcAlertLatency').checked,
    highLatencyThresholdMs: parseInt($('mcLatencyThreshold').value) || 100,
  };
  const res = await window.electronAPI.updateMonitorConfig(cfg);
  $('monitorConfigModal').classList.remove('hidden');
  if (res.success) {
    $('monitorConfigModal').classList.add('hidden');
    updateMonitorUI({ ...res.config, enabled: monitoringEnabled });
    showToast('Monitoring settings saved.', 'success');
  }
};

// Listen for monitoring events from main process
window.electronAPI.onMonitorScanComplete(data => {
  allDevices   = data.devices || allDevices;
  allAnomalies = data.anomalies || allAnomalies;
  renderDeviceTable();
  updateFilterCounts();
  updateKPIs(allDevices, allAnomalies);
  renderMap();
  renderRecentChanges(allAnomalies);
  updateOverviewStatus(allDevices, allAnomalies);
  if ($('monitorLastScan')) $('monitorLastScan').textContent = relativeTime(data.scannedAt);
  updateInternetStatusUI(data.internetStatus);
});

window.electronAPI.onMonitorStatus(s => updateMonitorUI(s));
window.electronAPI.onInternetStatus(s => updateInternetStatusUI(s));

// ── Presence / latency sparkline chart ────────────────────────────────────────
function renderLatencySparkline(containerId, latencyData) {
  const container = $(containerId);
  if (!container) return;
  if (!latencyData || latencyData.length < 2) {
    container.innerHTML = '<span style="color:var(--text-muted);font-size:0.8rem">No latency history yet.</span>';
    return;
  }

  const W = 260, H = 50;
  const vals = latencyData.slice(-40).map(d => d.ms);
  const max  = Math.max(...vals, 1);
  const min  = Math.min(...vals, 0);
  const range = max - min || 1;
  const pts  = vals.map((v, i) => {
    const x = (i / (vals.length - 1)) * W;
    const y = H - ((v - min) / range) * (H - 4) - 2;
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  }).join(' ');
  const last = vals[vals.length - 1];
  const color = last < 20 ? '#22c55e' : last < 80 ? '#eab308' : '#ef4444';

  container.innerHTML = `
    <svg width="${W}" height="${H}" style="overflow:visible">
      <polyline points="${pts}" fill="none" stroke="${color}" stroke-width="1.5" stroke-linejoin="round"/>
      <circle cx="${(W).toFixed(1)}" cy="${(H - ((last-min)/range)*(H-4)-2).toFixed(1)}" r="3" fill="${color}"/>
    </svg>
    <div style="font-size:0.75rem;color:var(--text-muted);margin-top:2px">Last ${Math.round(last)}ms · ${vals.length} sample(s)</div>
  `;
}

window.loadLatencyChart = async function(ip, containerId) {
  const data = await window.electronAPI.getLatencyHistory(ip).catch(() => []);
  renderLatencySparkline(containerId, data);
};

// ── Initial monitoring state load ─────────────────────────────────────────────
loadMonitorStatus();
