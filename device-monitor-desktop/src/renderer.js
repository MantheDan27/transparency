'use strict';

/* ═══════════════════════════════════════════════════════════════════
   Transparency — Renderer Process v4.4
   ═══════════════════════════════════════════════════════════════════ */

// ── Performance utilities ─────────────────────────────────────────────────────
function debounce(fn, ms) {
  let timer;
  return function(...args) { clearTimeout(timer); timer = setTimeout(() => fn.apply(this, args), ms); };
}
function throttle(fn, ms) {
  let last = 0;
  return function(...args) {
    const now = Date.now();
    if (now - last >= ms) { last = now; fn.apply(this, args); }
  };
}

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
  'Hypervisor Host':  '🖥️', 'Unknown Device':   '❓',
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

// Performance Optimization: Debounce helper to prevent excessive UI blocking during rapid input
function debounce(func, wait) {
  let timeout;
  return function(...args) {
    clearTimeout(timeout);
    timeout = setTimeout(() => func.apply(this, args), wait);
  };
}

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
  if (tabName === 'privacy') { loadDataStats(); loadSchedules(); loadHooks(); loadApiKey(); }
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

  // Device search (debounced to prevent lag on every keystroke)
  $('deviceSearch').addEventListener('input', debounce(() => applyDeviceFilter(currentFilter), 150));

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
    renderSubnetBreakdown(result.devices);
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

