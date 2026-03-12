import { db } from "./firebase-config.js";
import {
  collection, doc, addDoc, setDoc, getDoc, getDocs, deleteDoc,
  query, orderBy, limit, serverTimestamp, onSnapshot
} from "https://www.gstatic.com/firebasejs/11.4.0/firebase-firestore.js";

// --- DOM References ---
const networkList = document.getElementById("network-list");
const addNetworkBtn = document.getElementById("add-network-btn");
const networkModal = document.getElementById("network-modal");
const networkForm = document.getElementById("network-form");
const closeModalBtn = document.getElementById("close-modal");
const deviceTableBody = document.getElementById("device-table-body");
const deviceCount = document.getElementById("device-count");
const networkFilter = document.getElementById("network-filter");
const uploadScanBtn = document.getElementById("upload-scan-btn");
const uploadModal = document.getElementById("upload-modal");
const uploadForm = document.getElementById("upload-form");
const closeUploadModal = document.getElementById("close-upload-modal");
const alertFeed = document.getElementById("alert-feed");
const deviceSearch = document.getElementById("device-search");
const detailPanel = document.getElementById("device-detail");
const detailClose = document.getElementById("detail-close");

// --- Overview DOM ---
const kpiDevices = document.getElementById("kpi-devices");
const kpiNetworks = document.getElementById("kpi-networks");
const kpiAlerts = document.getElementById("kpi-alerts");
const kpiSync = document.getElementById("kpi-sync");
const kpiSyncSub = document.getElementById("kpi-sync-sub");
const overviewDeviceBody = document.getElementById("overview-device-body");
const overviewAlerts = document.getElementById("overview-alerts");
const viewAllDevicesBtn = document.getElementById("view-all-devices-btn");

let activeNetworkId = null;
let unsubDevices = null;
let unsubAlerts = null;
let allDevicesCache = [];
let allNetworksCache = [];

// ========== DASHBOARD INIT ==========
window.loadDashboard = async function (user) {
  await loadNetworks(user.uid);
  loadAlerts(user.uid);
  loadOverviewData(user.uid);
};

// ========== SIDEBAR NAVIGATION ==========
document.querySelectorAll(".nav-item").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".nav-item").forEach((b) => b.classList.remove("active"));
    document.querySelectorAll(".tab-panel").forEach((p) => p.classList.add("hidden"));
    btn.classList.add("active");
    const panel = document.getElementById(`panel-${btn.dataset.tab}`);
    if (panel) panel.classList.remove("hidden");
  });
});

// View all devices from overview
viewAllDevicesBtn.addEventListener("click", () => {
  document.querySelector('[data-tab="devices"]').click();
});

// ========== OVERVIEW ==========
async function loadOverviewData(uid) {
  // Networks count
  kpiNetworks.textContent = allNetworksCache.length;

  // Load all devices for overview
  const devSnap = await getDocs(
    query(collection(db, "users", uid, "devices"), orderBy("lastSeen", "desc"))
  );

  const devices = [];
  devSnap.forEach((d) => {
    devices.push({ id: d.id, ...d.data() });
  });

  allDevicesCache = devices;
  kpiDevices.textContent = devices.length;

  // Trust breakdown
  updateTrustBreakdown(devices);

  // Recent devices (top 5)
  renderOverviewDevices(devices.slice(0, 5));

  // Check desktop sync
  checkDesktopSync(uid);
}

function updateTrustBreakdown(devices) {
  const counts = { owned: 0, known: 0, guest: 0, unknown: 0, blocked: 0, watchlist: 0 };
  const total = devices.length || 1;

  for (const d of devices) {
    const trust = (d.trust || "unknown").toLowerCase();
    if (trust in counts) counts[trust]++;
    else counts.unknown++;
  }

  for (const [key, count] of Object.entries(counts)) {
    const bar = document.getElementById(`trust-bar-${key}`);
    const countEl = document.getElementById(`trust-count-${key}`);
    if (bar) bar.style.width = `${(count / total) * 100}%`;
    if (countEl) countEl.textContent = count;
  }
}

