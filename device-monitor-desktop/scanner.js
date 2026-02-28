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

// ── Per-port business impact descriptions ─────────────────────────────────────
const PORT_IMPACT = {
  23:   'Plaintext credential exposure can compromise the entire network. Credentials captured via Telnet are reusable against other systems, enabling lateral movement and full domain takeover.',
  135:  'MS-RPC exploitation enables remote code execution. Historic worms (Blaster, Sasser) spread across corporate networks in hours via this single port — zero user interaction required.',
  139:  'NetBIOS exposure enables credential harvesting, share enumeration, and lateral movement. Combined with pass-the-hash attacks, a single exposed device can lead to full domain compromise.',
  445:  'SMB exploits like EternalBlue (MS17-010) allow wormable remote code execution and ransomware self-propagation. WannaCry infected 200,000+ systems in 150 countries through this port alone.',
  3389: 'Direct RDP exposure is the primary ransomware entry vector. A single compromised account grants full interactive control — including the ability to disable endpoint protection and exfiltrate data at will.',
  5900: 'VNC exposure allows complete graphical takeover of the desktop. Weak passwords are trivially brute-forced; several VNC versions have had authentication-bypass CVEs with public exploits.',
};

// ── Per-port runbook / reference links ───────────────────────────────────────
const PORT_RUNBOOKS = {
  23: [
    { label: 'CISA: Eliminate Telnet', url: 'https://www.cisa.gov/uscert/ncas/tips/ST05-006' },
    { label: 'CIS Control 4: Secure Config', url: 'https://www.cisecurity.org/controls/secure-configuration-of-enterprise-assets-and-software/' },
  ],
  135: [
    { label: 'Microsoft: Securing MS-RPC', url: 'https://learn.microsoft.com/en-us/windows/win32/rpc/security' },
    { label: 'CISA: Windows Hardening', url: 'https://www.cisa.gov/resources-tools/services/cyber-hygiene-services' },
  ],
  139: [
    { label: 'Microsoft: Disable NetBIOS over TCP/IP', url: 'https://learn.microsoft.com/en-us/troubleshoot/windows-server/networking/disable-netbios-tcp-ip-using-dhcp' },
    { label: 'NSA: Network Infrastructure Security', url: 'https://media.defense.gov/2022/Jun/15/2003018261/-1/-1/0/CTR_NSA_NETWORK_INFRASTRUCTURE_SECURITY_GUIDE_20220615.PDF' },
  ],
  445: [
    { label: 'Microsoft Security Bulletin MS17-010', url: 'https://learn.microsoft.com/en-us/security-updates/securitybulletins/2017/ms17-010' },
    { label: 'CISA Alert: SMB Security (AA20-302A)', url: 'https://www.cisa.gov/news-events/cybersecurity-advisories/aa20-302a' },
    { label: 'CIS: Disable SMBv1', url: 'https://www.cisecurity.org/benchmark/microsoft_windows_10' },
  ],
  3389: [
    { label: 'CISA: Reducing RDP Exposure', url: 'https://www.cisa.gov/news-events/alerts/2018/10/19/alert-ta18-074a' },
    { label: 'Microsoft: Secure RDP Deployment', url: 'https://learn.microsoft.com/en-us/windows-server/remote/remote-desktop-services/rds-secure-deployment' },
    { label: 'NSA: Hardening Remote Access', url: 'https://media.defense.gov/2021/Aug/25/2002873168/-1/-1/0/CTR_NETWORK_INFRASTRUCTURE_SECURITY_GUIDANCE_20210825.PDF' },
  ],
  5900: [
    { label: 'NIST SP 800-46: Remote Access Security', url: 'https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-46r2.pdf' },
    { label: 'RealVNC: Security Hardening Guide', url: 'https://help.realvnc.com/hc/en-us/categories/360001543791-Security' },
  ],
};

