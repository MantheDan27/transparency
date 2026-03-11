import { db } from "./firebase-config.js";
import {
  collection, doc, addDoc, setDoc, getDoc, getDocs, deleteDoc,
  query, orderBy, limit, serverTimestamp, onSnapshot
} from "https://www.gstatic.com/firebasejs/11.4.0/firebase-firestore.js";

// --- Network Management ---
const networkList = document.getElementById("network-list");
const addNetworkBtn = document.getElementById("add-network-btn");
const networkModal = document.getElementById("network-modal");
const networkForm = document.getElementById("network-form");
const closeModalBtn = document.getElementById("close-modal");

// --- Device Table ---
const deviceTableBody = document.getElementById("device-table-body");
const deviceCount = document.getElementById("device-count");
const networkFilter = document.getElementById("network-filter");

// --- Scan Upload ---
const uploadScanBtn = document.getElementById("upload-scan-btn");
const uploadModal = document.getElementById("upload-modal");
const uploadForm = document.getElementById("upload-form");
const closeUploadModal = document.getElementById("close-upload-modal");

// --- Alert Feed ---
const alertFeed = document.getElementById("alert-feed");

let activeNetworkId = null;
let unsubDevices = null;
let unsubAlerts = null;

window.loadDashboard = async function (user) {
  loadNetworks(user.uid);
  loadAlerts(user.uid);
};

// ========== NETWORKS ==========
async function loadNetworks(uid) {
  const snap = await getDocs(collection(db, "users", uid, "networks"));
  networkList.innerHTML = "";
  networkFilter.innerHTML = '<option value="all">All Networks</option>';

  if (snap.empty) {
    networkList.innerHTML = '<div class="empty-state">No networks yet. Add one to get started.</div>';
    return;
  }

  snap.forEach((d) => {
    const net = d.data();
    addNetworkCard(d.id, net);
    const opt = document.createElement("option");
    opt.value = d.id;
    opt.textContent = net.name;
    networkFilter.appendChild(opt);
  });
}

function addNetworkCard(id, net) {
  const card = document.createElement("div");
  card.className = "network-card";
  card.dataset.id = id;
  card.innerHTML = `
    <div class="network-card-header">
      <h3>${escHtml(net.name)}</h3>
      <span class="network-subnet">${escHtml(net.subnet || "")}</span>
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
    document.getElementById("tab-devices").click();
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

async function loadAllDevices(uid) {
  if (unsubDevices) unsubDevices();

  deviceTableBody.innerHTML = '<tr><td colspan="7" class="loading">Loading devices...</td></tr>';

  const netSnap = await getDocs(collection(db, "users", uid, "networks"));
  const allDevices = [];

  for (const netDoc of netSnap.docs) {
    const devSnap = await getDocs(
      query(collection(db, "users", uid, "devices"), orderBy("lastSeen", "desc"))
    );
    devSnap.forEach((d) => {
      const dev = d.data();
      if (!allDevices.find((x) => x.mac === dev.mac)) {
        allDevices.push({ id: d.id, ...dev, networkName: netDoc.data().name });
      }
    });
  }

  renderDeviceTable(allDevices);
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

  deviceTableBody.innerHTML = devices.map((d) => `
    <tr class="device-row" data-trust="${d.trust || 'unknown'}">
      <td><span class="trust-dot trust-${(d.trust || 'unknown').toLowerCase()}"></span>${escHtml(d.name || d.hostname || "Unknown")}</td>
      <td class="mono">${escHtml(d.mac || "")}</td>
      <td class="mono">${escHtml(d.ip || "")}</td>
      <td>${escHtml(d.vendor || "Unknown")}</td>
      <td>${escHtml(d.deviceType || "Unknown")}</td>
      <td><span class="trust-badge trust-${(d.trust || 'unknown').toLowerCase()}">${escHtml(d.trust || "Unknown")}</span></td>
      <td>${d.lastSeen ? timeAgo(d.lastSeen.toDate ? d.lastSeen.toDate() : new Date(d.lastSeen)) : "N/A"}</td>
    </tr>
  `).join("");
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

  if (!file) return;

  try {
    const text = await file.text();
    const data = JSON.parse(text);
    const uid = currentUid();

    // Accept either an array of devices or { devices: [...] }
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

    // Reload devices
    if (networkFilter.value === networkId || networkFilter.value === "all") {
      networkFilter.dispatchEvent(new Event("change"));
    }
  } catch (err) {
    alert("Error parsing scan file: " + err.message);
  }
});

// ========== ALERTS ==========
function loadAlerts(uid) {
  if (unsubAlerts) unsubAlerts();

  unsubAlerts = onSnapshot(
    query(collection(db, "users", uid, "alerts"), orderBy("timestamp", "desc"), limit(50)),
    (snap) => {
      if (snap.empty) {
        alertFeed.innerHTML = '<div class="empty-state">No alerts yet.</div>';
        return;
      }
      alertFeed.innerHTML = "";
      snap.forEach((d) => {
        const a = d.data();
        const el = document.createElement("div");
        el.className = `alert-item alert-${a.severity || "info"}`;
        el.innerHTML = `
          <div class="alert-header">
            <span class="alert-type">${escHtml(a.type || "alert")}</span>
            <span class="alert-time">${a.timestamp ? timeAgo(a.timestamp.toDate ? a.timestamp.toDate() : new Date(a.timestamp)) : ""}</span>
          </div>
          <div class="alert-message">${escHtml(a.message || "")}</div>
        `;
        alertFeed.appendChild(el);
      });
    }
  );
}

// ========== TAB NAVIGATION ==========
document.querySelectorAll(".tab-btn").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".tab-btn").forEach((b) => b.classList.remove("active"));
    document.querySelectorAll(".tab-panel").forEach((p) => p.classList.add("hidden"));
    btn.classList.add("active");
    document.getElementById(`panel-${btn.dataset.tab}`).classList.remove("hidden");
  });
});

// ========== HELPERS ==========
function currentUid() {
  const { auth } = window._fbAuth || {};
  // Use the imported auth from firebase-config
  return window._currentUser?.uid;
}

// Patch: expose uid via auth state
import { auth as fbAuth } from "./firebase-config.js";
import { onAuthStateChanged } from "https://www.gstatic.com/firebasejs/11.4.0/firebase-auth.js";

onAuthStateChanged(fbAuth, (user) => {
  window._currentUser = user;
});

function escHtml(str) {
  const div = document.createElement("div");
  div.textContent = str;
  return div.innerHTML;
}

function timeAgo(date) {
  const seconds = Math.floor((new Date() - date) / 1000);
  if (seconds < 60) return "just now";
  if (seconds < 3600) return Math.floor(seconds / 60) + "m ago";
  if (seconds < 86400) return Math.floor(seconds / 3600) + "h ago";
  return Math.floor(seconds / 86400) + "d ago";
}