function renderOverviewDevices(devices) {
  if (devices.length === 0) {
    overviewDeviceBody.innerHTML = '<tr><td colspan="6" class="empty-state-sm">No devices yet</td></tr>';
    return;
  }

  overviewDeviceBody.innerHTML = devices.map((d) => `
    <tr>
      <td><span class="trust-dot trust-${(d.trust || 'unknown').toLowerCase()}"></span>${escHtml(d.name || d.hostname || "Unknown")}</td>
      <td class="mono">${escHtml(d.mac || "")}</td>
      <td class="mono">${escHtml(d.ip || "")}</td>
      <td>${escHtml(d.deviceType || "Unknown")}</td>
      <td><span class="trust-badge trust-${(d.trust || 'unknown').toLowerCase()}">${escHtml(d.trust || "Unknown")}</span></td>
      <td class="mono">${d.lastSeen ? timeAgo(toDate(d.lastSeen)) : "N/A"}</td>
    </tr>
  `).join("");
}

async function checkDesktopSync(uid) {
  // Check if user has API endpoint configured
  const userDoc = await getDoc(doc(db, "users", uid));
  if (userDoc.exists()) {
    const data = userDoc.data();
    if (data.apiEndpoint) {
      kpiSync.textContent = "On";
      kpiSync.style.color = "var(--accent-green)";
      kpiSyncSub.textContent = "connected";
    } else {
      kpiSync.textContent = "Off";
      kpiSyncSub.textContent = "not connected";
    }
  } else {
    kpiSync.textContent = "Off";
    kpiSyncSub.textContent = "not connected";
  }
}

// ========== NETWORKS ==========
async function loadNetworks(uid) {
  const snap = await getDocs(collection(db, "users", uid, "networks"));
  networkList.innerHTML = "";
  networkFilter.innerHTML = '<option value="all">All Networks</option>';
  const uploadNetworkSelect = document.getElementById("upload-network");
  uploadNetworkSelect.innerHTML = '<option value="">Select a network</option>';

  allNetworksCache = [];

  if (snap.empty) {
    networkList.innerHTML = '<div class="empty-state">No networks yet. Add one to get started.</div>';
    return;
  }

  snap.forEach((d) => {
    const net = d.data();
    allNetworksCache.push({ id: d.id, ...net });
    addNetworkCard(d.id, net);

    const opt = document.createElement("option");
    opt.value = d.id;
    opt.textContent = net.name;
    networkFilter.appendChild(opt);

    const opt2 = document.createElement("option");
    opt2.value = d.id;
    opt2.textContent = net.name;
    uploadNetworkSelect.appendChild(opt2);
  });
}

function addNetworkCard(id, net) {
  const card = document.createElement("div");
  card.className = "network-card";
  card.dataset.id = id;
  card.innerHTML = `
    <div class="network-card-header">
      <h3>${escHtml(net.name)}</h3>
      ${net.subnet ? `<span class="network-subnet">${escHtml(net.subnet)}</span>` : ""}
    </div>
    <div class="network-card-meta">
      <span>${net.deviceCount || 0} devices</span>
      <span>${net.location || "Unknown location"}</span>
    </div>
    <div class="network-card-actions">
      <button class="btn-sm btn-view" data-id="${id}">View Devices</button>
      <button class="btn-sm btn-delete" data-id="${id}">Remove</button>
    </div>
  `;
  networkList.appendChild(card);

  card.querySelector(".btn-view").addEventListener("click", () => {
    activeNetworkId = id;
    networkFilter.value = id;
    loadDevices(currentUid(), id);
    document.querySelector('[data-tab="devices"]').click();
  });

  card.querySelector(".btn-delete").addEventListener("click", async () => {
    if (confirm(`Delete network "${net.name}"?`)) {
      await deleteDoc(doc(db, "users", currentUid(), "networks", id));
      card.remove();
    }
  });
}

// Add network modal
addNetworkBtn.addEventListener("click", () => {
  networkModal.classList.remove("hidden");
});
closeModalBtn.addEventListener("click", () => {
  networkModal.classList.add("hidden");
});

networkForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  const name = document.getElementById("net-name").value.trim();
  const subnet = document.getElementById("net-subnet").value.trim();
  const location = document.getElementById("net-location").value.trim();
  const apiEndpoint = document.getElementById("net-api-endpoint").value.trim();

  const uid = currentUid();
  const ref = await addDoc(collection(db, "users", uid, "networks"), {
    name,
    subnet,
    location,
    apiEndpoint,
    deviceCount: 0,
    createdAt: serverTimestamp()
  });

  addNetworkCard(ref.id, { name, subnet, location, deviceCount: 0 });
  networkModal.classList.add("hidden");
  networkForm.reset();
  kpiNetworks.textContent = (allNetworksCache.length + 1);
});

// ========== DEVICES ==========
networkFilter.addEventListener("change", () => {
  const val = networkFilter.value;
  if (val === "all") {
    loadAllDevices(currentUid());
  } else {
    loadDevices(currentUid(), val);
  }
});

// Device search
deviceSearch.addEventListener("input", () => {
  const term = deviceSearch.value.toLowerCase();
  const rows = deviceTableBody.querySelectorAll("tr.device-row");
  rows.forEach((row) => {
    const text = row.textContent.toLowerCase();
    row.style.display = text.includes(term) ? "" : "none";
  });
});

async function loadAllDevices(uid) {
  if (unsubDevices) unsubDevices();

  deviceTableBody.innerHTML = '<tr><td colspan="7" class="loading">Loading devices...</td></tr>';

  const devSnap = await getDocs(
    query(collection(db, "users", uid, "devices"), orderBy("lastSeen", "desc"))
  );

  const devices = [];
  devSnap.forEach((d) => {
    devices.push({ id: d.id, ...d.data() });
  });

  allDevicesCache = devices;
  renderDeviceTable(devices);
}

function loadDevices(uid, networkId) {
  if (unsubDevices) unsubDevices();

  deviceTableBody.innerHTML = '<tr><td colspan="7" class="loading">Loading...</td></tr>';

  unsubDevices = onSnapshot(
    query(collection(db, "users", uid, "devices"), orderBy("lastSeen", "desc")),
    (snap) => {
      const devices = [];
      snap.forEach((d) => {
        const dev = d.data();
        if (dev.networkId === networkId) {
          devices.push({ id: d.id, ...dev });
        }
      });
      allDevicesCache = devices;
      renderDeviceTable(devices);
    }
  );
}

function renderDeviceTable(devices) {
  deviceCount.textContent = devices.length;

  if (devices.length === 0) {
    deviceTableBody.innerHTML = '<tr><td colspan="7" class="empty-state">No devices found. Upload a scan or connect your desktop app.</td></tr>';
    return;
  }

  deviceTableBody.innerHTML = devices.map((d, i) => `
    <tr class="device-row" data-index="${i}" data-trust="${d.trust || 'unknown'}">
      <td><span class="trust-dot trust-${(d.trust || 'unknown').toLowerCase()}"></span>${escHtml(d.name || d.hostname || "Unknown")}</td>
      <td class="mono">${escHtml(d.mac || "")}</td>
      <td class="mono">${escHtml(d.ip || "")}</td>
      <td>${escHtml(d.vendor || "Unknown")}</td>
      <td>${escHtml(d.deviceType || "Unknown")}</td>
      <td><span class="trust-badge trust-${(d.trust || 'unknown').toLowerCase()}">${escHtml(d.trust || "Unknown")}</span></td>
      <td class="mono">${d.lastSeen ? timeAgo(toDate(d.lastSeen)) : "N/A"}</td>
    </tr>
  `).join("");

  // Attach row click for detail panel
  deviceTableBody.querySelectorAll(".device-row").forEach((row) => {
    row.addEventListener("click", () => {
      const idx = parseInt(row.dataset.index, 10);
      openDeviceDetail(allDevicesCache[idx]);
      // Highlight selected row
      deviceTableBody.querySelectorAll(".device-row").forEach((r) => r.classList.remove("selected"));
      row.classList.add("selected");
    });
  });
}