// ── Generic runbook links (non-port anomalies) ────────────────────────────────
const ANOMALY_RUNBOOKS = {
  'Unknown MAC': [
    { label: 'CIS Control 1: Device Inventory', url: 'https://www.cisecurity.org/controls/inventory-and-control-of-enterprise-assets/' },
    { label: 'NIST: Asset Management', url: 'https://www.nist.gov/cyberframework' },
  ],
  'New Device': [
    { label: 'CIS Control 1: Authorized Device Policy', url: 'https://www.cisecurity.org/controls/inventory-and-control-of-enterprise-assets/' },
    { label: 'CISA: Network Segmentation', url: 'https://www.cisa.gov/resources-tools/resources/cisa-network-segmentation' },
  ],
  'Ports Changed': [
    { label: 'CIS Control 4: Configuration Management', url: 'https://www.cisecurity.org/controls/secure-configuration-of-enterprise-assets-and-software/' },
    { label: 'NIST: Configuration Baseline', url: 'https://csrc.nist.gov/publications/detail/sp/800-128/final' },
  ],
};

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
    const impacts = ports.map(p => PORT_IMPACT[p]).filter(Boolean);
    const runbookLinks = [...new Set(
      ports.flatMap(p => PORT_RUNBOOKS[p] || [])
        .map(r => JSON.stringify(r))
    )].map(s => JSON.parse(s));

    const explanation = details.length === 1 ? details[0] :
      details.length > 1 ? {
        what: details.map(d => d.what).join(' | '),
        risk: details.map(d => d.risk).join(' '),
        steps: [...new Set(details.flatMap(d => d.steps))]
      } : {
        what: `Unusual service(s) on port(s): ${ports.join(', ')}`,
        risk: 'One or more services may be exploitable for unauthorized access.',
        steps: ['Identify the service with: ss -tlnp', 'Disable if not required', 'Add firewall rules to restrict access']
      };

    return {
      ...explanation,
      impact: impacts[0] || 'This service may be exploitable for unauthorized remote access.',
      runbookLinks,
      category: 'exposure',
    };
  }

  if (type === 'Unknown MAC') return {
    what: 'The device\'s hardware (MAC) address could not be resolved via the ARP table.',
    risk: 'An unresolvable MAC may indicate a device using MAC spoofing to evade identification, a new unauthorized guest device, or a stale ARP cache entry.',
    impact: 'Unresolvable device identity prevents accurate inventory tracking. Without confirmed MAC addresses, MAC-based access controls and device spoofing detection are ineffective.',
    steps: [
      'Check your router\'s DHCP client list to identify the device by lease time',
      'Run manually: arp -n <ip> to attempt resolution after the device sends traffic',
      'If the device is unrecognized, isolate it by blocking its IP at the router',
      'Enable MAC-address filtering on your router for additional access control'
    ],
    runbookLinks: ANOMALY_RUNBOOKS['Unknown MAC'],
    category: 'identity',
  };

  if (type === 'New Device') return {
    what: 'A device appeared on the network that was not present in the previous scan.',
    risk: 'New devices could represent unauthorized access (e.g., neighbor on your Wi-Fi), a new IoT device with insecure defaults, or a guest connecting without authorization.',
    impact: 'An unverified device has full network access. If unauthorized, it can exfiltrate data, intercept traffic via ARP spoofing, or act as a pivot point for further attacks.',
    steps: [
      'Log in to your router admin panel and review the DHCP lease table',
      'Check the device\'s open ports with the Enrich feature to assess risk',
      'If unrecognized, block its MAC at the router and change your Wi-Fi password',
      'Enable WPA3 encryption and disable WPS on your access point'
    ],
    runbookLinks: ANOMALY_RUNBOOKS['New Device'],
    category: 'inventory',
  };

  if (type === 'Ports Changed') return {
    what: 'The open port configuration on this device changed since the last scan.',
    risk: 'Newly opened ports may indicate a newly installed service (possibly malicious), a backdoor being opened, or a misconfiguration that introduced a new attack surface.',
    impact: 'The attack surface of this device changed unexpectedly. New open ports may expose vulnerable services; unexpected closures may indicate a service was disabled after exfiltration.',
    steps: [
      'Identify what opened with: ss -tlnp (Linux) or netstat -ano (Windows)',
      'If the change was unintentional, run a full malware scan on the device',
      'Review recent software installations and scheduled task changes',
      'Check firewall logs for unusual outbound connection attempts'
    ],
    runbookLinks: ANOMALY_RUNBOOKS['Ports Changed'],
    category: 'drift',
  };

  return {
    what: type,
    risk: 'Review this anomaly carefully.',
    impact: 'Potential security risk — manual investigation required.',
    steps: [],
    runbookLinks: [],
    category: 'unknown',
  };
}

// ── Anomaly rules engine ──────────────────────────────────────────────────────
function analyzeAnomalies(devices, previousSnapshot = []) {
  const anomalies = [];
  const prevMap   = new Map(previousSnapshot.map(d => [d.ip, d]));

  for (const dev of devices) {
    const prev = prevMap.get(dev.ip);

    // Rule 1: Risky ports
    const risky = dev.ports.filter(p => RISKY_PORTS.has(p));
    if (risky.length > 0) {
      // Compute drift type based on previous scan
      let driftType = 'risk_detected';
      if (prev) {
        const prevRisky = new Set(prev.ports.filter(p => RISKY_PORTS.has(p)));
        const newlyRisky = risky.filter(p => !prevRisky.has(p));
        if (newlyRisky.length > 0) {
          driftType = 'risk_increased';
        } else {
          driftType = 'persistent_risk';
        }
      }

      anomalies.push({
        id: `risky-${dev.ip}-${risky.join('-')}`,
        type: 'Risky Port',
        severity: 'High',
        device: dev.ip,
        description: `Dangerous service(s): ${risky.map(p => `${PORT_NAMES[p]||p} (${p})`).join(', ')}`,
        ports: risky,
        driftType,
        driftDetails: prev
          ? { previousPorts: prev.ports, currentPorts: dev.ports }
          : null,
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
        driftType: 'identity_gap',
        driftDetails: null,
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
        driftType: 'new_device',
        driftDetails: { firstSeen: new Date().toISOString() },
        ...getAnomalyExplanation('New Device')
      });
    }

    // Rule 4: Port list changed since last snapshot
    if (prev) {
      const prevSet = new Set(prev.ports);
      const currSet = new Set(dev.ports);
      const added   = dev.ports.filter(p => !prevSet.has(p));
      const removed = prev.ports.filter(p => !currSet.has(p));
      if (added.length || removed.length) {
        const changes = [];
        if (added.length)   changes.push(`+${added.map(p=>PORT_NAMES[p]||p).join(', ')}`);
        if (removed.length) changes.push(`-${removed.map(p=>PORT_NAMES[p]||p).join(', ')}`);

        const hasNewRisky = added.some(p => RISKY_PORTS.has(p));
        anomalies.push({
          id: `ports-${dev.ip}`,
          type: 'Ports Changed',
          severity: 'Medium',
          device: dev.ip,
          description: `Port profile changed since last scan: ${changes.join(' | ')}`,
          ports: added,
          driftType: hasNewRisky ? 'risk_increased' : 'ports_changed',
          driftDetails: { added, removed, previousPorts: prev.ports, currentPorts: dev.ports },
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