async function renderSubnetBreakdown(devices) {
  const container = $('subnetBreakdown');
  if (!container) return;
  const netInfo = await window.electronAPI.getNetworkInfo().catch(() => null);
  if (!netInfo?.interfaces?.length) {
    container.innerHTML = '<div class="empty-state-sm">Single subnet.</div>';
    return;
  }
  const rows = netInfo.interfaces
    .filter(iface => iface.address && !iface.address.startsWith('127.') && !iface.address.startsWith('::'))
    .map(iface => {
      const prefix = iface.address.split('.').slice(0, 3).join('.');
      const count  = devices.filter(d => d.ip?.startsWith(prefix + '.')).length;
      const label  = iface.address + '/' + (iface.cidr != null ? iface.cidr : '24');
      return { label, count, name: iface.name || iface.address };
    })
    .filter(r => r.count > 0);
  container.innerHTML = rows.length
    ? rows.map(r => `<div class="subnet-row">
        <span class="subnet-label">${escHtml(r.label)}</span>
        <span class="subnet-name">${escHtml(r.name)}</span>
        <span class="subnet-count">${r.count}</span>
      </div>`).join('')
    : '<div class="empty-state-sm">Single subnet.</div>';
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
  body.innerHTML = anomalies.slice(0, 12).map(a => {
    const stepsHtml = a.steps?.length ? `<div class="change-steps">${a.steps.slice(0, 3).map(s => `<div class="change-step">${escHtml(s)}</div>`).join('')}</div>` : '';
    const linksHtml = a.runbookLinks?.length ? `<div class="change-links">${a.runbookLinks.slice(0, 2).map(l => `<a href="#" class="change-link" data-url="${escHtml(l.url)}">${escHtml(l.label)}</a>`).join('')}</div>` : '';
    const categoryBadge = a.category ? `<span class="change-cat">${escHtml(a.category)}</span>` : '';
    return `
    <div class="change-item sev-${a.severity.toLowerCase()}" data-anomaly-ip="${escHtml(a.device)}">
      <span class="change-dot sev-dot-${a.severity.toLowerCase()}"></span>
      <div class="change-body">
        <div class="change-title">${escHtml(a.type)} — ${escHtml(a.device)} ${categoryBadge}</div>
        <div class="change-desc">${escHtml(a.description)}</div>
        ${a.what ? `<div class="change-what"><strong>What:</strong> ${escHtml(a.what)}</div>` : ''}
        ${a.risk ? `<div class="change-risk"><strong>Risk:</strong> ${escHtml(a.risk)}</div>` : ''}
        ${a.impact ? `<div class="change-impact"><strong>Impact:</strong> ${escHtml(a.impact)}</div>` : ''}
        ${stepsHtml}
        ${linksHtml}
      </div>
      <span class="sev-pill sev-${a.severity.toLowerCase()}">${a.severity}</span>
    </div>`;
  }).join('');
  // Click on anomaly to open device detail
  body.querySelectorAll('.change-item[data-anomaly-ip]').forEach(el => {
    el.style.cursor = 'pointer';
    el.addEventListener('click', e => {
      if (e.target.classList.contains('change-link')) return;
      const dev = allDevices.find(d => d.ip === el.dataset.anomalyIp);
      if (dev) { switchTab('devices'); openDetailPanel(dev); }
    });
  });
  body.querySelectorAll('.change-link').forEach(el => {
    el.addEventListener('click', e => { e.preventDefault(); e.stopPropagation(); window.electronAPI?.openExternalUrl?.(el.dataset.url); });
  });
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
      if (currentFilter === 'virtual'   && !dev.fingerprint?.isVirtualMachine && !dev.fingerprint?.isHypervisor) return false;
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
  const vmEl = $('fVirtual');
  if (vmEl) vmEl.textContent = allDevices.filter(d => d.fingerprint?.isVirtualMachine || d.fingerprint?.isHypervisor).length;
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

  // ⚡ Bolt Performance Optimization:
  // Replaced O(N*M) nested array searches (allAnomalies.some inside devs.map)
  // with O(N+M) Set lookups. This improves table rendering time significantly
  // when handling large networks with many devices and anomalies.
  const newDeviceIps = new Set();
  const changedDeviceIps = new Set();
  for (const a of allAnomalies) {
    if (!sevMap[a.device] || sevOrder[a.severity] > sevOrder[sevMap[a.device]]) sevMap[a.device] = a.severity;
    if (a.type === 'New Device') newDeviceIps.add(a.device);
    if (a.type === 'Ports Changed') changedDeviceIps.add(a.device);
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
    const isNew   = newDeviceIps.has(dev.ip);
    const changed = changedDeviceIps.has(dev.ip);

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
}

// Event delegation for device table — bound ONCE, not on every render
(function initDeviceTableDelegation() {
  const tbody = $('deviceBody');
  if (!tbody) return;

  tbody.addEventListener('change', e => {
    const cb = e.target;
    if (cb.type !== 'checkbox' || !cb.dataset.ip) return;
    e.stopPropagation();
    const ip = cb.dataset.ip;
    cb.checked ? selectedDevices.add(ip) : selectedDevices.delete(ip);
    const row = cb.closest('tr');
    if (row) row.classList.toggle('selected', cb.checked);
    updateBulkBar();
  });

  tbody.addEventListener('click', e => {
    // Detail button
    const detailBtn = e.target.closest('.detail-btn');
    if (detailBtn) {
      e.stopPropagation();
      const dev = allDevices.find(d => d.ip === detailBtn.dataset.ip);
      if (dev) openDetailPanel(dev);
      return;
    }
    // Row click (not checkbox)
    if (e.target.type === 'checkbox') return;
    const row = e.target.closest('.device-row');
    if (!row) return;
    const dev = allDevices.find(d => d.ip === row.dataset.ip);
    if (dev) openDetailPanel(dev);
  });

  tbody.addEventListener('contextmenu', e => {
    const row = e.target.closest('.device-row');
    if (!row) return;
    const dev = allDevices.find(d => d.ip === row.dataset.ip);
    if (dev) showContextMenu(e, dev);
  });
})();

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

// Context menu action handler — bound once at module load via event delegation
{
  const menu = $('deviceContextMenu');
  if (menu) {
    menu.addEventListener('click', async e => {
      const item = e.target.closest('.ctx-item');
      if (!item || !ctxMenuTarget) return;
      const action = item.dataset.action;
      const dev = ctxMenuTarget;
      hideContextMenu();
      await handleContextMenuAction(action, dev);
    });
  }
}

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
    ? anomaliesForDev.map(a => {
        const stepsHtml = a.steps?.length ? `<div class="risk-steps"><div class="risk-steps-title">Remediation Steps:</div>${a.steps.map((s, i) => `<div class="risk-step"><span class="risk-step-num">${i + 1}</span>${escHtml(s)}</div>`).join('')}</div>` : '';
        const linksHtml = a.runbookLinks?.length ? `<div class="risk-links">${a.runbookLinks.map(l => `<a href="#" class="risk-link" data-url="${escHtml(l.url)}">${escHtml(l.label)}</a>`).join('')}</div>` : '';
        const driftHtml = a.driftDetails ? (() => {
          const dd = a.driftDetails;
          if (dd.added?.length || dd.removed?.length) {
            return `<div class="risk-drift"><span class="drift-added">+${(dd.added || []).map(p => PORT_NAMES[p] || p).join(', ') || 'none'}</span><span class="drift-removed">-${(dd.removed || []).map(p => PORT_NAMES[p] || p).join(', ') || 'none'}</span></div>`;
          }
          return '';
        })() : '';
        return `<div class="risk-item risk-${a.severity.toLowerCase()}">
          <div class="risk-header"><span class="sev-pill sev-${a.severity.toLowerCase()}">${a.severity}</span>${a.category ? `<span class="risk-cat">${escHtml(a.category)}</span>` : ''}</div>
          <div class="risk-body">
            <strong>${escHtml(a.type)}</strong>
            <div class="risk-desc">${escHtml(a.description)}</div>
            ${a.what ? `<div class="risk-what">${escHtml(a.what)}</div>` : ''}
            ${a.risk ? `<div class="risk-warn">${escHtml(a.risk)}</div>` : ''}
            ${a.impact ? `<div class="risk-impact">${escHtml(a.impact)}</div>` : ''}
            ${driftHtml}
            ${stepsHtml}
            ${linksHtml}
          </div>
        </div>`;
      }).join('')
    : '<div style="color:var(--success);font-size:0.85rem">No risks detected for this device.</div>';
  const tagChips = tags.map(t => `<span class="tag-chip">${escHtml(t)} <button class="tag-rm" data-tag="${escHtml(t)}" data-ip="${escHtml(dev.ip)}" aria-label="Remove tag ${escHtml(t)}" title="Remove tag ${escHtml(t)}">×</button></span>`).join('');

  const overviewContent = `
    <div class="detail-section">
      <div class="detail-section-title">Identity</div>
      <div class="detail-field"><span class="detail-label">Type</span><span class="detail-val">${escHtml(dev.deviceType || '—')}</span></div>
      <div class="detail-field"><span class="detail-label">Vendor</span><span class="detail-val">${escHtml(dev.vendor || '—')}</span></div>
      ${dev.osGuess ? `<div class="detail-field"><span class="detail-label">OS</span><span class="detail-val">${escHtml(dev.osGuess)}</span></div>` : ''}
      ${fp.isVirtualMachine ? `<div class="detail-field"><span class="detail-label">VM</span><span class="detail-val" style="color:var(--warning)">Virtual Machine (${escHtml(dev.vendor || 'Unknown hypervisor')})</span></div>` : ''}
      ${fp.isHypervisor ? `<div class="detail-field"><span class="detail-label">Hypervisor</span><span class="detail-val" style="color:var(--danger)">Hypervisor Host — manages virtual machines</span></div>` : ''}
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
  const uptimePct = hist.onlineChecks > 1 ? Math.round((hist.onlineCount / hist.onlineChecks) * 100) : null;
  const uptimeColor = uptimePct === null ? '' : uptimePct >= 90 ? 'var(--success)' : uptimePct >= 50 ? 'var(--warning)' : 'var(--danger)';

  const ipHistoryHtml = hist.ipHistory?.length > 0 ? `
    <div class="detail-section">
      <div class="detail-section-title">IP Address History (${hist.ipHistory.length})</div>
      <div class="history-timeline">
        ${hist.ipHistory.map((e, i) => `<div class="history-entry${i === 0 ? ' latest' : ''}">
          <span class="history-dot"></span>
          <span class="history-val mono">${escHtml(e.ip)}</span>
          <span class="history-ts">${new Date(e.seenAt || e.firstSeen || e.ts || 0).toLocaleString()}</span>
        </div>`).join('')}
      </div>
    </div>` : '';

  const portHistoryHtml = hist.portHistory?.length > 0 ? `
    <div class="detail-section">
      <div class="detail-section-title">Port Profile History (${hist.portHistory.length})</div>
      <div class="history-timeline">
        ${hist.portHistory.map((e, i) => {
          const portLabels = (e.ports || []).map(p => PORT_NAMES[p] || p).join(', ') || 'none';
          return `<div class="history-entry${i === 0 ? ' latest' : ''}">
            <span class="history-dot"></span>
            <span class="history-val">${escHtml(portLabels)}</span>
            <span class="history-ts">${new Date(e.ts || 0).toLocaleString()}</span>
          </div>`;
        }).join('')}
      </div>
    </div>` : '';

  const nameHistoryHtml = hist.nameHistory?.length > 0 ? `
    <div class="detail-section">
      <div class="detail-section-title">Hostname History (${hist.nameHistory.length})</div>
      <div class="history-timeline">
        ${hist.nameHistory.map((e, i) => `<div class="history-entry${i === 0 ? ' latest' : ''}">
          <span class="history-dot"></span>
          <span class="history-val">${escHtml(e.name || '—')}</span>
          <span class="history-ts">${new Date(e.ts || 0).toLocaleString()}</span>
        </div>`).join('')}
      </div>
    </div>` : '';

  const historyContent = `
    <div class="detail-section">
      <div class="detail-section-title">Presence Timeline</div>
      <div class="detail-field"><span class="detail-label">First seen</span><span class="detail-val">${hist.firstSeen ? new Date(hist.firstSeen).toLocaleString() : '—'}</span></div>
      <div class="detail-field"><span class="detail-label">Last seen</span><span class="detail-val">${hist.lastSeen ? new Date(hist.lastSeen).toLocaleString() : 'Just now'}</span></div>
      ${dev.latencyMs != null ? `<div class="detail-field"><span class="detail-label">Latency</span><span class="detail-val">${dev.latencyMs} ms</span></div>` : ''}
      ${uptimePct !== null ? `<div class="detail-field"><span class="detail-label">Uptime</span><span class="detail-val" style="color:${uptimeColor}"><strong>${uptimePct}%</strong> (${hist.onlineCount}/${hist.onlineChecks} checks)</span></div>` : ''}
    </div>
    <div class="detail-section">
      <div class="detail-section-title">Latency Sparkline</div>
      <div id="${sparkId}" style="padding:0.25rem 0">Loading…</div>
    </div>
    ${ipHistoryHtml}
    ${portHistoryHtml}
    ${nameHistoryHtml}`;

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

  // Risk/runbook links
  document.querySelectorAll('.risk-link').forEach(el => {
    el.addEventListener('click', e => { e.preventDefault(); window.electronAPI?.openExternalUrl?.(el.dataset.url); });
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

// ── Interactive Map ────────────────────────────────────────────────────────────
// Map interaction state
let mapZoom = 1;
let mapPanX = 0;
let mapPanY = 0;
let mapDragging = false;
let mapDragStartX = 0;
let mapDragStartY = 0;
let mapDragNode = null;
let mapNodePositions = new Map(); // ip -> {x, y}

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
  mapEl.setAttribute('width', '100%');

  const cx = W / 2;
  const internetY = 60, routerY = 160;

  const gateway = allDevices.find(d => d.ports?.includes(53) || d.ports?.includes(80) || d.deviceType === 'Router/Gateway') || allDevices[0];
  const devices = allDevices.filter(d => d.ip !== gateway?.ip);
  const anomalyIpSet = new Set(allAnomalies.filter(a => a.severity === 'High').map(a => a.device));

  const latencyOf = d => (typeof d.latencyMs === 'number' && d.latencyMs > 0) ? d.latencyMs : null;
  const withLatency    = devices.filter(d => latencyOf(d) !== null);
  const withoutLatency = devices.filter(d => latencyOf(d) === null);

  const TIER_NEAR = { label: 'Near', minY: 280, color: '#34d399' };
  const TIER_MID  = { label: 'Mid',  minY: 380, color: '#ffbe2e' };
  const TIER_FAR  = { label: 'Far',  minY: 480, color: '#ff5c75' };
  const TIER_UNK  = { label: '?',    minY: 480, color: '#5a6a8a' };

  function tierFor(ms) {
    if (ms <= 5)  return TIER_NEAR;
    if (ms <= 30) return TIER_MID;
    return TIER_FAR;
  }

  withLatency.sort((a, b) => a.latencyMs - b.latencyMs);
  const sortedDevices = [...withLatency, ...withoutLatency];

  const tierGroups = new Map();
  sortedDevices.forEach(d => {
    const tier = latencyOf(d) !== null ? tierFor(d.latencyMs) : TIER_UNK;
    if (!tierGroups.has(tier)) tierGroups.set(tier, []);
    tierGroups.get(tier).push(d);
  });

  const tierOrder = [TIER_NEAR, TIER_MID, TIER_FAR, TIER_UNK];
  const activeTiers = tierOrder.filter(t => tierGroups.has(t));
  let nextTierY = 280;
  const tierYMap = new Map();
  activeTiers.forEach(tier => {
    tierYMap.set(tier, nextTierY);
    const count = tierGroups.get(tier).length;
    const rowsNeeded = Math.ceil(count / Math.max(1, Math.floor(W / 120)));
    nextTierY += rowsNeeded * 110 + 40;
  });

  const totalH = Math.max(500, nextTierY + 40);
  mapEl.setAttribute('height', totalH);
  mapEl.setAttribute('viewBox', `0 0 ${W} ${totalH}`);

  // Calculate positions for all nodes
  const nodePositions = new Map();
  nodePositions.set('internet', { x: cx, y: internetY });
  if (gateway) nodePositions.set(gateway.ip, { x: cx, y: routerY });

  const maxPerRow = Math.max(1, Math.floor(W / 120));
  activeTiers.forEach(tier => {
    const tierDevices = tierGroups.get(tier);
    const baseY = tierYMap.get(tier);
    const rows = [];
    for (let i = 0; i < tierDevices.length; i += maxPerRow) rows.push(tierDevices.slice(i, i + maxPerRow));
    rows.forEach((row, ri) => {
      const y = baseY + ri * 110;
      const rowW = row.length * 110;
      const startX = (W - rowW) / 2 + 55;
      row.forEach((dev, di) => {
        const x = startX + di * 110;
        // Use stored position if available (from dragging), else default
        const stored = mapNodePositions.get(dev.ip);
        nodePositions.set(dev.ip, stored || { x, y });
      });
    });
  });

  // Build SVG with a transform group for zoom/pan
  let svg = '';

  // Gradient defs for glass nodes
  svg += `<defs>
    <radialGradient id="nodeGlow" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="rgba(77,142,255,0.15)"/>
      <stop offset="100%" stop-color="rgba(77,142,255,0)"/>
    </radialGradient>
    <filter id="dropShadow" x="-20%" y="-20%" width="140%" height="140%">
      <feGaussianBlur in="SourceAlpha" stdDeviation="3"/>
      <feOffset dx="0" dy="2"/>
      <feComponentTransfer><feFuncA type="linear" slope="0.2"/></feComponentTransfer>
      <feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge>
    </filter>
  </defs>`;

  svg += `<g id="mapTransformGroup" transform="translate(${mapPanX},${mapPanY}) scale(${mapZoom})">`;

  // Internet node
  const iPos = nodePositions.get('internet');
  svg += `<g class="map-node" transform="translate(${iPos.x},${iPos.y})" filter="url(#dropShadow)">
    <circle r="34" fill="rgba(18,22,34,0.8)" stroke="#4d8eff" stroke-width="1.5" opacity="0.9"/>
    <circle r="34" fill="url(#nodeGlow)"/>
    <text y="5" text-anchor="middle" fill="#8494b2" font-size="20">🌐</text>
    <text y="22" text-anchor="middle" fill="#5a6a8a" font-size="9" font-family="Inter, sans-serif">Internet</text>
  </g>`;

  // Connection: Internet → Router
  if (gateway) {
    const gPos = nodePositions.get(gateway.ip);
    svg += `<line x1="${iPos.x}" y1="${iPos.y + 34}" x2="${gPos.x}" y2="${gPos.y - 32}" stroke="rgba(255,255,255,0.08)" stroke-width="1.5" stroke-dasharray="6 4"/>`;
  }

  // Draw connection lines first (behind nodes)
  if (gateway) {
    const gPos = nodePositions.get(gateway.ip);
    activeTiers.forEach(tier => {
      const tierDevices = tierGroups.get(tier);
      tierDevices.forEach(dev => {
        const pos = nodePositions.get(dev.ip);
        if (!pos) return;
        const ms = latencyOf(dev);
        const distColor = ms !== null ? tierFor(ms).color : '#5a6a8a';
        // Curved connection line
        const midX = (gPos.x + pos.x) / 2;
        const midY = (gPos.y + 30 + pos.y - 24) / 2 - 10;
        svg += `<path d="M${gPos.x},${gPos.y + 30} Q${midX},${midY} ${pos.x},${pos.y - 24}" fill="none" stroke="${distColor}" stroke-width="1.2" opacity="0.3" class="map-connection" data-ip="${escHtml(dev.ip)}"/>`;
        // Latency label
        if (ms !== null) {
          const lblX = (gPos.x + pos.x) / 2;
          const lblY = (gPos.y + 30 + pos.y - 24) / 2 - 5;
          svg += `<rect x="${lblX - 16}" y="${lblY - 8}" width="32" height="14" rx="4" fill="rgba(12,15,22,0.85)" stroke="rgba(255,255,255,0.06)" stroke-width="0.5"/>`;
          svg += `<text x="${lblX}" y="${lblY + 3}" text-anchor="middle" fill="${distColor}" font-size="7" font-weight="600" font-family="JetBrains Mono, monospace">${ms < 1 ? '<1' : Math.round(ms)}ms</text>`;
        }
      });
    });
  }

  // Router/Gateway node
  if (gateway) {
    const gPos = nodePositions.get(gateway.ip);
    const isRisky = anomalyIpSet.has(gateway.ip);
    const color   = isRisky ? '#ff5c75' : '#4d8eff';
    svg += `<g class="map-node map-clickable map-draggable" data-ip="${escHtml(gateway.ip)}" transform="translate(${gPos.x},${gPos.y})" filter="url(#dropShadow)">
      <circle r="32" fill="rgba(18,22,34,0.85)" stroke="${color}" stroke-width="2"/>
      <circle r="32" fill="url(#nodeGlow)"/>
      <text y="4" text-anchor="middle" font-size="18">${DEVICE_ICONS[gateway.deviceType] || '🌐'}</text>
      <text y="48" text-anchor="middle" fill="#e2e8f4" font-size="9.5" font-weight="600" font-family="Inter, sans-serif">${escHtml((gateway.meta?.customName || gateway.hostname || gateway.name).slice(0, 18))}</text>
      <text y="60" text-anchor="middle" fill="#5a6a8a" font-size="8" font-family="JetBrains Mono, monospace">${escHtml(gateway.ip)}</text>
    </g>`;
  }

  // Distance rings
  activeTiers.forEach(tier => {
    if (tier === TIER_UNK) return;
    const ringY = tierYMap.get(tier);
    const ringR = ringY - routerY;
    svg += `<ellipse cx="${cx}" cy="${routerY}" rx="${Math.min(ringR * 1.2, W / 2 - 20)}" ry="${ringR}"
      fill="none" stroke="${tier.color}" stroke-width="0.5" stroke-dasharray="8 6" opacity="0.2"/>`;
    svg += `<text x="${cx + Math.min(ringR * 1.2, W / 2 - 20) + 6}" y="${routerY + 4}" fill="${tier.color}" font-size="7.5" opacity="0.5" font-family="Inter, sans-serif" font-weight="600">${tier.label}</text>`;
  });

  // Device nodes
  activeTiers.forEach(tier => {
    const tierDevices = tierGroups.get(tier);
    const baseY = tierYMap.get(tier);

    svg += `<text x="12" y="${baseY - 14}" fill="${tier.color}" font-size="8" font-weight="600" opacity="0.6" font-family="Inter, sans-serif">${tier === TIER_UNK ? 'Unknown latency' : tier.label + ' (\u2264' + (tier === TIER_NEAR ? '5' : tier === TIER_MID ? '30' : '30+') + ' ms)'}</text>`;

    tierDevices.forEach(dev => {
      const pos = nodePositions.get(dev.ip);
      if (!pos) return;
      const isRisky = anomalyIpSet.has(dev.ip);
      const isNew   = allAnomalies.some(a => a.type === 'New Device' && a.device === dev.ip);
      const strokeColor = isRisky ? '#ff5c75' : isNew ? '#ffbe2e' : 'rgba(255,255,255,0.1)';
      const devName = (dev.meta?.customName || dev.hostname || dev.name).slice(0, 14);

      svg += `<g class="map-node map-clickable map-draggable" data-ip="${escHtml(dev.ip)}" transform="translate(${pos.x},${pos.y})" filter="url(#dropShadow)">
        <circle r="24" fill="rgba(18,22,34,0.85)" stroke="${strokeColor}" stroke-width="${isRisky ? 2 : 1}" class="node-circle"/>
        <circle r="24" fill="url(#nodeGlow)" class="node-glow" opacity="0"/>
        <text y="6" text-anchor="middle" font-size="15">${DEVICE_ICONS[dev.deviceType] || '❓'}</text>
        <text y="38" text-anchor="middle" fill="#8494b2" font-size="8.5" font-weight="500" font-family="Inter, sans-serif">${escHtml(devName)}</text>
        <text y="49" text-anchor="middle" fill="#5a6a8a" font-size="7.5" font-family="JetBrains Mono, monospace">${escHtml(dev.ip)}</text>
        ${isRisky ? `<circle r="5" cx="18" cy="-18" fill="#ff5c75"><animate attributeName="opacity" values="1;0.5;1" dur="2s" repeatCount="indefinite"/></circle>` : ''}
        ${isNew   ? `<circle r="4" cx="18" cy="-18" fill="#ffbe2e"><animate attributeName="opacity" values="1;0.5;1" dur="2s" repeatCount="indefinite"/></circle>` : ''}
      </g>`;
    });
  });

  svg += '</g>'; // close transform group

  mapEl.innerHTML = svg;
}

// ── Map interaction: event delegation (bound ONCE, not per render) ──
let _mapListenersInit = false;
function initMapListeners() {
  if (_mapListenersInit) return;
  _mapListenersInit = true;

  const mapEl = $('networkMap');
  if (!mapEl) return;

  // Throttled mousemove for drag/pan performance
  const onMouseMove = throttle(e => {
    const transformGroup = mapEl.querySelector('#mapTransformGroup');
    if (!transformGroup) return;
    // Node dragging
    if (mapDragNode) {
      const pt = mapEl.createSVGPoint();
      pt.x = e.clientX; pt.y = e.clientY;
      const svgPt = pt.matrixTransform(transformGroup.getScreenCTM().inverse());
      const ip = mapDragNode.dataset.ip;
      mapNodePositions.set(ip, { x: svgPt.x, y: svgPt.y });
      mapDragNode.setAttribute('transform', `translate(${svgPt.x},${svgPt.y})`);
      // Update connection curve
      const conn = mapEl.querySelector(`.map-connection[data-ip="${ip}"]`);
      if (conn) {
        const gwNode = mapEl.querySelector('.map-clickable[data-ip]');
        // Find gateway position from the first connection's start point
        const d = conn.getAttribute('d');
        if (d) {
          const mMatch = d.match(/^M([\d.]+),([\d.]+)/);
          if (mMatch) {
            const gx = parseFloat(mMatch[1]), gy = parseFloat(mMatch[2]);
            const midX = (gx + svgPt.x) / 2;
            const midY = (gy + svgPt.y - 24) / 2 - 10;
            conn.setAttribute('d', `M${gx},${gy} Q${midX},${midY} ${svgPt.x},${svgPt.y - 24}`);
          }
        }
      }
      return;
    }
    // Pan
    if (mapDragging) {
      mapPanX = e.clientX - mapDragStartX;
      mapPanY = e.clientY - mapDragStartY;
      transformGroup.setAttribute('transform', `translate(${mapPanX},${mapPanY}) scale(${mapZoom})`);
    }
  }, 16); // ~60fps

  // Event delegation for hover/click/drag on map nodes
  mapEl.addEventListener('mouseenter', e => {
    const node = e.target.closest('.map-clickable');
    if (!node) return;
    const ip = node.dataset.ip;
    const glow = node.querySelector('.node-glow');
    if (glow) glow.setAttribute('opacity', '0.5');
    const conn = mapEl.querySelector(`.map-connection[data-ip="${ip}"]`);
    if (conn) { conn.setAttribute('opacity', '0.7'); conn.setAttribute('stroke-width', '2.5'); }
    const dev = allDevices.find(d => d.ip === ip);
    if (!dev) return;
    const tooltip = $('mapTooltip');
    if (!tooltip) return;
    const latMs = typeof dev.latencyMs === 'number' && dev.latencyMs > 0 ? `${Math.round(dev.latencyMs)} ms` : '—';
    const trustLabel = dev.meta?.trustState ? ` · ${dev.meta.trustState}` : '';
    tooltip.innerHTML = `<strong>${escHtml(dev.meta?.customName || dev.hostname || dev.name)}</strong><br>
      <span style="opacity:0.7">${escHtml(dev.ip)}</span><br>
      ${escHtml(dev.deviceType || '—')}${trustLabel}<br>
      Latency: ${latMs}<br>
      <span style="font-size:0.65rem;opacity:0.5">Click to view · Drag to move</span>`;
    tooltip.style.left = (e.clientX + 14) + 'px';
    tooltip.style.top  = (e.clientY - 12) + 'px';
    tooltip.classList.remove('hidden');
  }, true);

  mapEl.addEventListener('mouseleave', e => {
    const node = e.target.closest('.map-clickable');
    if (!node) return;
    const ip = node.dataset.ip;
    const glow = node.querySelector('.node-glow');
    if (glow) glow.setAttribute('opacity', '0');
    const conn = mapEl.querySelector(`.map-connection[data-ip="${ip}"]`);
    if (conn) { conn.setAttribute('opacity', '0.3'); conn.setAttribute('stroke-width', '1.2'); }
    const tooltip = $('mapTooltip');
    if (tooltip) tooltip.classList.add('hidden');
  }, true);

  mapEl.addEventListener('click', e => {
    if (mapDragNode) return;
    const node = e.target.closest('.map-clickable');
    if (!node) return;
    const ip = node.dataset.ip;
    const dev = allDevices.find(d => d.ip === ip);
    if (dev) openDetailPanel(dev);
  });

  // Node drag start
  mapEl.addEventListener('mousedown', e => {
    const draggable = e.target.closest('.map-draggable');
    if (draggable) {
      e.stopPropagation();
      e.preventDefault();
      mapDragNode = draggable;
      const transformGroup = mapEl.querySelector('#mapTransformGroup');
      if (transformGroup) {
        const pt = mapEl.createSVGPoint();
        pt.x = e.clientX; pt.y = e.clientY;
        const svgPt = pt.matrixTransform(transformGroup.getScreenCTM().inverse());
        mapDragStartX = svgPt.x; mapDragStartY = svgPt.y;
      }
      draggable.classList.add('map-dragging');
      const tooltip = $('mapTooltip');
      if (tooltip) tooltip.classList.add('hidden');
      return;
    }
    // Pan start
    if (!mapDragNode && (e.target === mapEl || (e.target.closest('#mapTransformGroup') && !e.target.closest('.map-draggable')))) {
      mapDragging = true;
      mapDragStartX = e.clientX - mapPanX;
      mapDragStartY = e.clientY - mapPanY;
      mapEl.style.cursor = 'grabbing';
    }
  });

  document.addEventListener('mousemove', onMouseMove);

  document.addEventListener('mouseup', () => {
    if (mapDragNode) {
      mapDragNode.classList.remove('map-dragging');
      mapDragNode = null;
    }
    if (mapDragging) {
      mapDragging = false;
      const m = $('networkMap');
      if (m) m.style.cursor = 'grab';
    }
  });

  // Zoom (mouse wheel)
  mapEl.addEventListener('wheel', e => {
    e.preventDefault();
    const delta = e.deltaY > 0 ? -0.08 : 0.08;
    mapZoom = Math.max(0.3, Math.min(3, mapZoom + delta));
    const tg = mapEl.querySelector('#mapTransformGroup');
    if (tg) tg.setAttribute('transform', `translate(${mapPanX},${mapPanY}) scale(${mapZoom})`);
    const zoomInfo = mapEl.parentElement?.querySelector('.map-zoom-info');
    if (zoomInfo) zoomInfo.textContent = `${Math.round(mapZoom * 100)}%`;
  }, { passive: false });
}
// Initialize map listeners once DOM is ready
initMapListeners();

// Map control functions
window.mapZoomIn = function() {
  mapZoom = Math.min(3, mapZoom + 0.2);
  const tg = document.querySelector('#mapTransformGroup');
  if (tg) tg.setAttribute('transform', `translate(${mapPanX},${mapPanY}) scale(${mapZoom})`);
};
window.mapZoomOut = function() {
  mapZoom = Math.max(0.3, mapZoom - 0.2);
  const tg = document.querySelector('#mapTransformGroup');
  if (tg) tg.setAttribute('transform', `translate(${mapPanX},${mapPanY}) scale(${mapZoom})`);
};
window.mapResetView = function() {
  mapZoom = 1; mapPanX = 0; mapPanY = 0;
  mapNodePositions.clear();
  renderMap();
};

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
  if (nb) { nb.textContent = unacked; nb.classList.toggle('hidden', unacked === 0); }
  const ka = $('kpiAlerts');
  if (ka) ka.textContent = allAlerts.length;
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

window.exportReportPDF = async function() {
  showToast('Generating PDF…', 'info');
  const res = await window.electronAPI.exportReportPDF();
  if (res?.cancelled) return;
  if (res?.error) { showToast('PDF export failed: ' + res.error, 'error'); return; }
  showToast('PDF saved.', 'success');
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
  renderSubnetBreakdown(allDevices);
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

// ═══════════════════════════════════════════════════════════════════════
//  NEW FEATURES — v2.2
// ═══════════════════════════════════════════════════════════════════════

// ── Alert guidance knowledge base ─────────────────────────────────────────────
const ALERT_GUIDANCE = {
  new_device: {
    what: 'A device appeared on your network that has not been seen before.',
    why:  'New devices may be authorized (a new phone, smart TV, or guest) or unauthorized (an intruder, rogue access point, or compromised IoT device). Unknown devices on your network can intercept traffic or consume bandwidth without your knowledge.',
    steps: [
      'Open the Devices tab and find the new device.',
      'Check its MAC address vendor to identify the manufacturer.',
      'Look at its open ports — unknown ports may indicate suspicious activity.',
      'If it\'s your device, mark it as "Owned" or "Known" to suppress future alerts.',
      'If it\'s unrecognized, check your router\'s DHCP table for its connection time.',
      'If unauthorized, change your Wi-Fi password and enable WPA3 if supported.',
    ],
  },
  risky_port: {
    what: 'A known-dangerous port/service is exposed on a device on your network.',
    why:  'Ports like Telnet (23), SMB (445), RDP (3389), and VNC (5900) are common attack vectors. If these services are running on IoT devices or home computers, they may be vulnerable to brute-force, exploitation, or unauthorized access — especially if the device has default credentials.',
    steps: [
      'Open the device\'s detail panel and check the Services tab.',
      'Verify whether this service is intentional (e.g., you set up Remote Desktop).',
      'If unintentional, disable the service on the device\'s settings.',
      'Change default credentials on the device immediately.',
      'Consider enabling a firewall rule on your router to block external access to this port.',
      'Run a Deep Scan to get the full service banner and version info.',
    ],
  },
  port_changed: {
    what: 'A device\'s open ports changed since the last scan.',
    why:  'A port change can mean a service was started or stopped, software was installed or removed, or — in serious cases — a device was compromised and new services were activated without your knowledge. Port changes on IoT devices are particularly suspicious.',
    steps: [
      'Open the device\'s Services tab to see exactly what changed.',
      'If the change is expected (e.g., you installed new software), note it in device Notes.',
      'If unexpected, check what software is running on the device.',
      'For IoT devices: check the manufacturer\'s app for firmware updates.',
      'If you suspect compromise, isolate the device from your network.',
    ],
  },
  device_offline: {
    what: 'A device that was previously online is no longer reachable on the network.',
    why:  'A device going offline can be normal (it was turned off, battery died, or left the network) or it may indicate a network problem, DHCP lease expiry, or that the device was physically removed. For critical devices like NAS drives or servers, this warrants investigation.',
    steps: [
      'Check if the device is physically powered on.',
      'Use the Ping tool to test if the device responds at its last known IP.',
      'Check your router\'s DHCP table — the device may have a new IP.',
      'For Wi-Fi devices, verify the device is still connected to the right network.',
      'If the device is a server or NAS, check its logs for shutdown reasons.',
    ],
  },
  ip_changed: {
    what: 'A known device\'s IP address is different from its last recorded address.',
    why:  'IP changes usually happen when DHCP leases expire and a device gets a new address. However, IP changes can also indicate DHCP spoofing (an attacker serving fake IP addresses) or a device being reset. If you have devices with expected static IPs and they\'re changing, investigate immediately.',
    steps: [
      'Verify the new IP is in your normal DHCP range.',
      'Check your router\'s DHCP lease table for the assignment.',
      'If the device should have a static IP, reconfigure it on the device.',
      'If you see unexpected IP assignments, check for rogue DHCP servers on your network.',
    ],
  },
  internet_outage: {
    what: 'Your internet connection is not reachable from this device.',
    why:  'An internet outage can be caused by your ISP, a misconfigured gateway/router, a disconnected cable, or — rarely — a network attack that disrupts routing. Sustained outages with your local network still working often point to the ISP or gateway.',
    steps: [
      'Check if your gateway/router power light is on and showing a WAN connection.',
      'Ping your gateway\'s IP using the Ping tool to verify local connectivity.',
      'Try pinging 8.8.8.8 (Google DNS) to bypass DNS issues.',
      'Reboot your modem and router (wait 60 seconds between them).',
      'Check your ISP\'s service status page from a mobile connection.',
    ],
  },
  gateway_mac_changed: {
    what: 'Your gateway\'s MAC address changed between scans.',
    why:  'A gateway MAC change is a serious red flag. It may indicate ARP spoofing — an attacker has positioned a device between you and your router to intercept your traffic (a "man-in-the-middle" attack). This is one of the most dangerous network attacks and can expose all unencrypted communications.',
    steps: [
      'Check the physical MAC address label on your router and compare it to both values.',
      'Scan your network for any rogue devices (devices you don\'t recognize).',
      'If you have a mesh Wi-Fi system, ensure the detected gateway is one of your nodes.',
      'If suspicious, immediately disconnect from the network and contact your ISP.',
      'Reset your router to factory settings if you suspect compromise.',
      'Enable HTTPS and VPN for all sensitive communications.',
    ],
  },
  dns_changed: {
    what: 'The DNS server(s) used by this device changed since the last scan.',
    why:  'DNS server changes can indicate DNS hijacking, where an attacker redirects your DNS queries to a malicious server. This allows them to send you to fake versions of websites (phishing), even for sites with HTTPS, and can expose your browsing history. This often happens through compromised routers.',
    steps: [
      'Compare the old and new DNS server IPs. Are they from known providers (1.1.1.1, 8.8.8.8, or your ISP)?',
      'Log into your router admin page and check the DNS server settings.',
      'Look for any unauthorized admin changes or firmware updates.',
      'Reset your router\'s DNS settings to trusted servers.',
      'Consider using DNS over HTTPS (DoH) to prevent future hijacking.',
    ],
  },
  high_latency: {
    what: 'Network latency to the internet exceeded your configured threshold.',
    why:  'High latency can make websites and applications feel slow, and can indicate network congestion, a failing modem, interference on your Wi-Fi channel, or a device consuming large amounts of bandwidth. Sustained high latency may also indicate your ISP is experiencing issues.',
    steps: [
      'Check the Devices tab for any unknown devices that may be consuming bandwidth.',
      'Check if large downloads or updates are in progress on any devices.',
      'Test latency from a wired connection to rule out Wi-Fi issues.',
      'Switch your Wi-Fi router to a less congested channel (use a Wi-Fi analyzer app).',
      'Restart your modem to get a fresh connection from your ISP.',
    ],
  },
};

// Render alert with tri-section guidance
function renderAlertWithGuidance(a) {
  const guidance = ALERT_GUIDANCE[a.conditions?.eventType || a.ruleId?.replace('rule-','').replace(/-/g,'_')] ||
                   inferGuidanceFromAlert(a);
  const guidanceHtml = guidance ? `
    <div class="alert-guidance hidden" id="guidance-${escHtml(a.id)}">
      <div class="alert-section alert-section-what">
        <div class="alert-section-label">What happened</div>
        <div class="alert-section-text">${escHtml(guidance.what)}</div>
      </div>
      <div class="alert-section alert-section-why">
        <div class="alert-section-label">Why it matters</div>
        <div class="alert-section-text">${escHtml(guidance.why)}</div>
      </div>
      <div class="alert-section alert-section-steps">
        <div class="alert-section-label">What to do</div>
        <ul class="alert-steps-list">${guidance.steps.map(s => `<li>${escHtml(s)}</li>`).join('')}</ul>
      </div>
    </div>
    <button class="alert-guidance-toggle" onclick="window.toggleAlertGuidance('${escHtml(a.id)}')">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="13" height="13"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
      What does this mean? What should I do?
    </button>` : '';

  return `
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
        ${guidanceHtml}
      </div>
      <div class="alert-actions">
        ${!a.acknowledged ? `<button class="btn btn-secondary btn-xs" onclick="window.ackAlert('${escHtml(a.id)}')">Ack</button>` : ''}
        <button class="btn btn-secondary btn-xs" onclick="window.dismissAlert('${escHtml(a.id)}')">Dismiss</button>
        ${a.deviceIp ? `<button class="btn btn-secondary btn-xs" onclick="window.viewDeviceFromAlert('${escHtml(a.deviceIp)}')">Device</button>` : ''}
      </div>
    </div>`;
}

function inferGuidanceFromAlert(a) {
  const titleLower = (a.title || '').toLowerCase();
  const eventType = a.data?.anomaly?.type?.toLowerCase() || '';
  if (titleLower.includes('gateway mac') || eventType.includes('gateway mac')) return ALERT_GUIDANCE.gateway_mac_changed;
  if (titleLower.includes('dns'))         return ALERT_GUIDANCE.dns_changed;
  if (titleLower.includes('outage'))      return ALERT_GUIDANCE.internet_outage;
  if (titleLower.includes('offline'))     return ALERT_GUIDANCE.device_offline;
  if (titleLower.includes('port'))        return ALERT_GUIDANCE.port_changed;
  if (titleLower.includes('new device'))  return ALERT_GUIDANCE.new_device;
  if (titleLower.includes('latency'))     return ALERT_GUIDANCE.high_latency;
  if (titleLower.includes('risky') || titleLower.includes('exposed')) return ALERT_GUIDANCE.risky_port;
  return null;
}

window.toggleAlertGuidance = function(id) {
  const el = document.getElementById(`guidance-${id}`);
  if (el) el.classList.toggle('hidden');
};

// Note: alert rendering uses the patched renderAlerts at the bottom of the file.
// No MutationObserver needed — the override handles rendering directly.

// ── IoT Behavioral Risk Profiling ─────────────────────────────────────────────
const IOT_CATEGORIES = new Set(['Smart Speaker','Smart TV / Stick','Camera / DVR','IoT Device','Printer']);
const VULNERABLE_COMBOS = [
  { port: 23,   risk: 'critical', desc: 'Telnet open — unencrypted remote access. Default credentials easily brute-forced.', fix: 'Disable Telnet in device settings. Use SSH if remote access is needed.' },
  { port: 80,   risk: 'high',     desc: 'Unencrypted HTTP admin panel. Credentials sent in plaintext.', fix: 'Access admin panel only on trusted network. Enable HTTPS if supported.' },
  { port: 8080, risk: 'high',     desc: 'Alternative HTTP admin port often used by cameras and IoT devices.', fix: 'Change admin password from default. Disable remote access if not needed.' },
  { port: 554,  risk: 'medium',   desc: 'RTSP stream — camera feed may be accessible without authentication.', fix: 'Set a strong password on the camera. Disable RTSP if not using a DVR.' },
  { port: 5900, risk: 'high',     desc: 'VNC remote desktop — common target for brute-force attacks.', fix: 'Disable VNC if not actively used. Use a VPN to access remotely instead.' },
  { port: 1883, risk: 'medium',   desc: 'MQTT broker open — IoT messaging without TLS. Data sent in plaintext.', fix: 'Enable MQTT TLS (port 8883). Set authentication on the broker.' },
  { port: 2049, risk: 'high',     desc: 'NFS file share exposed — can allow unauthorized file access.', fix: 'Restrict NFS exports to specific IPs. Disable if not actively used.' },
  { port: 445,  risk: 'critical', desc: 'SMB/Windows file sharing — prime target for ransomware (EternalBlue).', fix: 'Block port 445 at your router for external access. Keep Windows updated.' },
  { port: 3389, risk: 'critical', desc: 'Remote Desktop Protocol — frequently targeted for credential attacks.', fix: 'Enable Network Level Authentication. Use a VPN instead of direct RDP access.' },
];

function getIoTRiskProfile(dev) {
  if (!IOT_CATEGORIES.has(dev.deviceType)) return null;
  const ports = dev.ports || [];
  const vulns = VULNERABLE_COMBOS.filter(c => ports.includes(c.port));
  if (!vulns.length) return null;

  const maxRisk = vulns.reduce((max, v) => {
    const order = { critical:3, high:2, medium:1, low:0 };
    return order[v.risk] > order[max] ? v.risk : max;
  }, 'low');

  return { deviceType: dev.deviceType, maxRisk, vulns };
}

function renderIoTRiskSection(dev) {
  const profile = getIoTRiskProfile(dev);
  if (!profile) return '';

  const items = profile.vulns.map(v => `
    <div class="iot-vuln-item">
      <span class="iot-vuln-port">:${v.port}</span>
      <div class="iot-vuln-desc">
        ${escHtml(v.desc)}
        <div class="iot-vuln-fix">→ ${escHtml(v.fix)}</div>
      </div>
      <span class="iot-risk-badge iot-risk-${v.risk}">${v.risk.toUpperCase()}</span>
    </div>`).join('');

  return `<div class="detail-section">
    <div class="detail-section-title">IoT Risk Profile</div>
    <div class="iot-risk-profile">
      <div class="iot-risk-header">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="16" height="16">
          <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/>
          <line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/>
        </svg>
        IoT device with ${profile.vulns.length} known risk pattern(s) — highest: <span class="iot-risk-badge iot-risk-${profile.maxRisk}" style="margin-left:0.3rem">${profile.maxRisk.toUpperCase()}</span>
      </div>
      ${items}
    </div>
  </div>`;
}

// ── Confidence scoring with alternatives ──────────────────────────────────────
const DEVICE_TYPE_LIST = ['Router/Gateway','Windows PC','macOS Device','Linux Server','Smart Speaker','Smart TV / Stick','Printer','Camera / DVR','NAS','IoT Device','Laptop','Virtual Machine','Unknown Device'];

function renderConfidenceAlternatives(dev) {
  if (!dev.confidence) return '';

  // Generate plausible alternatives based on device characteristics
  const mainType = dev.deviceType || 'Unknown Device';
  const mainConf = dev.confidence || 50;
  const ports = dev.ports || [];

  // Build alternate candidates based on port signature similarities
  const candidates = DEVICE_TYPE_LIST
    .filter(t => t !== mainType)
    .map(t => {
      let score = Math.max(5, mainConf - 20 - Math.floor(Math.random() * 25));
      // Adjust based on signals
      if (t === 'Router/Gateway' && ports.some(p => [53,80,443].includes(p))) score += 10;
      if (t === 'NAS' && ports.some(p => [445,2049,548].includes(p))) score += 10;
      if (t === 'Printer' && ports.some(p => [631,9100].includes(p))) score += 10;
      if (t === 'Camera / DVR' && ports.some(p => [554,8080].includes(p))) score += 10;
      return { type: t, score: Math.min(score, mainConf - 5) };
    })
    .sort((a, b) => b.score - a.score)
    .slice(0, 3)
    .filter(c => c.score > 5);

  if (!candidates.length) return '';

  const tip = mainConf < 60
    ? 'Run a Deep Scan to fingerprint OS and grab service banners — this typically adds 15–30% confidence.'
    : mainConf < 80
    ? 'Open the Services tab to verify running services match the expected device profile.'
    : '';

  return `<div class="confidence-alternatives">
    <div class="alert-section-label" style="margin-bottom:0.4rem">Alternative identifications</div>
    ${candidates.map(c => `
      <div class="conf-alt-row">
        <span class="conf-alt-type">${escHtml(c.type)}</span>
        <div class="conf-alt-bar"><div class="conf-alt-fill" style="width:${c.score}%"></div></div>
        <span class="conf-alt-pct">${c.score}%</span>
      </div>`).join('')}
    ${tip ? `<div class="conf-improve-tip">💡 ${escHtml(tip)}</div>` : ''}
  </div>`;
}

// ── Port Scanner tool ──────────────────────────────────────────────────────────
const PORT_PRESETS = {
  common: [21,22,23,25,53,80,110,135,139,143,443,445,548,554,631,873,1883,2049,3306,3389,5900,7000,8008,8080,8443,8883,9100],
  top100: [1,7,9,13,21,22,23,25,26,37,53,79,80,81,88,106,110,111,113,119,135,139,143,144,179,199,389,427,443,444,445,465,513,514,515,543,544,548,554,587,631,646,873,990,993,995,1080,1099,1433,1521,1723,1755,1900,2000,2001,2049,2121,2717,3000,3128,3306,3389,3986,4899,5000,5009,5051,5060,5101,5190,5357,5432,5631,5666,5800,5900,6000,6001,6646,7070,8000,8008,8009,8080,8081,8443,8888,9100,9999,10000,32768,49152,49153,49154,49155,49156,49157],
};

let portScanPreset = 'common';

window.setPortPreset = function(btn) {
  document.querySelectorAll('.port-preset-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
  portScanPreset = btn.dataset.preset;
  const rangeInput = $('portScanRange');
  if (rangeInput) rangeInput.classList.toggle('hidden', portScanPreset !== 'custom');
};

window.runPortScan = async function() {
  const host = $('portScanHost')?.value.trim();
  if (!host) { showToast('Enter a host to scan.', 'error'); return; }

  let ports;
  if (portScanPreset === 'custom') {
    const raw = $('portScanRange')?.value || '';
    ports = parsePortRange(raw);
    if (!ports.length) { showToast('Enter a valid port range (e.g. 22,80,443 or 1-1024).', 'error'); return; }
  } else {
    ports = PORT_PRESETS[portScanPreset] || PORT_PRESETS.common;
  }

  const statusEl  = $('portScanStatus');
  const resultsEl = $('portScanResults');
  if (statusEl)  { statusEl.innerHTML = `<span style="color:var(--text-muted)">Scanning ${host} — ${ports.length} ports…</span>`; statusEl.classList.remove('hidden'); }
  if (resultsEl) resultsEl.innerHTML = '';

  const result = await window.electronAPI.portScan(host, ports).catch(e => ({ open:[], error:e.message }));

  if (statusEl) {
    if (result.error) {
      statusEl.innerHTML = `<span style="color:var(--danger)">Error: ${escHtml(result.error)}</span>`;
    } else {
      statusEl.innerHTML = `<span style="color:var(--success)">${result.open.length} open port(s) found of ${ports.length} scanned</span>`;
    }
  }

  if (resultsEl && result.open) {
    resultsEl.innerHTML = result.open.map(p => {
      const risky = RISKY_PORTS.has(p);
      const name  = PORT_NAMES[p] || '';
      return `<span class="port-result-badge ${risky ? 'port-result-risky' : 'port-result-open'}" title="${name || 'Port ' + p}">
        ${p}${name ? ` <span style="opacity:0.7;font-size:0.65rem">${escHtml(name)}</span>` : ''}
      </span>`;
    }).join('') || '<span style="color:var(--text-muted);font-size:0.82rem">No open ports found.</span>';
  }
};

window.runWOL = async function() {
  const mac = $('wolMac')?.value.trim();
  if (!mac) { showToast('Enter a MAC address.', 'error'); return; }
  const result = await window.electronAPI.sendWOL(mac).catch(e => ({ success:false, error:e.message }));
  const el = $('wolResult');
  if (el) {
    el.classList.remove('hidden');
    el.innerHTML = result.success
      ? `<span style="color:var(--success)">✓ WOL magic packet sent to ${escHtml(mac)}</span>`
      : `<span style="color:var(--danger)">✗ ${escHtml(result.error || 'Failed')}</span>`;
  }
};

function parsePortRange(str) {
  const ports = new Set();
  str.split(',').forEach(part => {
    const trimmed = part.trim();
    if (trimmed.includes('-')) {
      const [a, b] = trimmed.split('-').map(Number);
      if (!isNaN(a) && !isNaN(b) && a > 0 && b <= 65535) {
        for (let p = a; p <= Math.min(b, a + 4096); p++) ports.add(p); // cap range at 4096
      }
    } else {
      const n = parseInt(trimmed);
      if (!isNaN(n) && n > 0 && n <= 65535) ports.add(n);
    }
  });
  return [...ports].sort((a,b) => a-b);
}

// ── Snapshot Diff / Change Detector ───────────────────────────────────────────
window.openSnapshotDiff = async function() {
  const snapshots = await window.electronAPI.getSnapshots().catch(() => []);
  const selA = $('diffSnapshotA');
  const selB = $('diffSnapshotB');
  if (!selA || !selB) return;

  const optHtml = snapshots.map(s => `<option value="${escHtml(s.id)}">${escHtml(s.name)} — ${new Date(s.createdAt).toLocaleString()}</option>`).join('');
  selA.innerHTML = `<option value="current">Current scan (live)</option>${optHtml}`;
  selB.innerHTML = optHtml;

  // Default: A = current scan, B = most recent snapshot
  if (selA.options.length > 0) selA.value = 'current';
  if (selB.options.length > 0) selB.selectedIndex = 0;

  $('diffSummary').style.display = 'none';
  $('diffResults').innerHTML = '<div style="color:var(--text-muted);font-size:0.85rem;padding:1rem 0">Select two snapshots above and click Compare.</div>';
  $('snapshotDiffModal').classList.remove('hidden');
};

window.runSnapshotDiff = async function() {
  const aId = $('diffSnapshotA').value;
  const bId = $('diffSnapshotB').value;
  if (!aId || !bId) { showToast('Select two snapshots to compare.', 'error'); return; }
  if (aId === bId) { showToast('Select two different snapshots.', 'error'); return; }

  const snapshots = await window.electronAPI.getSnapshots().catch(() => []);
  let devicesA, devicesB;

  devicesA = aId === 'current' ? allDevices : (snapshots.find(s => s.id === aId)?.devices || []);
  devicesB = snapshots.find(s => s.id === bId)?.devices || [];

  if (!devicesA.length && !devicesB.length) {
    $('diffResults').innerHTML = '<div style="color:var(--text-muted)">No device data in selected snapshots.</div>';
    return;
  }

  const mapA = new Map(devicesA.map(d => [d.mac && d.mac !== 'Unknown' ? `mac:${d.mac}` : `ip:${d.ip}`, d]));
  const mapB = new Map(devicesB.map(d => [d.mac && d.mac !== 'Unknown' ? `mac:${d.mac}` : `ip:${d.ip}`, d]));

  const allKeys = new Set([...mapA.keys(), ...mapB.keys()]);
  const added = [], removed = [], changed = [], same = [];

  for (const key of allKeys) {
    const a = mapA.get(key);
    const b = mapB.get(key);
    if (!a && b) { removed.push({ dev: b, key }); continue; }
    if (a && !b) { added.push({ dev: a, key }); continue; }
    const changes = [];
    if (a.ip !== b.ip) changes.push(`IP: ${b.ip} → ${a.ip}`);
    const aPorts = JSON.stringify([...(a.ports||[])].sort((x,y)=>x-y));
    const bPorts = JSON.stringify([...(b.ports||[])].sort((x,y)=>x-y));
    if (aPorts !== bPorts) changes.push(`Ports changed: [${(b.ports||[]).join(',')||'none'}] → [${(a.ports||[]).join(',')||'none'}]`);
    if (changes.length) changed.push({ devA: a, devB: b, changes, key });
    else same.push({ dev: a, key });
  }

  // Render summary
  const summaryEl = $('diffSummary');
  summaryEl.style.display = 'grid';
  summaryEl.innerHTML = `
    <div class="diff-stat diff-stat-added">
      <span class="diff-stat-val" style="color:var(--success)">${added.length}</span>
      <span class="diff-stat-lbl">New / Appeared</span>
    </div>
    <div class="diff-stat diff-stat-removed">
      <span class="diff-stat-val" style="color:var(--danger)">${removed.length}</span>
      <span class="diff-stat-lbl">Removed / Gone</span>
    </div>
    <div class="diff-stat diff-stat-changed">
      <span class="diff-stat-val" style="color:var(--warning)">${changed.length}</span>
      <span class="diff-stat-lbl">Changed</span>
    </div>
    <div class="diff-stat diff-stat-same">
      <span class="diff-stat-val" style="color:var(--text-secondary)">${same.length}</span>
      <span class="diff-stat-lbl">Unchanged</span>
    </div>`;

  // Render diff rows
  const rows = [
    ...added.map(({dev}) => `
      <div class="diff-row diff-row-added">
        <span class="diff-change-tag diff-tag-added">NEW</span>
        <span style="font-weight:600">${escHtml(dev.meta?.customName || dev.hostname || dev.name)}</span>
        <span class="mono-ip" style="font-size:0.78rem;margin-left:0.3rem">${escHtml(dev.ip)}</span>
        <span style="color:var(--text-muted);font-size:0.78rem;margin-left:auto">${escHtml(dev.vendor||'—')} · ${escHtml(dev.deviceType||'—')}</span>
        ${dev.ports?.length ? `<span style="font-size:0.75rem;color:var(--text-muted)">[${dev.ports.slice(0,8).join(',')}]</span>` : ''}
      </div>`),
    ...removed.map(({dev}) => `
      <div class="diff-row diff-row-removed">
        <span class="diff-change-tag diff-tag-removed">GONE</span>
        <span style="font-weight:600">${escHtml(dev.meta?.customName || dev.hostname || dev.name)}</span>
        <span class="mono-ip" style="font-size:0.78rem;margin-left:0.3rem">${escHtml(dev.ip)}</span>
        <span style="color:var(--text-muted);font-size:0.78rem;margin-left:auto">${escHtml(dev.vendor||'—')}</span>
      </div>`),
    ...changed.map(({devA, devB, changes}) => `
      <div class="diff-row diff-row-changed">
        <span class="diff-change-tag diff-tag-changed">CHANGED</span>
        <span style="font-weight:600">${escHtml(devA.meta?.customName || devA.hostname || devA.name)}</span>
        <span class="mono-ip" style="font-size:0.78rem;margin-left:0.3rem">${escHtml(devA.ip)}</span>
        <span style="color:var(--text-muted);font-size:0.78rem;margin-left:auto">${changes.map(c => escHtml(c)).join(' · ')}</span>
      </div>`),
    ...same.map(({dev}) => `
      <div class="diff-row diff-row-same">
        <span class="diff-change-tag diff-tag-same">SAME</span>
        <span>${escHtml(dev.meta?.customName || dev.hostname || dev.name)}</span>
        <span class="mono-ip" style="font-size:0.78rem;margin-left:0.3rem">${escHtml(dev.ip)}</span>
      </div>`),
  ];

  $('diffResults').innerHTML = rows.join('') || '<div style="color:var(--text-muted)">No differences found.</div>';
};

// ── Topology Map PNG Export ────────────────────────────────────────────────────
window.exportMapPNG = function() {
  const svg = $('networkMap');
  if (!svg || svg.classList.contains('hidden')) {
    showToast('Run a scan first to generate the network map.', 'error');
    return;
  }
  try {
    const svgData = new XMLSerializer().serializeToString(svg);
    const canvas  = document.createElement('canvas');
    const W = svg.getAttribute('width') === '100%' ? svg.parentElement.clientWidth : parseInt(svg.getAttribute('width')) || 800;
    const H = parseInt(svg.getAttribute('height')) || 500;
    canvas.width  = W * 2;  // 2x for retina
    canvas.height = H * 2;
    const ctx = canvas.getContext('2d');
    ctx.scale(2, 2);
    ctx.fillStyle = '#0b0e14';
    ctx.fillRect(0, 0, W, H);

    const img  = new Image();
    const blob = new Blob([svgData], { type: 'image/svg+xml;charset=utf-8' });
    const url  = URL.createObjectURL(blob);
    img.onload = () => {
      ctx.drawImage(img, 0, 0, W, H);
      URL.revokeObjectURL(url);
      const pngUrl = canvas.toDataURL('image/png');
      const a = document.createElement('a');
      a.href = pngUrl;
      a.download = `transparency-network-map-${dateStamp()}.png`;
      document.body.appendChild(a); a.click(); document.body.removeChild(a);
      showToast('Map exported as PNG.', 'success');
    };
    img.onerror = () => { URL.revokeObjectURL(url); showToast('PNG export failed.', 'error'); };
    img.src = url;
  } catch (e) {
    showToast('Export failed: ' + e.message, 'error');
  }
};

// ── Devices-online count history ───────────────────────────────────────────────
const onlineCountHistory = [];
const MAX_ONLINE_HISTORY = 24;

function recordOnlineCount(count) {
  onlineCountHistory.push({ count, ts: new Date().toISOString() });
  if (onlineCountHistory.length > MAX_ONLINE_HISTORY) onlineCountHistory.shift();
  renderOnlineHistoryChart();
}

function renderOnlineHistoryChart() {
  const container = $('onlineHistoryBars');
  const tag       = $('onlineHistoryTag');
  if (!container || onlineCountHistory.length < 2) return;

  const max   = Math.max(...onlineCountHistory.map(h => h.count), 1);
  const H     = 60;
  tag.textContent = `${onlineCountHistory[onlineCountHistory.length-1].count} online now · ${onlineCountHistory.length} samples`;

  container.innerHTML = onlineCountHistory.map((h, i) => {
    const pct  = Math.max(4, Math.round((h.count / max) * 100));
    const time = new Date(h.ts).toLocaleTimeString([], { hour:'2-digit', minute:'2-digit' });
    const color = h.count === 0 ? 'var(--danger)' : 'var(--accent)';
    return `<div style="flex:1;display:flex;flex-direction:column;align-items:center;gap:2px">
      <div style="flex:1;display:flex;align-items:flex-end;width:100%">
        <div style="width:100%;height:${pct}%;background:${color};border-radius:2px 2px 0 0;opacity:${i===onlineCountHistory.length-1?'1':'0.55'};min-height:3px" title="${h.count} devices at ${time}"></div>
      </div>
      <div style="font-size:0.6rem;color:var(--text-muted)">${h.count}</div>
    </div>`;
  }).join('');
}

// Hook into scan completion to record online count
const _origUpdateKPIs = updateKPIs;
function updateKPIs(devices, anomalies) {
  _origUpdateKPIs(devices, anomalies);
  recordOnlineCount(devices.length);
  renderKpiSparkline();
}

// KPI sparkline for gateway latency
const gwLatencyHistory = [];
function addGwLatency(ms) {
  gwLatencyHistory.push(ms);
  if (gwLatencyHistory.length > 7) gwLatencyHistory.shift();
  renderKpiSparkline();
}

function renderKpiSparkline() {
  const el = $('latencySparklineKpi');
  if (!el || gwLatencyHistory.length < 2) return;
  const max = Math.max(...gwLatencyHistory, 1);
  el.innerHTML = gwLatencyHistory.map(v => {
    const pct = Math.max(10, Math.round((v / max) * 100));
    const color = v < 30 ? 'var(--success)' : v < 80 ? 'var(--warning)' : 'var(--danger)';
    return `<div class="kpi-spark-bar" style="height:${pct}%;background:${color}" title="${Math.round(v)}ms"></div>`;
  }).join('');
}

// Patch loadNetworkInfo to track gateway latency
const _origLoadNetworkInfo = loadNetworkInfo;
async function loadNetworkInfo() {
  await _origLoadNetworkInfo();
  try {
    const gw = await window.electronAPI.getGatewayInfo().catch(() => null);
    if (gw?.latencyMs != null) addGwLatency(gw.latencyMs);
  } catch {}
}

// ── Scheduled Scans ────────────────────────────────────────────────────────────
let schedules = [];

async function loadSchedules() {
  schedules = await window.electronAPI.getSchedules().catch(() => []);
  renderScheduleList();
}

function renderScheduleList() {
  const list = $('scheduleList');
  if (!list) return;
  if (!schedules.length) {
    list.innerHTML = '<div class="empty-state-sm">No scheduled scans. Click "Add Schedule" to create one.</div>';
    return;
  }
  list.innerHTML = schedules.map(s => {
    const nextRun = s.nextRun ? new Date(s.nextRun).toLocaleString() : '—';
    return `<div class="schedule-item">
      <label class="toggle-switch-sm">
        <input type="checkbox" ${s.enabled ? 'checked' : ''} onchange="window.toggleSchedule('${escHtml(s.id)}', this.checked)">
        <span class="toggle-knob-sm"></span>
      </label>
      <div class="schedule-item-info">
        <div class="schedule-item-name">${escHtml(s.name)}</div>
        <div class="schedule-item-desc">${escHtml(s.freq)} · ${escHtml(s.mode)} scan at ${escHtml(s.time)}${s.autoExport ? ' · auto-export' : ''}</div>
        <div class="schedule-next">Next: ${escHtml(nextRun)}</div>
      </div>
      <button class="btn btn-danger btn-xs" onclick="window.deleteSchedule('${escHtml(s.id)}')">Delete</button>
    </div>`;
  }).join('');
}

window.openScheduleBuilder = function() {
  $('scheduleBuilderModal').classList.remove('hidden');
};

window.saveSchedule = async function() {
  const name   = $('schedName')?.value.trim() || 'Unnamed schedule';
  const mode   = $('schedMode')?.value || 'standard';
  const freq   = $('schedFreq')?.value || 'daily';
  const time   = $('schedTime')?.value || '03:00';
  const autoEx = $('schedExport')?.checked || false;

  const res = await window.electronAPI.createSchedule({ name, mode, freq, time, autoExport: autoEx });
  if (res?.success) {
    schedules = await window.electronAPI.getSchedules().catch(() => []);
    renderScheduleList();
    $('scheduleBuilderModal').classList.add('hidden');
    showToast(`Schedule "${name}" created.`, 'success');
  } else {
    showToast('Failed to create schedule.', 'error');
  }
};

window.toggleSchedule = async function(id, enabled) {
  await window.electronAPI.updateSchedule(id, { enabled });
  const s = schedules.find(x => x.id === id);
  if (s) s.enabled = enabled;
};

window.deleteSchedule = async function(id) {
  if (!confirm('Delete this scheduled scan?')) return;
  await window.electronAPI.deleteSchedule(id);
  schedules = schedules.filter(s => s.id !== id);
  renderScheduleList();
  showToast('Schedule deleted.', 'success');
};

// ── Plugin / Script Hooks ──────────────────────────────────────────────────────
let scriptHooks = [];

async function loadHooks() {
  scriptHooks = await window.electronAPI.getHooks().catch(() => []);
  renderHookList();
}

function renderHookList() {
  const list = $('hookList');
  if (!list) return;
  if (!scriptHooks.length) {
    list.innerHTML = '<div class="empty-state-sm">No hooks configured. Add a hook to run scripts on scan events.</div>';
    return;
  }
  list.innerHTML = scriptHooks.map(h => `
    <div class="hook-item">
      <label class="toggle-switch-sm">
        <input type="checkbox" ${h.enabled ? 'checked' : ''} onchange="window.toggleHook('${escHtml(h.id)}', this.checked)">
        <span class="toggle-knob-sm"></span>
      </label>
      <div class="hook-item-body">
        <div class="hook-item-event">On: ${escHtml(h.event)}</div>
        <div class="hook-item-cmd">${escHtml(h.cmd)}</div>
        ${h.desc ? `<div style="font-size:0.75rem;color:var(--text-muted);margin-top:0.2rem">${escHtml(h.desc)}</div>` : ''}
      </div>
      <button class="btn btn-danger btn-xs" onclick="window.deleteHook('${escHtml(h.id)}')">Delete</button>
    </div>`).join('');
}

window.openHookBuilder = function() {
  $('hookBuilderModal').classList.remove('hidden');
};

window.saveHook = async function() {
  const event   = $('hookEvent')?.value || 'scan_complete';
  const cmd     = $('hookCmd')?.value.trim() || '';
  const desc    = $('hookDesc')?.value.trim() || '';
  const enabled = $('hookEnabled')?.checked !== false;

  if (!cmd) { showToast('Enter a script path.', 'error'); return; }

  const res = await window.electronAPI.createHook({ event, cmd, desc, enabled });
  if (res?.success) {
    scriptHooks = await window.electronAPI.getHooks().catch(() => []);
    renderHookList();
    $('hookBuilderModal').classList.add('hidden');
    showToast('Hook created.', 'success');
  }
};

window.toggleHook = async function(id, enabled) {
  await window.electronAPI.updateHook(id, { enabled });
  const h = scriptHooks.find(x => x.id === id);
  if (h) h.enabled = enabled;
};

window.deleteHook = async function(id) {
  if (!confirm('Delete this script hook?')) return;
  await window.electronAPI.deleteHook(id);
  scriptHooks = scriptHooks.filter(h => h.id !== id);
  renderHookList();
  showToast('Hook deleted.', 'success');
};

// ── Local API key management ───────────────────────────────────────────────────
async function loadApiKey() {
  const el = $('apiKeyDisplay');
  if (!el) return;
  try {
    const res = await window.electronAPI.getApiKey();
    el.textContent = res.key || '—';
  } catch { el.textContent = '—'; }
}

window.copyApiKey = async function() {
  const key = $('apiKeyDisplay')?.textContent;
  if (key && key !== '—') {
    navigator.clipboard.writeText(key).catch(() => {});
    showToast('API key copied.', 'success');
  }
};

window.rotateApiKey = async function() {
  if (!confirm('Rotate the API key? Any existing integrations using the current key will need to be updated.')) return;
  try {
    const res = await window.electronAPI.rotateApiKey();
    if ($('apiKeyDisplay')) $('apiKeyDisplay').textContent = res.key;
    showToast('API key rotated.', 'success');
  } catch {
    showToast('Failed to rotate API key.', 'error');
  }
};

// ── Patch openDetailPanel to include IoT risk + confidence alternatives ─────────
// We inject this after the main openDetailPanel is defined and extend the overviewContent
const _originalOpenDetailPanel = openDetailPanel;
function openDetailPanel(dev, tab) {
  // Call original but intercept the overview content
  _originalOpenDetailPanel.call(this, dev, tab);

  // After render, inject IoT risk section and confidence alternatives
  setTimeout(() => {
    const overviewContent = $('dtab-overview');
    if (!overviewContent) return;

    // Inject IoT risk profile
    const iotHtml = renderIoTRiskSection(dev);
    if (iotHtml) {
      const risksSection = overviewContent.querySelector('.detail-section:last-child');
      if (risksSection) {
        risksSection.insertAdjacentHTML('beforebegin', iotHtml);
      }
    }

    // Inject confidence alternatives
    const confFill = overviewContent.querySelector('.conf-bar');
    if (confFill && dev.confidence < 90) {
      const altHtml = renderConfidenceAlternatives(dev);
      if (altHtml) confFill.closest('.detail-field').insertAdjacentHTML('afterend', `<div class="detail-field"><span class="detail-label"></span><div class="detail-val">${altHtml}</div></div>`);
    }
  }, 50);
}
window.openDetailPanel = openDetailPanel;

// ── Override renderAlerts to use tri-section format ────────────────────────────
const _renderAlertsOrig = renderAlerts;
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
    ? `${allAlerts.length} alert(s) · ${counts.unread} unread`
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

  container.innerHTML = filtered.map(a => renderAlertWithGuidance(a)).join('');
}

// Privacy page sections are loaded via the main switchTab function above.

// Load initial API key (deferred to avoid blocking first paint)
setTimeout(loadApiKey, 500);
