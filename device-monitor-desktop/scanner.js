'use strict';

const os   = require('os');
const net  = require('net');
const dns  = require('dns');
const { exec } = require('child_process');
const util = require('util');

const execPromise = util.promisify(exec);
const dnsReverse  = util.promisify(dns.reverse);

// ── Port catalogue ────────────────────────────────────────────────────────────
const SCAN_PORTS = [21, 22, 23, 25, 53, 80, 110, 135, 139, 443, 445, 3306, 3389, 5900, 8080, 8443];
const RISKY_PORTS = new Set([23, 135, 139, 445, 3389, 5900]);
const PORT_NAMES  = {
  21:'FTP', 22:'SSH', 23:'Telnet', 25:'SMTP', 53:'DNS',
  80:'HTTP', 110:'POP3', 135:'MS-RPC', 139:'NetBIOS',
  443:'HTTPS', 445:'SMB', 3306:'MySQL', 3389:'RDP',
  5900:'VNC', 8080:'HTTP-Alt', 8443:'HTTPS-Alt'
};

// ── Network helpers ───────────────────────────────────────────────────────────
function getLocalNetwork() {
  for (const iface of Object.values(os.networkInterfaces()).flat()) {
    if (iface.family === 'IPv4' && !iface.internal) {
      const parts = iface.address.split('.');
      return { baseIp: parts.slice(0,3).join('.'), localIp: iface.address };
    }
  }
  return { baseIp: '192.168.1', localIp: '192.168.1.100' };
}

async function pingHost(ip) {
  const cmd = process.platform === 'win32'
    ? `ping -n 1 -w 600 ${ip}`
    : `ping -c 1 -W 1 ${ip}`;
  try {
    await execPromise(cmd, { timeout: 3000 });
    return true;
  } catch {
    // TCP fallback: check a handful of common ports
    const checks = await Promise.all([
      probePort(ip, 80, 400), probePort(ip, 443, 400),
      probePort(ip, 22, 400),  probePort(ip, 8080, 400)
    ]);
    return checks.some(Boolean);
  }
}

async function getMac(ip) {
  try {
    if (process.platform === 'linux') {
      const { stdout } = await execPromise('cat /proc/net/arp', { timeout: 2000 });
      for (const line of stdout.split('\n').slice(1)) {
        const p = line.trim().split(/\s+/);
        if (p[0] === ip && p[3] && p[3] !== '00:00:00:00:00:00') return p[3].toUpperCase();
      }
    } else if (process.platform === 'darwin') {
      const { stdout } = await execPromise(`arp -n ${ip}`, { timeout: 2000 });
      const m = stdout.match(/([0-9a-f]{1,2}(?::[0-9a-f]{1,2}){5})/i);
      if (m) return m[1].toUpperCase();
    } else if (process.platform === 'win32') {
      const { stdout } = await execPromise(`arp -a ${ip}`, { timeout: 2000 });
      const m = stdout.match(/([0-9a-f]{2}(?:-[0-9a-f]{2}){5})/i);
      if (m) return m[1].replace(/-/g, ':').toUpperCase();
    }
  } catch { /* ignore */ }
  return 'Unknown';
}

async function probePort(ip, port, timeout = 600) {
  return new Promise(resolve => {
    const sock = new net.Socket();
    let done = false;
    const finish = (v) => { if (!done) { done = true; sock.destroy(); resolve(v); } };
    sock.setTimeout(timeout);
    sock.on('connect', () => finish(true));
    sock.on('timeout', () => finish(false));
    sock.on('error',   () => finish(false));
    sock.connect(port, ip);
  });
}

async function scanPorts(ip) {
  const hits = await Promise.all(
    SCAN_PORTS.map(p => probePort(ip, p).then(open => open ? p : null))
  );
  return hits.filter(Boolean);
}

async function resolveHostname(ip) {
  try { return (await dnsReverse(ip))[0] || null; }
  catch { return null; }
}