// ========== DEVICE DETAIL PANEL ==========
function openDeviceDetail(device) {
  if (!device) return;

  document.getElementById("detail-name").textContent = device.name || device.hostname || "Unknown Device";

  const trustBadge = document.getElementById("detail-trust-badge");
  const trust = (device.trust || "unknown").toLowerCase();
  trustBadge.textContent = device.trust || "Unknown";
  trustBadge.className = `trust-badge trust-${trust}`;

  document.getElementById("detail-mac").textContent = device.mac || "—";
  document.getElementById("detail-ip").textContent = device.ip || "—";
  document.getElementById("detail-vendor").textContent = device.vendor || "—";
  document.getElementById("detail-type").textContent = device.deviceType || "—";
  document.getElementById("detail-reason").textContent = device.classificationReason || "No classification data";

  document.getElementById("detail-first-seen").textContent = device.firstSeen
    ? formatDate(toDate(device.firstSeen))
    : "—";
  document.getElementById("detail-last-seen").textContent = device.lastSeen
    ? formatDate(toDate(device.lastSeen))
    : "—";
  document.getElementById("detail-sightings").textContent = device.sightings || "1";

  // Ports
  const portsEl = document.getElementById("detail-ports");
  const ports = device.ports || [];
  if (ports.length > 0) {
    portsEl.innerHTML = ports.map((p) => {
      const portNum = typeof p === "object" ? p.port : p;
      return `<span class="port-chip">${escHtml(String(portNum))}</span>`;
    }).join("");
  } else {
    portsEl.innerHTML = '<span class="empty-state-sm">No ports detected</span>';
  }

  // Show panel with animation
  detailPanel.classList.remove("hidden");
  requestAnimationFrame(() => {
    detailPanel.classList.add("visible");
  });

  // Trust action buttons
  setupTrustActions(device);
}

function closeDetailPanel() {
  detailPanel.classList.remove("visible");
  setTimeout(() => {
    detailPanel.classList.add("hidden");
  }, 300);
  deviceTableBody.querySelectorAll(".device-row").forEach((r) => r.classList.remove("selected"));
}

detailClose.addEventListener("click", closeDetailPanel);

function setupTrustActions(device) {
  const mac = (device.mac || "").toUpperCase().replace(/[^A-F0-9]/g, "");
  if (!mac) return;

  document.getElementById("detail-trust-btn").onclick = () => updateDeviceTrust(mac, "Owned");
  document.getElementById("detail-watch-btn").onclick = () => updateDeviceTrust(mac, "Watchlist");
  document.getElementById("detail-block-btn").onclick = () => updateDeviceTrust(mac, "Blocked");
}

async function updateDeviceTrust(mac, trust) {
  const uid = currentUid();
  if (!uid || !mac) return;

  await setDoc(doc(db, "users", uid, "devices", mac), { trust }, { merge: true });

  // Update cache and re-render
  const cached = allDevicesCache.find((d) => {
    const m = (d.mac || "").toUpperCase().replace(/[^A-F0-9]/g, "");
    return m === mac;
  });
  if (cached) {
    cached.trust = trust;
    renderDeviceTable(allDevicesCache);
    openDeviceDetail(cached);
  }
}

// ========== SCAN UPLOAD ==========
uploadScanBtn.addEventListener("click", () => {
  uploadModal.classList.remove("hidden");
});
closeUploadModal.addEventListener("click", () => {
  uploadModal.classList.add("hidden");
});

uploadForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  const fileInput = document.getElementById("scan-file");
  const networkId = document.getElementById("upload-network").value;
  const file = fileInput.files[0];

  if (!file || !networkId) {
    alert("Please select a network and a scan file.");
    return;
  }

  try {
    const text = await file.text();
    const data = JSON.parse(text);
    const uid = currentUid();

    const devices = Array.isArray(data) ? data : data.devices || [];

    let count = 0;
    for (const dev of devices) {
      const mac = (dev.mac || dev.macAddress || "").toUpperCase().replace(/[^A-F0-9]/g, "");
      if (!mac) continue;

      await setDoc(doc(db, "users", uid, "devices", mac), {
        mac: dev.mac || dev.macAddress || "",
        ip: dev.ip || dev.ipAddress || "",
        hostname: dev.hostname || dev.name || "",
        name: dev.name || dev.hostname || "",
        vendor: dev.vendor || dev.manufacturer || "",
        deviceType: dev.deviceType || dev.type || "Unknown",
        trust: dev.trust || "Unknown",
        ports: dev.ports || [],
        networkId: networkId,
        lastSeen: serverTimestamp(),
        firstSeen: dev.firstSeen || serverTimestamp(),
        sightings: dev.sightings || 1,
        classificationReason: dev.classificationReason || "",
        uploadedAt: serverTimestamp()
      });
      count++;
    }

    // Update device count on network
    const netRef = doc(db, "users", uid, "networks", networkId);
    const netSnap = await getDoc(netRef);
    if (netSnap.exists()) {
      await setDoc(netRef, { deviceCount: count }, { merge: true });
    }

    alert(`Uploaded ${count} devices successfully.`);
    uploadModal.classList.add("hidden");
    uploadForm.reset();

    // Reload
    if (networkFilter.value === networkId || networkFilter.value === "all") {
      networkFilter.dispatchEvent(new Event("change"));
    }
    loadOverviewData(uid);
  } catch (err) {
    alert("Error parsing scan file: " + err.message);
  }
});