// ── Anomaly explanation library ───────────────────────────────────────────────
const PORT_EXPLAIN = {
  23: {
    what: 'Telnet service actively listening on port 23.',
    risk: 'Telnet transmits ALL data — including usernames and passwords — in unencrypted plaintext. Anyone on the same network can intercept credentials with a basic packet sniffer.',
    steps: [
      'Disable the Telnet daemon immediately (service telnet stop / systemctl disable telnet)',
      'Install and configure OpenSSH as a secure replacement (port 22)',
      'Update firewall rules to block inbound port 23 from all sources',
      'Rotate any credentials that may have been exposed over Telnet'
    ]
  },
  135: {
    what: 'Microsoft Remote Procedure Call (MS-RPC) service on port 135.',
    risk: 'MS-RPC has been exploited by major network worms including Blaster and Sasser. It should never be reachable from outside a trusted segment.',
    steps: [
      'Block port 135 at the perimeter firewall for all external traffic',
      'Apply all available Windows security patches (particularly MS03-026)',
      'Use Windows Firewall to restrict RPC to trusted host ranges only'
    ]
  },
  139: {
    what: 'NetBIOS Session Service on port 139.',
    risk: 'NetBIOS leaks system details (OS, usernames, shares) and has been a reliable target for credential theft and lateral movement inside networks.',
    steps: [
      'Disable NetBIOS over TCP/IP in Network Adapter → Advanced → WINS settings',
      'Block ports 137–139 at the network perimeter firewall',
      'Migrate file sharing to SMBv2/v3 or a secure SFTP-based solution'
    ]
  },
  445: {
    what: 'SMB (Server Message Block / Windows File Sharing) on port 445.',
    risk: 'Unpatched SMB was the attack vector for WannaCry and NotPetya ransomware outbreaks. Exposed SMB allows remote code execution, ransomware deployment, and full system takeover.',
    steps: [
      'Apply MS17-010 patch immediately (check: Get-HotFix -Id KB4012212)',
      'Disable SMBv1 via PowerShell: Set-SmbServerConfiguration -EnableSMB1Protocol $false',
      'Block port 445 at the firewall unless local file sharing is absolutely required',
      'Restrict SMB access to explicitly trusted IP ranges only'
    ]
  },
  3389: {
    what: 'Remote Desktop Protocol (RDP) service on port 3389.',
    risk: 'RDP exposed directly to a network is the #1 ransomware entry point. Automated credential-stuffing bots scan for open RDP ports 24 hours a day. A single weak password means full system compromise.',
    steps: [
      'Place RDP behind a VPN gateway — never expose it directly',
      'Enable Network Level Authentication (NLA) in System → Remote Settings',
      'Use a strong unique password of 20+ characters for all remote-capable accounts',
      'Enable account lockout after 3–5 failed attempts via Group Policy',
      'Consider restricting access by IP allowlist'
    ]
  },
  5900: {
    what: 'VNC (Virtual Network Computing) remote desktop service on port 5900.',
    risk: 'Many VNC implementations have weak or absent authentication. Several versions have had authentication-bypass vulnerabilities allowing full desktop takeover with no credentials.',
    steps: [
      'Tunnel VNC through SSH instead of direct exposure: ssh -L 5901:localhost:5900',
      'Set a strong VNC password and enable available encryption options',
      'Block port 5900 at the firewall from all external sources',
      'Evaluate switching to a modern encrypted solution (RustDesk, Guacamole, or similar)'
    ]
  }
};

function getAnomalyExplanation(type, ports = []) {
  if (type === 'Risky Port') {
    const details = ports.map(p => PORT_EXPLAIN[p]).filter(Boolean);
    if (details.length === 1) return details[0];
    if (details.length > 1) return {
      what: details.map(d => d.what).join(' | '),
      risk: details.map(d => d.risk).join(' '),
      steps: [...new Set(details.flatMap(d => d.steps))]
    };
    return {
      what: `Unusual service(s) on port(s): ${ports.join(', ')}`,
      risk: 'One or more services may be exploitable for unauthorized access.',
      steps: ['Identify the service with: ss -tlnp', 'Disable if not required', 'Add firewall rules to restrict access']
    };
  }
  if (type === 'Unknown MAC') return {
    what: 'The device\'s hardware (MAC) address could not be resolved via the ARP table.',
    risk: 'An unresolvable MAC may indicate a device using MAC spoofing to evade identification, a new unauthorized guest device, or a stale ARP cache entry.',
    steps: [
      'Check your router\'s DHCP client list to identify the device by lease time',
      'Run manually: arp -n <ip> to attempt resolution after the device sends traffic',
      'If the device is unrecognized, isolate it by blocking its IP at the router',
      'Enable MAC-address filtering on your router for additional access control'
    ]
  };
  if (type === 'New Device') return {
    what: 'A device appeared on the network that was not present in the previous scan.',
    risk: 'New devices could represent unauthorized access (e.g., neighbor on your Wi-Fi), a new IoT device with insecure defaults, or a guest connecting without authorization.',
    steps: [
      'Log in to your router admin panel and review the DHCP lease table',
      'Check the device\'s open ports with the Enrich feature to assess risk',
      'If unrecognized, block its MAC at the router and change your Wi-Fi password',
      'Enable WPA3 encryption and disable WPS on your access point'
    ]
  };
  if (type === 'Ports Changed') return {
    what: 'The open port configuration on this device changed since the last scan.',
    risk: 'Newly opened ports may indicate a newly installed service (possibly malicious), a backdoor being opened, or a misconfiguration that introduced a new attack surface.',
    steps: [
      'Identify what opened with: ss -tlnp (Linux) or netstat -ano (Windows)',
      'If the change was unintentional, run a full malware scan on the device',
      'Review recent software installations and scheduled task changes',
      'Check firewall logs for unusual outbound connection attempts'
    ]
  };
  return { what: type, risk: 'Review this anomaly carefully.', steps: [] };
}