// ========== DESKTOP SYNC ==========
const testConnectionBtn = document.getElementById("test-connection-btn");
const syncDevicesBtn = document.getElementById("sync-devices-btn");
const connectionStatus = document.getElementById("connection-status");

testConnectionBtn.addEventListener("click", async () => {
  const endpoint = document.getElementById("setting-api-endpoint").value.trim();
  const apiKey = document.getElementById("setting-api-key").value.trim();

  if (!endpoint) {
    showConnectionStatus("Enter an API endpoint first.", "error");
    return;
  }

  try {
    const headers = {};
    if (apiKey) headers["X-API-Key"] = apiKey;

    const res = await fetch(`${endpoint}/api/health`, { headers, signal: AbortSignal.timeout(5000) });
    if (res.ok) {
      showConnectionStatus("Connection successful. Desktop app is reachable.", "ok");
      // Save to user settings
      const uid = currentUid();
      if (uid) {
        await setDoc(doc(db, "users", uid), { apiEndpoint: endpoint }, { merge: true });
      }
    } else {
      showConnectionStatus(`Connection failed: HTTP ${res.status}`, "error");
    }
  } catch (err) {
    showConnectionStatus(`Connection failed: ${err.message}`, "error");
  }
});

syncDevicesBtn.addEventListener("click", async () => {
  const endpoint = document.getElementById("setting-api-endpoint").value.trim();
  const apiKey = document.getElementById("setting-api-key").value.trim();

  if (!endpoint) {
    showConnectionStatus("Enter an API endpoint first.", "error");
    return;
  }

  try {
    const headers = {};
    if (apiKey) headers["X-API-Key"] = apiKey;

    const res = await fetch(`${endpoint}/api/devices`, { headers, signal: AbortSignal.timeout(10000) });
    if (!res.ok) {
      showConnectionStatus(`Sync failed: HTTP ${res.status}`, "error");
      return;
    }

    const data = await res.json();
    const devices = Array.isArray(data) ? data : data.devices || [];
    const uid = currentUid();

    let count = 0;
    for (const dev of devices) {
      const mac = (dev.mac || "").toUpperCase().replace(/[^A-F0-9]/g, "");
      if (!mac) continue;

      // Find which network to associate
      let networkId = "";
      if (dev.subnet && allNetworksCache.length > 0) {
        const match = allNetworksCache.find((n) => n.subnet === dev.subnet);
        if (match) networkId = match.id;
      }
      if (!networkId && allNetworksCache.length > 0) {
        networkId = allNetworksCache[0].id;
      }

      await setDoc(doc(db, "users", uid, "devices", mac), {
        mac: dev.mac || "",
        ip: dev.ip || "",
        hostname: dev.hostname || "",
        name: dev.name || dev.hostname || "",
        vendor: dev.vendor || "",
        deviceType: dev.deviceType || dev.type || "Unknown",
        trust: dev.trust || "Unknown",
        ports: dev.ports || [],
        networkId: networkId,
        lastSeen: serverTimestamp(),
        firstSeen: dev.firstSeen || serverTimestamp(),
        sightings: dev.sightings || 1,
        classificationReason: dev.classificationReason || "",
        subnet: dev.subnet || "",
        syncedAt: serverTimestamp()
      }, { merge: true });
      count++;
    }

    showConnectionStatus(`Synced ${count} devices from desktop app.`, "ok");
    loadOverviewData(uid);
  } catch (err) {
    showConnectionStatus(`Sync failed: ${err.message}`, "error");
  }
});