// ── Anomaly rules engine ──────────────────────────────────────────────────────
function analyzeAnomalies(devices, previousSnapshot = []) {
  const anomalies = [];
  const prevMap   = new Map(previousSnapshot.map(d => [d.ip, d]));

  for (const dev of devices) {
    // Rule 1: Risky ports
    const risky = dev.ports.filter(p => RISKY_PORTS.has(p));
    if (risky.length > 0) {
      anomalies.push({
        id: `risky-${dev.ip}-${risky.join('-')}`,
        type: 'Risky Port',
        severity: 'High',
        device: dev.ip,
        description: `Dangerous service(s): ${risky.map(p => `${PORT_NAMES[p]||p} (${p})`).join(', ')}`,
        ports: risky,
        ...getAnomalyExplanation('Risky Port', risky)
      });
    }

    // Rule 2: Unknown MAC
    if (!dev.mac || dev.mac === 'Unknown') {
      anomalies.push({
        id: `mac-${dev.ip}`,
        type: 'Unknown MAC',
        severity: 'Medium',
        device: dev.ip,
        description: 'MAC address could not be resolved via ARP.',
        ports: [],
        ...getAnomalyExplanation('Unknown MAC')
      });
    }

    // Rule 3: New device since last snapshot
    if (previousSnapshot.length > 0 && !prevMap.has(dev.ip)) {
      anomalies.push({
        id: `new-${dev.ip}`,
        type: 'New Device',
        severity: 'Low',
        device: dev.ip,
        description: 'Device not present in previous scan — appeared since last check.',
        ports: [],
        ...getAnomalyExplanation('New Device')
      });
    }

    // Rule 4: Port list changed since last snapshot
    const prev = prevMap.get(dev.ip);
    if (prev) {
      const prevSet = new Set(prev.ports);
      const currSet = new Set(dev.ports);
      const added   = dev.ports.filter(p => !prevSet.has(p));
      const removed = prev.ports.filter(p => !currSet.has(p));
      if (added.length || removed.length) {
        const changes = [];
        if (added.length)   changes.push(`+${added.map(p=>PORT_NAMES[p]||p).join(', ')}`);
        if (removed.length) changes.push(`-${removed.map(p=>PORT_NAMES[p]||p).join(', ')}`);
        anomalies.push({
          id: `ports-${dev.ip}`,
          type: 'Ports Changed',
          severity: 'Medium',
          device: dev.ip,
          description: `Port profile changed since last scan: ${changes.join(' | ')}`,
          ports: added,
          ...getAnomalyExplanation('Ports Changed')
        });
      }
    }
  }

  return anomalies;
}

// ── Main scan entry point ─────────────────────────────────────────────────────
async function scanNetwork(progressCallback) {
  const report = msg => progressCallback?.(msg);
  const { baseIp, localIp } = getLocalNetwork();

  report(`Detecting subnet ${baseIp}.0/24 — starting ping sweep...`);

  const ips   = Array.from({ length: 254 }, (_, i) => `${baseIp}.${i + 1}`);
  const alive = [];
  const BATCH = 25;

  for (let i = 0; i < ips.length; i += BATCH) {
    const hits = await Promise.all(
      ips.slice(i, i + BATCH).map(ip => pingHost(ip).then(up => up ? ip : null))
    );
    alive.push(...hits.filter(Boolean));
    report(`Ping sweep: ${Math.min(i + BATCH, 254)}/254 — ${alive.length} host(s) responding`);
  }

  // Always include this machine
  if (!alive.includes(localIp)) alive.push(localIp);

  report(`${alive.length} device(s) found. Probing ports and resolving identities...`);

  const devices = await Promise.all(alive.map(async ip => {
    const [ports, mac, hostname] = await Promise.all([
      scanPorts(ip),
      getMac(ip),
      resolveHostname(ip)
    ]);
    const name = hostname || (ip === localIp ? 'This Device' : `Device-${ip.split('.').pop()}`);
    return { ip, mac, name, ports };
  }));

  report(`Scan complete — ${devices.length} device(s) inventoried.`);
  return devices;
}

module.exports = { scanNetwork, analyzeAnomalies, RISKY_PORTS, PORT_NAMES };