function showConnectionStatus(msg, type) {
  connectionStatus.textContent = msg;
  connectionStatus.className = `connection-status status-${type}`;
  connectionStatus.classList.remove("hidden");
}

// ========== EXPORT DATA ==========
document.getElementById("export-data-btn").addEventListener("click", async () => {
  const uid = currentUid();
  if (!uid) return;

  const devSnap = await getDocs(collection(db, "users", uid, "devices"));
  const netSnap = await getDocs(collection(db, "users", uid, "networks"));

  const exportData = {
    exportedAt: new Date().toISOString(),
    networks: [],
    devices: []
  };

  netSnap.forEach((d) => exportData.networks.push({ id: d.id, ...d.data() }));
  devSnap.forEach((d) => exportData.devices.push({ id: d.id, ...d.data() }));

  const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = `transparency-export-${new Date().toISOString().slice(0, 10)}.json`;
  a.click();
  URL.revokeObjectURL(url);
});

// ========== ALERTS ==========
function loadAlerts(uid) {
  if (unsubAlerts) unsubAlerts();

  unsubAlerts = onSnapshot(
    query(collection(db, "users", uid, "alerts"), orderBy("timestamp", "desc"), limit(50)),
    (snap) => {
      const alertBadge = document.getElementById("alert-badge");

      if (snap.empty) {
        alertFeed.innerHTML = '<div class="empty-state">No alerts yet.</div>';
        overviewAlerts.innerHTML = '<div class="empty-state-sm">No recent alerts</div>';
        alertBadge.classList.add("hidden");
        kpiAlerts.textContent = "0";
        return;
      }

      alertFeed.innerHTML = "";
      const alertItems = [];
      let alertCount = 0;

      snap.forEach((d) => {
        const a = d.data();
        alertCount++;
        const html = buildAlertHtml(a);
        alertFeed.innerHTML += html;
        if (alertItems.length < 5) alertItems.push(html);
      });

      // Overview alerts (top 5)
      overviewAlerts.innerHTML = alertItems.join("");

      // Badge
      kpiAlerts.textContent = alertCount;
      if (alertCount > 0) {
        alertBadge.textContent = alertCount;
        alertBadge.classList.remove("hidden");
      }
    }
  );
}

function buildAlertHtml(a) {
  return `
    <div class="alert-item alert-${a.severity || "info"}">
      <div class="alert-header">
        <span class="alert-type">${escHtml(a.type || "alert")}</span>
        <span class="alert-time">${a.timestamp ? timeAgo(toDate(a.timestamp)) : ""}</span>
      </div>
      <div class="alert-message">${escHtml(a.message || "")}</div>
    </div>
  `;
}

// ========== AUTH SETTINGS DISPLAY ==========
import { auth as fbAuth } from "./firebase-config.js";
import { onAuthStateChanged } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-auth.js";

onAuthStateChanged(fbAuth, (user) => {
  window._currentUser = user;
  if (user) {
    document.getElementById("settings-email").textContent = user.email || "—";
    document.getElementById("settings-name").textContent = user.displayName || "—";
    document.getElementById("user-display-name").textContent = user.displayName || user.email;
    document.getElementById("user-email").textContent = user.email || "";
    const avatar = document.getElementById("user-avatar");
    avatar.textContent = (user.displayName || user.email || "U").charAt(0).toUpperCase();
  }
});

// ========== HELPERS ==========
function currentUid() {
  return window._currentUser?.uid;
}

function escHtml(str) {
  const div = document.createElement("div");
  div.textContent = str;
  return div.innerHTML;
}

function toDate(val) {
  if (!val) return new Date();
  if (val.toDate) return val.toDate();
  return new Date(val);
}

function timeAgo(date) {
  const seconds = Math.floor((new Date() - date) / 1000);
  if (seconds < 60) return "just now";
  if (seconds < 3600) return Math.floor(seconds / 60) + "m ago";
  if (seconds < 86400) return Math.floor(seconds / 3600) + "h ago";
  return Math.floor(seconds / 86400) + "d ago";
}

function formatDate(date) {
  return date.toLocaleDateString("en-US", {
    month: "short",
    day: "numeric",
    year: "numeric",
    hour: "2-digit",
    minute: "2-digit"
  });
}
