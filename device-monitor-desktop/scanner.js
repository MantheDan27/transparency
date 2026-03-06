'use strict';

const os    = require('os');
const net   = require('net');
const tls   = require('tls');
const dns   = require('dns');
const dgram = require('dgram');
const { exec } = require('child_process');
const util  = require('util');

const execPromise = util.promisify(exec);
const dnsReverse  = util.promisify(dns.reverse);

// ── Port catalogues ────────────────────────────────────────────────────────────
const PORT_PROFILES = {
  common:   [21, 22, 23, 25, 53, 80, 110, 135, 139, 443, 445, 902, 903, 2376, 3306, 3389, 5900, 5985, 8006, 8080, 8443, 16509],
  iot:      [80, 443, 1883, 1884, 5683, 7000, 7100, 8008, 8009, 8080, 8443, 8883, 9100, 9123],
  nas:      [21, 22, 80, 111, 139, 443, 445, 548, 873, 2049, 3260, 5000, 5001, 8080, 8443, 9091],
  security: [8080, 8443, 8888, 9000, 9090, 9443, 9999, 37777, 37778, 34567, 34599, 554],
  full:     Array.from({ length: 1024 }, (_, i) => i + 1),
};
const SCAN_PORTS = PORT_PROFILES.common;
const RISKY_PORTS = new Set([23, 135, 139, 445, 3389, 5900]);

// Ports commonly used by hypervisors and VM management
const VM_PORTS = new Set([
  902, 903,           // VMware ESXi/vCenter
  8006,               // Proxmox VE
  2376, 2377,         // Docker daemon / Swarm
  6443,               // Kubernetes API
  16509, 16514,       // libvirt (QEMU/KVM)
  5985, 5986,         // WinRM (Hyper-V management)
  8000, 8443,         // Various hypervisor web UIs
]);

// Known VM / hypervisor MAC OUI prefixes
const VM_MAC_PREFIXES = [
  '00:0C:29', '00:50:56', '00:05:69', '00:1C:14',  // VMware
  '08:00:27',                                         // VirtualBox
  '52:54:00',                                         // QEMU/KVM
  '00:16:3E',                                         // Xen
  '00:1C:42',                                         // Parallels
  '00:15:5D',                                         // Hyper-V
  '00:0F:4B',                                         // Oracle VM
  '02:42:AC', '02:42:00',                             // Docker
  '0A:58:0A',                                         // Kubernetes
];

const PORT_NAMES = {
  21: 'FTP', 22: 'SSH', 23: 'Telnet', 25: 'SMTP', 53: 'DNS',
  80: 'HTTP', 110: 'POP3', 111: 'RPC', 135: 'MS-RPC', 139: 'NetBIOS',
  443: 'HTTPS', 445: 'SMB', 548: 'AFP', 554: 'RTSP', 631: 'IPP',
  902: 'VMware', 903: 'VMware-Web',
  873: 'rsync', 1883: 'MQTT', 2049: 'NFS', 2376: 'Docker', 2377: 'Docker-Swarm', 3306: 'MySQL', 3260: 'iSCSI',
  3389: 'RDP', 5000: 'UPnP/Web', 5001: 'Web', 5683: 'CoAP', 5900: 'VNC', 5985: 'WinRM', 5986: 'WinRM-SSL',
  7000: 'AirPlay', 7100: 'AirPlay', 8008: 'Chromecast', 8009: 'Chromecast',
  6443: 'Kubernetes', 8006: 'Proxmox', 8080: 'HTTP-Alt', 8443: 'HTTPS-Alt', 8883: 'MQTT-SSL', 9000: 'Web',
  9090: 'Web', 9100: 'Printing', 9091: 'Web', 9123: 'Web', 9443: 'HTTPS-Alt',
  16509: 'libvirt', 16514: 'libvirt-TLS',
  34567: 'DVR', 37777: 'DVR', 37778: 'DVR',
};

// ── OUI Vendor Database (top 500+ vendors) ────────────────────────────────────
const OUI_VENDORS = {
  // Apple
  '00:03:93':'Apple','00:05:02':'Apple','00:0A:95':'Apple','00:0D:93':'Apple',
  '00:11:24':'Apple','00:14:51':'Apple','00:16:CB':'Apple','00:17:F2':'Apple',
  '00:19:E3':'Apple','00:1B:63':'Apple','00:1C:B3':'Apple','00:1E:52':'Apple',
  'AC:CF:5C':'Apple','A4:C3:61':'Apple','F8:1E:DF':'Apple','E8:8D:28':'Apple',
  'D0:81:7A':'Apple','CC:25:EF':'Apple','B8:F6:B1':'Apple','AC:BC:32':'Apple',
  '9C:F3:87':'Apple','8C:85:90':'Apple','84:78:8B':'Apple','78:67:D7':'Apple',
  '70:73:CB':'Apple','6C:96:CF':'Apple','60:69:44':'Apple','58:55:CA':'Apple',
  '50:EA:D6':'Apple','48:D7:05':'Apple','40:4D:7F':'Apple','38:66:F0':'Apple',
  '34:08:BC':'Apple','28:CF:E9':'Apple','20:78:F0':'Apple','18:65:90':'Apple',
  '10:93:E9':'Apple','08:66:98':'Apple','04:0C:CE':'Apple','00:26:BB':'Apple',
  'DC:2B:2A':'Apple','C8:2A:14':'Apple','B8:8D:12':'Apple','A8:96:8A':'Apple',
  'F4:F1:5A':'Apple','EC:35:86':'Apple','E0:AC:CB':'Apple','D0:03:4B':'Apple',
  'BC:92:6B':'Apple','98:01:A7':'Apple','90:72:40':'Apple','8C:7C:92':'Apple',
  // Samsung
  '00:07:AB':'Samsung','00:12:FB':'Samsung','00:15:B9':'Samsung','00:17:C9':'Samsung',
  '00:1A:8A':'Samsung','00:1C:43':'Samsung','F4:7B:5E':'Samsung','E8:50:8B':'Samsung',
  'D8:57:EF':'Samsung','CC:07:AB':'Samsung','B8:BB:AF':'Samsung','A8:06:00':'Samsung',
  '94:63:D1':'Samsung','90:18:7C':'Samsung','8C:71:F8':'Samsung','78:9E:D0':'Samsung',
  '68:EB:C5':'Samsung','5C:E8:EB':'Samsung','4C:66:41':'Samsung','40:0E:85':'Samsung',
  '34:AA:8B':'Samsung','30:07:4D':'Samsung','28:39:5E':'Samsung','24:FB:65':'Samsung',
  'FC:00:12':'Samsung','F0:BF:97':'Samsung','E8:F2:E2':'Samsung','DC:CF:96':'Samsung',
  // Google / Nest
  '00:1A:11':'Google','F4:F5:D8':'Google','D4:F5:47':'Google','54:60:09':'Google',
  '3C:5A:B4':'Google','20:DF:B9':'Google','A4:77:33':'Google','B0:19:C6':'Google',
  'F4:FF:E5':'Google','94:EB:2C':'Google','08:9E:08':'Google','48:D6:D5':'Google',
  '68:C4:4D':'Google','F8:8F:CA':'Google','E4:F0:42':'Google','C4:62:EA':'Google',
  // Raspberry Pi
  'B8:27:EB':'Raspberry Pi','DC:A6:32':'Raspberry Pi','E4:5F:01':'Raspberry Pi',
  '28:CD:C1':'Raspberry Pi','2C:CF:67':'Raspberry Pi','D8:3A:DD':'Raspberry Pi',
  // Amazon
  'FC:65:DE':'Amazon','74:C2:46':'Amazon','68:37:E9':'Amazon','44:65:0D':'Amazon',
  '34:D2:70':'Amazon','0C:47:C9':'Amazon','A0:02:DC':'Amazon','F0:27:2D':'Amazon',
  '84:D6:D0':'Amazon','40:B4:CD':'Amazon','FC:A6:67':'Amazon','50:DC:E7':'Amazon',
  'CC:9E:A2':'Amazon','B4:7C:9C':'Amazon','AC:63:BE':'Amazon','A4:08:F5':'Amazon',
  // Cisco
  '00:00:0C':'Cisco','00:01:42':'Cisco','00:01:43':'Cisco','00:03:6B':'Cisco',
  '00:0A:41':'Cisco','00:0B:46':'Cisco','00:0C:CE':'Cisco','00:0D:28':'Cisco',
  '00:0E:08':'Cisco','00:0F:24':'Cisco','00:10:07':'Cisco','00:11:93':'Cisco',
  '00:12:17':'Cisco','00:13:10':'Cisco','00:14:A9':'Cisco','00:15:2B':'Cisco',
  '00:16:47':'Cisco','00:17:0E':'Cisco','00:18:18':'Cisco','00:18:BA':'Cisco',
  '00:19:06':'Cisco','00:1A:2F':'Cisco','00:1B:53':'Cisco','00:1C:57':'Cisco',
  '00:1D:45':'Cisco','00:1E:13':'Cisco','00:1F:26':'Cisco','00:21:1B':'Cisco',
  '34:DB:FD':'Cisco','40:F4:EC':'Cisco','4C:00:82':'Cisco','50:06:04':'Cisco',
  '58:AC:78':'Cisco','64:D8:14':'Cisco','6C:41:6A':'Cisco','70:10:5C':'Cisco',
  '7C:AD:74':'Cisco','84:3D:C6':'Cisco','88:75:98':'Cisco','9C:57:AD':'Cisco',
  // Ubiquiti
  '00:15:6D':'Ubiquiti','00:27:22':'Ubiquiti','04:18:D6':'Ubiquiti','0C:83:CC':'Ubiquiti',
  '18:E8:29':'Ubiquiti','24:A4:3C':'Ubiquiti','44:D9:E7':'Ubiquiti','58:07:44':'Ubiquiti',
  '60:22:32':'Ubiquiti','68:72:51':'Ubiquiti','74:83:C2':'Ubiquiti','78:8A:20':'Ubiquiti',
  '80:2A:A8':'Ubiquiti','AC:8B:A9':'Ubiquiti','B4:FB:E4':'Ubiquiti','D4:21:22':'Ubiquiti',
  'DC:9F:DB':'Ubiquiti','E4:38:83':'Ubiquiti','F0:9F:C2':'Ubiquiti','FC:EC:DA':'Ubiquiti',
  // TP-Link
  '00:1D:0F':'TP-Link','08:95:2A':'TP-Link','10:BF:48':'TP-Link','14:CC:20':'TP-Link',
  '18:A6:F7':'TP-Link','1C:3B:F3':'TP-Link','24:DB:AC':'TP-Link','28:87:BA':'TP-Link',
  '30:B5:C2':'TP-Link','34:60:F9':'TP-Link','3C:46:D8':'TP-Link','44:94:FC':'TP-Link',
  '50:3E:AA':'TP-Link','54:C8:0F':'TP-Link','5C:89:9A':'TP-Link','60:32:B1':'TP-Link',
  '64:70:02':'TP-Link','6C:19:C0':'TP-Link','74:EA:3A':'TP-Link','78:44:FD':'TP-Link',
  '80:35:C1':'TP-Link','84:16:F9':'TP-Link','90:F6:52':'TP-Link','98:DA:C4':'TP-Link',
  'A4:2B:B0':'TP-Link','B0:48:7A':'TP-Link','B8:9B:C9':'TP-Link','C0:4A:00':'TP-Link',
  'C4:E9:84':'TP-Link','CC:32:E5':'TP-Link','D4:6E:0E':'TP-Link','D8:07:B6':'TP-Link',
  'E4:C3:2A':'TP-Link','E8:DE:27':'TP-Link','EC:08:6B':'TP-Link','F0:A7:31':'TP-Link',
  // Netgear
  '00:09:5B':'Netgear','00:0F:B5':'Netgear','00:14:6C':'Netgear','00:1B:2F':'Netgear',
  '00:1E:2A':'Netgear','00:22:3F':'Netgear','00:26:F2':'Netgear','1C:40:24':'Netgear',
  '20:0C:C8':'Netgear','28:C6:8E':'Netgear','2C:B0:5D':'Netgear','30:46:9A':'Netgear',
  '60:38:E0':'Netgear','6C:B0:CE':'Netgear','84:1B:5E':'Netgear','A0:21:B7':'Netgear',
  'C0:3F:0E':'Netgear','C4:04:15':'Netgear','D4:87:D8':'Netgear','E0:46:9A':'Netgear',
  // Dell
  '00:06:5B':'Dell','00:08:74':'Dell','00:0B:DB':'Dell','00:0D:56':'Dell',
  '00:0F:1F':'Dell','00:11:43':'Dell','00:12:3F':'Dell','00:13:72':'Dell',
  '00:14:22':'Dell','00:15:C5':'Dell','00:17:08':'Dell','00:18:8B':'Dell',
  '00:19:B9':'Dell','00:1A:A0':'Dell','00:1C:23':'Dell','00:1D:09':'Dell',
  '00:1E:4F':'Dell','00:21:70':'Dell','00:22:19':'Dell','00:23:AE':'Dell',
  '18:03:73':'Dell','18:66:DA':'Dell','24:B6:FD':'Dell','34:17:EB':'Dell',
  '44:A8:42':'Dell','50:9A:4C':'Dell','74:86:7A':'Dell','A4:1F:72':'Dell',
  'B0:83:FE':'Dell','B8:2A:72':'Dell','BC:30:5B':'Dell','D8:9E:F3':'Dell',
  // HP
  '00:01:E6':'HP','00:06:CC':'HP','00:08:02':'HP','00:0D:9D':'HP',
  '00:0E:7F':'HP','00:11:0A':'HP','00:12:79':'HP','00:13:21':'HP',
  '00:14:38':'HP','00:15:60':'HP','00:16:35':'HP','00:18:FE':'HP',
  '00:19:BB':'HP','00:1A:4B':'HP','00:1B:78':'HP','00:1C:C4':'HP',
  '3C:D9:2B':'HP','40:B0:34':'HP','58:20:B1':'HP','68:B5:99':'HP',
  '70:5A:0F':'HP','78:E7:D1':'HP','80:C1:6E':'HP','94:57:A5':'HP',
  // Microsoft
  '00:15:5D':'Microsoft','00:17:FA':'Microsoft','00:1D:D8':'Microsoft',
  '00:22:48':'Microsoft','00:25:AE':'Microsoft','28:18:78':'Microsoft',
  '3C:83:75':'Microsoft','48:50:73':'Microsoft','50:1A:C5':'Microsoft',
  '60:45:BD':'Microsoft','7C:1E:52':'Microsoft','98:5F:D3':'Microsoft',
  'A0:CE:C8':'Microsoft','DC:53:60':'Microsoft','E4:A7:C5':'Microsoft',
  // Intel
  '00:02:B3':'Intel','00:03:47':'Intel','00:07:E9':'Intel','00:0C:F1':'Intel',
  '00:0E:35':'Intel','00:11:11':'Intel','00:12:F0':'Intel','00:13:02':'Intel',
  '00:13:CE':'Intel','00:15:00':'Intel','00:16:6F':'Intel','00:18:DE':'Intel',
  '00:19:D1':'Intel','00:1B:21':'Intel','00:1C:BF':'Intel','00:1D:E0':'Intel',
  '40:25:C2':'Intel','44:1C:A8':'Intel','60:57:18':'Intel','64:5D:86':'Intel',
  '7C:7A:91':'Intel','8C:EC:4B':'Intel','90:E2:BA':'Intel','A4:C3:F0':'Intel',
  'B8:88:E3':'Intel','C8:60:00':'Intel','D4:BE:D9':'Intel','F4:4D:30':'Intel',
  // Sonos
  '00:0E:58':'Sonos','34:7E:5C':'Sonos','48:A6:B8':'Sonos','54:2A:1B':'Sonos',
  '5C:AA:FD':'Sonos','78:28:CA':'Sonos','94:9F:3E':'Sonos','B8:E9:37':'Sonos',
  // Philips / Signify (Hue)
  'EC:B5:FA':'Philips Hue','00:17:88':'Philips Hue',
  // ASUS
  '00:08:54':'ASUS','00:0C:6E':'ASUS','00:0E:A6':'ASUS','00:11:2F':'ASUS',
  '00:13:D4':'ASUS','00:15:F2':'ASUS','00:17:31':'ASUS','00:18:F3':'ASUS',
  '00:1A:92':'ASUS','00:1B:FC':'ASUS','00:1D:60':'ASUS','00:1E:8C':'ASUS',
  'A4:7E:46':'ASUS','B8:EE:65':'ASUS','E0:3F:49':'ASUS','F0:79:59':'ASUS',
  'AC:22:0B':'ASUS','14:DA:E9':'ASUS','2C:56:DC':'ASUS','50:46:5D':'ASUS',
  // Synology
  '00:11:32':'Synology','00:24:13':'Synology','90:09:D0':'Synology',
  // QNAP
  '00:08:9B':'QNAP','24:5E:BE':'QNAP','D8:4F:1E':'QNAP',
  // Roku
  'B8:3E:59':'Roku','CC:6D:A0':'Roku','D8:31:CF':'Roku','F8:A2:D6':'Roku',
  // D-Link
  '00:05:5D':'D-Link','00:0D:88':'D-Link','00:0F:3D':'D-Link','00:11:95':'D-Link',
  '00:13:46':'D-Link','00:15:E9':'D-Link','00:17:9A':'D-Link','00:19:5B':'D-Link',
  '1C:7E:E5':'D-Link','28:10:7B':'D-Link','5C:D9:98':'D-Link','90:94:E4':'D-Link',
  'C4:A8:1D':'D-Link','CC:B2:55':'D-Link',
  // VMware / Virtual
  '00:0C:29':'VMware','00:50:56':'VMware','00:05:69':'VMware',
  '00:1C:14':'VMware','00:1C:42':'Parallels','00:0F:4B':'Oracle VM',
  '52:54:00':'QEMU/KVM','00:16:3E':'Xen','02:42:AC':'Docker',
  '02:42:00':'Docker','0A:58:0A':'Kubernetes',
  '08:00:27':'VirtualBox',
  // Hyper-V
  '00:15:5D':'Hyper-V',
  // Eero
  'F4:A0:39':'Eero','78:8B:2A':'Eero',
  // Linksys
  '00:06:25':'Linksys','00:0C:41':'Linksys','00:12:17':'Linksys',
  '00:18:39':'Linksys','00:1C:10':'Linksys','20:AA:4B':'Linksys',
  'C0:C1:C0':'Linksys','48:F8:B3':'Linksys',
};

// ── Device type fingerprint patterns ─────────────────────────────────────────
const DEVICE_PATTERNS = [
  { type: 'Router/Gateway',   osGuess: 'Embedded',  portSet: [53, 80, 443], required: [80], vendorHints: ['Cisco','Netgear','Ubiquiti','TP-Link','D-Link','Linksys','ASUS','Eero'] },
  { type: 'NAS',              osGuess: 'Linux',      portSet: [80, 443, 445, 548, 873, 2049, 5000, 5001], required: [445], vendorHints: ['Synology','QNAP'] },
  { type: 'Smart Speaker',    osGuess: 'Embedded',   portSet: [80, 443, 1400, 1443, 8008, 8009], required: [], vendorHints: ['Sonos','Google','Amazon'] },
  { type: 'Smart TV / Stick', osGuess: 'Android TV', portSet: [80, 443, 8008, 8009, 9080], required: [8008], vendorHints: ['Roku','Samsung','Google'] },
  { type: 'Printer',          osGuess: 'Embedded',   portSet: [80, 443, 515, 631, 9100], required: [9100], vendorHints: ['HP','Brother','Epson','Canon'] },
  { type: 'Camera / DVR',     osGuess: 'Embedded',   portSet: [80, 443, 554, 8000, 34567, 37777], required: [554], vendorHints: ['Dahua','Hikvision'] },
  { type: 'Windows PC',       osGuess: 'Windows',    portSet: [135, 139, 445, 3389], required: [135], vendorHints: ['Dell','HP','Microsoft','Lenovo'] },
  { type: 'macOS Device',     osGuess: 'macOS',      portSet: [22, 548, 5000, 7000], required: [], vendorHints: ['Apple'] },
  { type: 'Linux Server',     osGuess: 'Linux',      portSet: [22, 80, 443], required: [22], vendorHints: ['Dell','HP'] },
  { type: 'IoT Device',       osGuess: 'Embedded',   portSet: [80, 1883, 8008, 9123], required: [1883], vendorHints: ['Philips Hue'] },
  { type: 'Laptop',           osGuess: 'Unknown',    portSet: [22, 80, 443], required: [], vendorHints: ['Apple','Dell','HP','Lenovo','ASUS','Microsoft'] },
  { type: 'Virtual Machine',  osGuess: 'Various',    portSet: [22, 80, 135, 443, 902, 903, 2376, 5985, 8006, 16509], required: [], vendorHints: ['VMware','VirtualBox','QEMU/KVM','Hyper-V','Parallels','Xen','Docker','Oracle VM','Kubernetes'] },
  { type: 'Hypervisor Host',  osGuess: 'Various',    portSet: [902, 903, 8006, 2376, 2377, 6443, 16509, 16514, 5985, 5986], required: [], vendorHints: ['VMware','Proxmox'] },
];

// ── Port impact/explanation library ───────────────────────────────────────────
const PORT_IMPACT = {
  23:   'Plaintext credential exposure can compromise the entire network.',
  135:  'MS-RPC exploitation enables remote code execution. Historic worms (Blaster, Sasser) spread via this port.',
  139:  'NetBIOS exposure enables credential harvesting and lateral movement.',
  445:  'SMB exploits like EternalBlue allow wormable remote code execution and ransomware propagation.',
  3389: 'Direct RDP exposure is the primary ransomware entry vector.',
  5900: 'VNC exposure allows complete graphical takeover. Weak passwords trivially brute-forced.',
};

const PORT_RUNBOOKS = {
  23:   [{ label:'CISA: Eliminate Telnet', url:'https://www.cisa.gov/uscert/ncas/tips/ST05-006' }],
  135:  [{ label:'Microsoft: Securing MS-RPC', url:'https://learn.microsoft.com/en-us/windows/win32/rpc/security' }],
  139:  [{ label:'Microsoft: Disable NetBIOS over TCP/IP', url:'https://learn.microsoft.com/en-us/troubleshoot/windows-server/networking/disable-netbios-tcp-ip-using-dhcp' }],
  445:  [{ label:'Microsoft Security Bulletin MS17-010', url:'https://learn.microsoft.com/en-us/security-updates/securitybulletins/2017/ms17-010' }, { label:'CISA Alert: SMB Security', url:'https://www.cisa.gov/news-events/cybersecurity-advisories/aa20-302a' }],
  3389: [{ label:'CISA: Reducing RDP Exposure', url:'https://www.cisa.gov/news-events/alerts/2018/10/19/alert-ta18-074a' }, { label:'Microsoft: Secure RDP Deployment', url:'https://learn.microsoft.com/en-us/windows-server/remote/remote-desktop-services/rds-secure-deployment' }],
  5900: [{ label:'NIST SP 800-46: Remote Access Security', url:'https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-46r2.pdf' }],
};

const ANOMALY_RUNBOOKS = {
  'Unknown MAC':    [{ label:'CIS Control 1: Device Inventory', url:'https://www.cisecurity.org/controls/inventory-and-control-of-enterprise-assets/' }],
  'New Device':     [{ label:'CIS Control 1: Authorized Device Policy', url:'https://www.cisecurity.org/controls/inventory-and-control-of-enterprise-assets/' }],
  'Ports Changed':  [{ label:'CIS Control 4: Configuration Management', url:'https://www.cisecurity.org/controls/secure-configuration-of-enterprise-assets-and-software/' }],
};

const PORT_EXPLAIN = {
  23:   { what:'Telnet service actively listening on port 23.', risk:'Telnet transmits ALL data — including passwords — in unencrypted plaintext.', steps:['Disable the Telnet daemon immediately','Install and configure OpenSSH','Update firewall rules to block port 23','Rotate any credentials that may have been exposed'] },
  135:  { what:'Microsoft Remote Procedure Call on port 135.', risk:'MS-RPC has been exploited by major network worms including Blaster and Sasser.', steps:['Block port 135 at the perimeter firewall','Apply all Windows security patches','Use Windows Firewall to restrict RPC to trusted ranges'] },
  139:  { what:'NetBIOS Session Service on port 139.', risk:'NetBIOS leaks system details and enables credential theft and lateral movement.', steps:['Disable NetBIOS over TCP/IP in network adapter settings','Block ports 137–139 at firewall','Migrate file sharing to SMBv2/v3'] },
  445:  { what:'SMB (Windows File Sharing) on port 445.', risk:'Unpatched SMB was the attack vector for WannaCry and NotPetya ransomware.', steps:['Apply MS17-010 patch immediately','Disable SMBv1: Set-SmbServerConfiguration -EnableSMB1Protocol $false','Block port 445 at firewall unless required','Restrict to trusted IP ranges'] },
  3389: { what:'Remote Desktop Protocol on port 3389.', risk:'RDP exposed directly is the #1 ransomware entry point. Bots scan 24/7.', steps:['Place RDP behind a VPN gateway','Enable Network Level Authentication (NLA)','Use strong passwords (20+ chars) for remote accounts','Enable account lockout after failed attempts'] },
  5900: { what:'VNC remote desktop on port 5900.', risk:'Many VNC implementations have weak authentication. Auth-bypass CVEs exist.', steps:['Tunnel VNC through SSH instead','Set strong VNC password and enable encryption','Block port 5900 at firewall','Switch to modern encrypted solution (RustDesk, etc.)'] },
};

// ── Network helpers ───────────────────────────────────────────────────────────
function getLocalNetworks() {
  const networks = [];
  for (const [name, ifaces] of Object.entries(os.networkInterfaces())) {
    for (const iface of ifaces) {
      if (iface.family === 'IPv4' && !iface.internal) {
        const parts = iface.address.split('.');
        networks.push({
          name,
          localIp:  iface.address,
          baseIp:   parts.slice(0, 3).join('.'),
          netmask:  iface.netmask,
          cidr:     `${parts.slice(0, 3).join('.')}.0/24`,
        });
      }
    }
  }
  if (networks.length === 0) {
    networks.push({ name:'default', localIp:'192.168.1.100', baseIp:'192.168.1', cidr:'192.168.1.0/24' });
  }
  return networks;
}

function getLocalNetwork() {
  return getLocalNetworks()[0] || { baseIp:'192.168.1', localIp:'192.168.1.100' };
}

// ── OUI vendor lookup ─────────────────────────────────────────────────────────
function lookupVendor(mac) {
  if (!mac || mac === 'Unknown') return null;
  // Try 6-char prefix first (XX:XX:XX)
  const prefix6 = mac.substring(0, 8).toUpperCase();
  if (OUI_VENDORS[prefix6]) return OUI_VENDORS[prefix6];
  return null;
}

// ── Ping with latency measurement ─────────────────────────────────────────────
async function pingHost(ip, gentle = false) {
  const timeout = gentle ? 2000 : 1000;
  const cmd = process.platform === 'win32'
    ? `ping -n 1 -w ${timeout} ${ip}`
    : `ping -c 1 -W ${Math.ceil(timeout / 1000)} ${ip}`;
  const start = Date.now();
  try {
    await execPromise(cmd, { timeout: timeout + 1500 });
    return { alive: true, latencyMs: Date.now() - start };
  } catch {
    // Fallback: TCP probe on common ports
    const t = gentle ? 600 : 350;
    const checks = await Promise.all([
      probePort(ip, 80, t), probePort(ip, 443, t),
      probePort(ip, 22, t), probePort(ip, 8080, t),
    ]);
    const alive = checks.some(Boolean);
    return { alive, latencyMs: alive ? Date.now() - start : null };
  }
}

// ── ARP table lookup ──────────────────────────────────────────────────────────
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

// ── TCP port probe ────────────────────────────────────────────────────────────
async function probePort(ip, port, timeout = 500) {
  return new Promise(resolve => {
    const sock = new net.Socket();
    let done = false;
    const finish = v => { if (!done) { done = true; sock.destroy(); resolve(v); } };
    sock.setTimeout(timeout);
    sock.on('connect', () => finish(true));
    sock.on('timeout', () => finish(false));
    sock.on('error',   () => finish(false));
    sock.connect(port, ip);
  });
}

// ── Port scanning ─────────────────────────────────────────────────────────────
async function scanPorts(ip, ports = SCAN_PORTS, concurrency = 40, gentle = false) {
  const timeout = gentle ? 800 : 450;
  const results = [];
  for (let i = 0; i < ports.length; i += concurrency) {
    const chunk = ports.slice(i, i + concurrency);
    const hits = await Promise.all(
      chunk.map(p => probePort(ip, p, timeout).then(open => open ? p : null))
    );
    results.push(...hits.filter(Boolean));
    if (gentle && i + concurrency < ports.length) {
      await new Promise(r => setTimeout(r, 50)); // gentle throttle
    }
  }
  return results;
}

// ── DNS reverse lookup ────────────────────────────────────────────────────────
async function resolveHostname(ip) {
  try { return (await dnsReverse(ip))[0] || null; }
  catch { return null; }
}

// ── Banner grabbing ───────────────────────────────────────────────────────────
async function grabBanner(ip, port, timeout = 1500) {
  return new Promise(resolve => {
    const sock = new net.Socket();
    let banner = '';
    let done = false;
    const finish = () => { if (!done) { done = true; sock.destroy(); resolve(banner.trim().slice(0, 256)); } };
    sock.setTimeout(timeout);
    sock.on('connect', () => {
      // Send HTTP request for HTTP ports, empty line for others
      if (port === 80 || port === 8080 || port === 8008) {
        sock.write(`HEAD / HTTP/1.0\r\nHost: ${ip}\r\n\r\n`);
      } else {
        setTimeout(() => finish(), 600); // wait for banner
      }
    });
    sock.on('data', d => { banner += d.toString('ascii', 0, 256); finish(); });
    sock.on('timeout', finish);
    sock.on('error', finish);
    sock.connect(port, ip);
  });
}

// ── TLS certificate info ──────────────────────────────────────────────────────
async function getTLSCert(ip, port, timeout = 3000) {
  return new Promise(resolve => {
    try {
      const sock = tls.connect({ host: ip, port, rejectUnauthorized: false, timeout }, () => {
        const cert = sock.getPeerCertificate();
        sock.destroy();
        if (!cert || !cert.subject) return resolve(null);
        resolve({
          cn:      cert.subject.CN   || null,
          org:     cert.subject.O    || null,
          issuer:  cert.issuer?.CN   || null,
          expiry:  cert.valid_to     || null,
          san:     cert.subjectaltname || null,
        });
      });
      sock.on('error', () => resolve(null));
      setTimeout(() => { sock.destroy(); resolve(null); }, timeout);
    } catch { resolve(null); }
  });
}

// ── mDNS / DNS-SD discovery ───────────────────────────────────────────────────
function discoverMDNS(timeoutMs = 3000) {
  return new Promise(resolve => {
    const results = new Map(); // ip -> { services: [], hostnames: [] }
    const sock = dgram.createSocket({ type: 'udp4', reuseAddr: true });
    const MDNS_ADDR = '224.0.0.251';
    const MDNS_PORT = 5353;

    // Common service types to query
    const services = [
      '_http._tcp.local', '_https._tcp.local', '_ssh._tcp.local',
      '_ftp._tcp.local', '_smb._tcp.local', '_afpovertcp._tcp.local',
      '_nfs._tcp.local', '_printer._tcp.local', '_ipp._tcp.local',
      '_airplay._tcp.local', '_raop._tcp.local', '_googlecast._tcp.local',
      '_sonos._tcp.local', '_homekit._tcp.local', '_hap._tcp.local',
      '_matter._tcp.local', '_apple-mobdev2._tcp.local',
      '_workstation._tcp.local', '_device-info._tcp.local',
    ];

    sock.on('error', () => resolve(results));
    sock.on('message', (msg, rinfo) => {
      try {
        const txt = msg.toString('binary');
        const ip = rinfo.address;
        if (!results.has(ip)) results.set(ip, { services: [], hostname: null });
        const entry = results.get(ip);

        // Extract service type names from DNS answers (simplified parsing)
        const patterns = services.map(s => s.replace('.local', ''));
        for (const svc of patterns) {
          if (txt.includes(svc.replace('._tcp', '').replace('._', '')) || txt.includes(svc)) {
            if (!entry.services.includes(svc + '.local')) {
              entry.services.push(svc + '.local');
            }
          }
        }
      } catch { /* ignore */ }
    });

    sock.bind(() => {
      try {
        sock.addMembership(MDNS_ADDR);
        // Send PTR query for each service type
        for (const svc of services) {
          const query = buildMDNSQuery(svc);
          if (query) sock.send(query, 0, query.length, MDNS_PORT, MDNS_ADDR);
        }
      } catch { /* ignore */ }
    });

    setTimeout(() => {
      try { sock.close(); } catch { /* ignore */ }
      resolve(results);
    }, timeoutMs);
  });
}

function buildMDNSQuery(name) {
  try {
    // Minimal DNS query packet (PTR query)
    const labels = name.split('.');
    const parts = [
      Buffer.from([0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    ];
    for (const label of labels) {
      const b = Buffer.from(label, 'ascii');
      parts.push(Buffer.from([b.length]), b);
    }
    parts.push(Buffer.from([0x00, 0x00, 0x0c, 0x00, 0x01])); // PTR + IN
    return Buffer.concat([Buffer.from([0x00, 0x00]), ...parts]);
  } catch { return null; }
}

// ── SSDP / UPnP discovery ─────────────────────────────────────────────────────
function discoverSSDP(timeoutMs = 3000) {
  return new Promise(resolve => {
    const results = new Map(); // ip -> { server, location, deviceType, friendlyName }
    const sock = dgram.createSocket('udp4');
    const SSDP_ADDR = '239.255.255.250';
    const SSDP_PORT = 1900;

    const msg = Buffer.from(
      'M-SEARCH * HTTP/1.1\r\n' +
      `HOST: ${SSDP_ADDR}:${SSDP_PORT}\r\n` +
      'MAN: "ssdp:discover"\r\n' +
      'MX: 2\r\n' +
      'ST: ssdp:all\r\n\r\n'
    );

    sock.on('error', () => resolve(results));
    sock.on('message', (data, rinfo) => {
      const ip = rinfo.address;
      const txt = data.toString();
      const server    = (txt.match(/SERVER:\s*(.+)/i)    || [])[1]?.trim();
      const location  = (txt.match(/LOCATION:\s*(.+)/i)  || [])[1]?.trim();
      const st        = (txt.match(/\bST:\s*(.+)/i)      || [])[1]?.trim();
      const usn       = (txt.match(/\bUSN:\s*(.+)/i)     || [])[1]?.trim();

      if (!results.has(ip)) results.set(ip, { server: null, location: null, deviceTypes: [], usn: null });
      const entry = results.get(ip);
      if (server)   entry.server = server;
      if (location) entry.location = location;
      if (st)       entry.deviceTypes.push(st);
      if (usn)      entry.usn = usn;
    });

    sock.bind(() => {
      try { sock.send(msg, 0, msg.length, SSDP_PORT, SSDP_ADDR); } catch { /* ignore */ }
    });

    setTimeout(() => {
      try { sock.close(); } catch { /* ignore */ }
      resolve(results);
    }, timeoutMs);
  });
}

// ── NetBIOS name discovery ────────────────────────────────────────────────────
async function lookupNetBIOS(ip, timeout = 1500) {
  return new Promise(resolve => {
    try {
      const sock = dgram.createSocket('udp4');
      // NetBIOS Name Service query
      const query = Buffer.from([
        0x00, 0x01, // Transaction ID
        0x00, 0x00, // Flags: Query
        0x00, 0x01, // Questions: 1
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Encoded name: CKAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA (wildcard *)
        0x20,
        0x43, 0x4b, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
        0x00, // End of name
        0x00, 0x21, // QTYPE: NBSTAT
        0x00, 0x01, // QCLASS: IN
      ]);

      let resolved = false;
      sock.on('message', data => {
        if (resolved) return;
        resolved = true;
        sock.close();
        // Parse NetBIOS name from response (simplified)
        const name = parseNetBIOSResponse(data);
        resolve(name);
      });
      sock.on('error', () => { if (!resolved) { resolved = true; resolve(null); } });
      sock.send(query, 0, query.length, 137, ip, () => {});
      setTimeout(() => {
        if (!resolved) {
          resolved = true;
          try { sock.close(); } catch { /* ignore */ }
          resolve(null);
        }
      }, timeout);
    } catch { resolve(null); }
  });
}

function parseNetBIOSResponse(data) {
  try {
    if (data.length < 57) return null;
    // Skip header (12 bytes) + question (34 bytes) + RR header (11 bytes) = offset 57
    const numNames = data[56];
    if (numNames < 1) return null;
    // Each name entry is 18 bytes: 15 bytes name + 1 type + 2 flags
    const name = data.slice(57, 72).toString('ascii').replace(/\x00/g, '').trim();
    return name || null;
  } catch { return null; }
}

// ── Device fingerprinting ─────────────────────────────────────────────────────
function fingerprintDevice({ ip, mac, hostname, ports, mdnsServices = [], ssdpInfo = null, netbiosName = null, banners = {} }) {
  const signals = [];
  let score = 0;

  // Signal 1: OUI vendor
  const vendor = lookupVendor(mac);
  if (vendor) {
    signals.push({ type: 'oui', label: 'MAC/OUI', value: vendor, weight: 30 });
    score += 30;
  }

  // Signal 2: mDNS service types
  if (mdnsServices.length > 0) {
    for (const svc of mdnsServices) {
      signals.push({ type: 'mdns', label: 'mDNS Service', value: svc, weight: 20 });
      score += 20;
    }
  }

  // Signal 3: SSDP/UPnP
  if (ssdpInfo) {
    if (ssdpInfo.server) {
      signals.push({ type: 'ssdp', label: 'UPnP/SSDP Server', value: ssdpInfo.server, weight: 25 });
      score += 25;
    }
    if (ssdpInfo.deviceTypes?.length > 0) {
      signals.push({ type: 'ssdp', label: 'UPnP Device Type', value: ssdpInfo.deviceTypes[0], weight: 15 });
      score += 15;
    }
  }

  // Signal 4: NetBIOS name
  if (netbiosName) {
    signals.push({ type: 'netbios', label: 'NetBIOS Name', value: netbiosName, weight: 15 });
    score += 15;
  }

  // Signal 5: Hostname patterns
  if (hostname) {
    const hn = hostname.toLowerCase();
    if (hn.includes('mac') || hn.includes('apple') || hn.includes('imac') || hn.includes('macbook'))
      { signals.push({ type: 'hostname', label: 'Hostname Pattern', value: 'Apple device pattern', weight: 15 }); score += 15; }
    else if (hn.includes('iphone') || hn.includes('ipad'))
      { signals.push({ type: 'hostname', label: 'Hostname Pattern', value: 'Apple iOS device', weight: 20 }); score += 20; }
    else if (hn.includes('android') || hn.includes('pixel') || hn.includes('galaxy'))
      { signals.push({ type: 'hostname', label: 'Hostname Pattern', value: 'Android device', weight: 15 }); score += 15; }
    else if (hn.includes('router') || hn.includes('gateway') || hn.includes('ap') || hn.includes('ubnt'))
      { signals.push({ type: 'hostname', label: 'Hostname Pattern', value: 'Network device', weight: 20 }); score += 20; }
    else if (hn.includes('synology') || hn.includes('qnap') || hn.includes('nas'))
      { signals.push({ type: 'hostname', label: 'Hostname Pattern', value: 'NAS device', weight: 25 }); score += 25; }
    else if (hn.includes('printer') || hn.includes('hp') || hn.includes('canon') || hn.includes('epson'))
      { signals.push({ type: 'hostname', label: 'Hostname Pattern', value: 'Printer', weight: 20 }); score += 20; }
    else if (hn.includes('raspberrypi') || hn.includes('pi'))
      { signals.push({ type: 'hostname', label: 'Hostname Pattern', value: 'Raspberry Pi', weight: 20 }); score += 20; }
    else if (hn !== '') {
      signals.push({ type: 'hostname', label: 'Hostname', value: hostname, weight: 5 });
      score += 5;
    }
  }

  // Signal 6: Banner content
  for (const [port, banner] of Object.entries(banners)) {
    if (banner) {
      signals.push({ type: 'banner', label: `Port ${port} Banner`, value: banner.slice(0, 60), weight: 20 });
      score += 20;
    }
  }

  // Signal 7: Port pattern matching for device type
  const portSet = new Set(ports);
  let bestMatch = null;
  let bestMatchScore = 0;

  for (const pattern of DEVICE_PATTERNS) {
    let matchScore = 0;
    const hasRequired = pattern.required.every(p => portSet.has(p));
    if (!hasRequired && pattern.required.length > 0) continue;

    for (const p of pattern.portSet) {
      if (portSet.has(p)) matchScore += 10;
    }
    if (vendor && pattern.vendorHints.includes(vendor)) matchScore += 20;

    // Check mDNS/SSDP alignment
    if (pattern.type === 'Smart Speaker' && mdnsServices.some(s => s.includes('sonos') || s.includes('googlecast'))) matchScore += 30;
    if (pattern.type === 'NAS' && (ssdpInfo?.server?.toLowerCase().includes('synology') || ssdpInfo?.server?.toLowerCase().includes('qnap'))) matchScore += 30;

    if (matchScore > bestMatchScore) {
      bestMatchScore = matchScore;
      bestMatch = pattern;
    }
  }

  let deviceType = 'Unknown Device';
  let osGuess    = 'Unknown';
  if (bestMatch && bestMatchScore > 0) {
    deviceType = bestMatch.type;
    osGuess    = bestMatch.osGuess;
    signals.push({ type: 'ports', label: 'Port Pattern', value: `Matches ${bestMatch.type} profile`, weight: bestMatchScore });
    score += Math.min(bestMatchScore, 30);
  }

  // Signal 8: VM / hypervisor detection
  const isVmByMac = mac && VM_MAC_PREFIXES.some(pfx => mac.toUpperCase().startsWith(pfx));
  const vmPortHits = ports.filter(p => VM_PORTS.has(p));
  const hypervisorPorts = ports.filter(p => [902, 903, 8006, 2376, 2377, 6443, 16509, 16514, 5985, 5986].includes(p));

  let isVirtualMachine = false;
  let isHypervisor = false;

  if (isVmByMac) {
    signals.push({ type: 'vm', label: 'VM MAC Address', value: `OUI matches virtual NIC (${vendor || mac?.substring(0, 8)})`, weight: 40 });
    score += 40;
    isVirtualMachine = true;
  }

  if (vmPortHits.length > 0) {
    signals.push({ type: 'vm', label: 'VM/Hypervisor Ports', value: vmPortHits.map(p => PORT_NAMES[p] || p).join(', '), weight: 25 });
    score += 25;
  }

  if (hypervisorPorts.length >= 1) {
    signals.push({ type: 'hypervisor', label: 'Hypervisor Management', value: hypervisorPorts.map(p => PORT_NAMES[p] || p).join(', '), weight: 35 });
    score += 35;
    isHypervisor = true;
  }

  // Check hostname patterns for VM / container names
  if (hostname) {
    const hnLow = hostname.toLowerCase();
    const vmHostPatterns = ['vm-', 'vm_', 'docker', 'container', 'kube', 'k8s', 'proxmox', 'esxi', 'hyperv', 'vbox', 'qemu', 'libvirt', 'lxc', 'xen'];
    const matchedPattern = vmHostPatterns.find(p => hnLow.includes(p));
    if (matchedPattern) {
      signals.push({ type: 'vm', label: 'VM Hostname Pattern', value: `Matches "${matchedPattern}" pattern`, weight: 20 });
      score += 20;
      isVirtualMachine = true;
    }
  }

  // Override device type if VM/hypervisor signals are strong
  if (isHypervisor && hypervisorPorts.length >= 1) {
    deviceType = 'Hypervisor Host';
    osGuess = 'Various';
  } else if (isVirtualMachine) {
    deviceType = 'Virtual Machine';
    osGuess = 'Various';
  }

  // Clamp confidence to 0-100
  const confidence = Math.min(100, Math.round(score));

  // Build explainability summary
  const reasons = signals
    .sort((a, b) => b.weight - a.weight)
    .map(s => `${s.label}: ${s.value}`)
    .slice(0, 5);

  const summary = reasons.length > 0
    ? `We think this is a ${deviceType} because: ${reasons.join('; ')}.`
    : `Unable to identify device type — no reliable signals found.`;

  return { vendor, deviceType, osGuess, confidence, signals, summary, isVirtualMachine, isHypervisor };
}

// ── Anomaly explanation library ───────────────────────────────────────────────
function getAnomalyExplanation(type, ports = []) {
  if (type === 'Risky Port') {
    const details = ports.map(p => PORT_EXPLAIN[p]).filter(Boolean);
    const impacts = ports.map(p => PORT_IMPACT[p]).filter(Boolean);
    const runbookLinks = [...new Set(
      ports.flatMap(p => PORT_RUNBOOKS[p] || []).map(r => JSON.stringify(r))
    )].map(s => JSON.parse(s));

    const explanation = details.length === 1 ? details[0]
      : details.length > 1 ? {
          what: details.map(d => d.what).join(' | '),
          risk: details.map(d => d.risk).join(' '),
          steps: [...new Set(details.flatMap(d => d.steps))]
        }
      : { what:`Unusual service(s) on port(s): ${ports.join(', ')}`, risk:'One or more services may be exploitable.', steps:['Identify the service','Disable if not required','Add firewall rules'] };

    return { ...explanation, impact: impacts[0] || 'This service may be exploitable.', runbookLinks, category:'exposure' };
  }

  if (type === 'Unknown MAC') return {
    what:'The device\'s hardware (MAC) address could not be resolved via the ARP table.',
    risk:'An unresolvable MAC may indicate MAC spoofing, a new unauthorized device, or a stale ARP cache.',
    impact:'Unresolvable device identity prevents accurate inventory tracking and MAC-based access controls.',
    steps:['Check your router\'s DHCP client list','Run: arp -n <ip> after the device sends traffic','If unrecognized, isolate by blocking its IP at the router','Enable MAC-address filtering on your router'],
    runbookLinks: ANOMALY_RUNBOOKS['Unknown MAC'], category:'identity',
  };

  if (type === 'New Device') return {
    what:'A device appeared on the network that was not present in the previous scan.',
    risk:'New devices could represent unauthorized access, a new IoT device with insecure defaults, or a guest connecting without authorization.',
    impact:'An unverified device has full network access. If unauthorized, it can exfiltrate data or intercept traffic.',
    steps:['Log into your router and review the DHCP lease table','Check the device\'s open ports to assess risk','If unrecognized, block its MAC at the router and change your Wi-Fi password','Enable WPA3 encryption and disable WPS'],
    runbookLinks: ANOMALY_RUNBOOKS['New Device'], category:'inventory',
  };

  if (type === 'Virtual Machine Detected') return {
    what:'A device with a virtual network adapter (VM MAC OUI) was found on the network.',
    risk:'Virtual machines can be used to hide malicious activity, run unauthorized services, or bypass network segmentation and monitoring.',
    impact:'A hidden VM may have separate firewall rules, run unpatched software, or tunnel traffic outside your network without detection.',
    steps:['Identify the hypervisor host running this VM','Verify the VM is authorized and inventoried','Check VM network configuration (bridged vs NAT)','Audit running services inside the VM','If unauthorized, shut down and investigate the host machine'],
    runbookLinks: [{ label:'CIS Control 1: Hardware Asset Inventory', url:'https://www.cisecurity.org/controls/inventory-and-control-of-enterprise-assets/' }], category:'virtualization',
  };

  if (type === 'Hypervisor Detected') return {
    what:'A device running hypervisor management services was found on the network.',
    risk:'Hypervisors control multiple virtual machines and represent a high-value target. Compromise of a hypervisor means compromise of all VMs it hosts.',
    impact:'An exposed hypervisor management interface can allow attackers to create, modify, or destroy VMs, exfiltrate data, or establish persistent access.',
    steps:['Restrict hypervisor management interfaces to a dedicated management VLAN','Enable strong authentication (MFA if possible)','Keep hypervisor software fully patched','Audit all VMs running on this host','Block management ports from general network access'],
    runbookLinks: [{ label:'CIS Benchmark: VMware ESXi', url:'https://www.cisecurity.org/benchmark/vmware' }], category:'virtualization',
  };

  if (type === 'Ports Changed') return {
    what:'The open port configuration on this device changed since the last scan.',
    risk:'Newly opened ports may indicate a newly installed service, a backdoor, or a misconfiguration.',
    impact:'The attack surface of this device changed unexpectedly. New open ports may expose vulnerable services.',
    steps:['Identify what opened: ss -tlnp (Linux) or netstat -ano (Windows)','If unintentional, run a full malware scan','Review recent software installations','Check firewall logs for unusual outbound connections'],
    runbookLinks: ANOMALY_RUNBOOKS['Ports Changed'], category:'drift',
  };

  return { what:type, risk:'Review this anomaly carefully.', impact:'Potential security risk — manual investigation required.', steps:[], runbookLinks:[], category:'unknown' };
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
      let driftType = 'risk_detected';
      if (prev) {
        const prevRisky = new Set(prev.ports.filter(p => RISKY_PORTS.has(p)));
        const newlyRisky = risky.filter(p => !prevRisky.has(p));
        driftType = newlyRisky.length > 0 ? 'risk_increased' : 'persistent_risk';
      }
      anomalies.push({
        id: `risky-${dev.ip}-${risky.join('-')}`,
        type: 'Risky Port', severity: 'High',
        device: dev.ip, hostname: dev.hostname || dev.name,
        description: `Dangerous service(s): ${risky.map(p => `${PORT_NAMES[p]||p} (${p})`).join(', ')}`,
        ports: risky, driftType,
        driftDetails: prev ? { previousPorts: prev.ports, currentPorts: dev.ports } : null,
        ...getAnomalyExplanation('Risky Port', risky)
      });
    }

    // Rule 2: Unknown MAC
    if (!dev.mac || dev.mac === 'Unknown') {
      anomalies.push({
        id: `mac-${dev.ip}`,
        type: 'Unknown MAC', severity: 'Medium',
        device: dev.ip, hostname: dev.hostname || dev.name,
        description: 'MAC address could not be resolved via ARP.',
        ports: [], driftType: 'identity_gap', driftDetails: null,
        ...getAnomalyExplanation('Unknown MAC')
      });
    }

    // Rule 3: New device
    if (previousSnapshot.length > 0 && !prevMap.has(dev.ip)) {
      anomalies.push({
        id: `new-${dev.ip}`,
        type: 'New Device', severity: 'Low',
        device: dev.ip, hostname: dev.hostname || dev.name,
        description: 'Device not present in previous scan — appeared since last check.',
        ports: [], driftType: 'new_device',
        driftDetails: { firstSeen: new Date().toISOString() },
        ...getAnomalyExplanation('New Device')
      });
    }

    // Rule 4: Virtual machine detection
    const fp = dev.fingerprint || {};
    if (fp.isVirtualMachine) {
      anomalies.push({
        id: `vm-${dev.ip}`,
        type: 'Virtual Machine Detected', severity: 'Medium',
        device: dev.ip, hostname: dev.hostname || dev.name,
        description: `Virtual machine detected (${dev.vendor || 'VM MAC'}). Verify this VM is authorized.`,
        ports: [], driftType: 'vm_detected', driftDetails: { vendor: dev.vendor, mac: dev.mac },
        ...getAnomalyExplanation('Virtual Machine Detected')
      });
    }

    // Rule 5: Hypervisor detection
    if (fp.isHypervisor) {
      anomalies.push({
        id: `hypervisor-${dev.ip}`,
        type: 'Hypervisor Detected', severity: 'High',
        device: dev.ip, hostname: dev.hostname || dev.name,
        description: `Hypervisor management interface detected. High-value target — verify access controls.`,
        ports: dev.ports.filter(p => [902, 903, 8006, 2376, 2377, 6443, 16509, 16514, 5985, 5986].includes(p)),
        driftType: 'hypervisor_detected', driftDetails: { managementPorts: dev.ports.filter(p => VM_PORTS.has(p)) },
        ...getAnomalyExplanation('Hypervisor Detected')
      });
    }

    // Rule 6: Port drift
    if (prev) {
      const prevSet = new Set(prev.ports);
      const currSet = new Set(dev.ports);
      const added   = dev.ports.filter(p => !prevSet.has(p));
      const removed = prev.ports.filter(p => !currSet.has(p));
      if (added.length || removed.length) {
        const changes = [];
        if (added.length)   changes.push(`+${added.map(p => PORT_NAMES[p]||p).join(', ')}`);
        if (removed.length) changes.push(`-${removed.map(p => PORT_NAMES[p]||p).join(', ')}`);
        const hasNewRisky = added.some(p => RISKY_PORTS.has(p));
        anomalies.push({
          id: `ports-${dev.ip}`,
          type: 'Ports Changed', severity: 'Medium',
          device: dev.ip, hostname: dev.hostname || dev.name,
          description: `Port profile changed: ${changes.join(' | ')}`,
          ports: added, driftType: hasNewRisky ? 'risk_increased' : 'ports_changed',
          driftDetails: { added, removed, previousPorts: prev.ports, currentPorts: dev.ports },
          ...getAnomalyExplanation('Ports Changed')
        });
      }
    }
  }

  return anomalies;
}

// ── Scan modes ────────────────────────────────────────────────────────────────

async function quickScan(progressCb) {
  const report = msg => progressCb?.(msg);
  const networks = getLocalNetworks();
  const net0 = networks[0];
  const multiSubnet = networks.length > 1;

  report(`Quick scan — ${multiSubnet ? networks.length + ' subnet(s)' : net0.cidr}`);
  report(`Launching mDNS, SSDP, and IPv6 NDP discovery in parallel...`);

  // Run mDNS, SSDP, and IPv6 NDP in parallel
  const [mdnsResults, ssdpResults, ipv6Neighbors] = await Promise.all([
    discoverMDNS(2500).catch(() => new Map()),
    discoverSSDP(2500).catch(() => new Map()),
    discoverIPv6Neighbors().catch(() => new Map()),
  ]);

  if (ipv6Neighbors.size > 0) {
    report(`IPv6 NDP: ${ipv6Neighbors.size} neighbor(s) found.`);
  }

  report(`Multi-cast discovery complete. Running ARP ping sweep${multiSubnet ? ' across all subnets' : ''}...`);

  // Multi-subnet IP list
  const ips = buildSubnetIPs(networks);
  const alive = [];
  const BATCH = 30;

  for (let i = 0; i < ips.length; i += BATCH) {
    const hits = await Promise.all(
      ips.slice(i, i + BATCH).map(ip => pingHost(ip, false).then(r => r.alive ? ip : null))
    );
    alive.push(...hits.filter(Boolean));
    report(`Ping sweep: ${Math.min(i + BATCH, ips.length)}/${ips.length} — ${alive.length} host(s) found`);
  }

  for (const net of networks) {
    if (!alive.includes(net.localIp)) alive.push(net.localIp);
  }

  // Merge IPv6-only hosts (hosts found via NDP but not via ARP)
  for (const [ipv6, info] of ipv6Neighbors) {
    if (!alive.includes(ipv6)) alive.push(ipv6);
  }

  report(`${alive.length} device(s) found. Resolving identities...`);

  const devices = await Promise.all(alive.map(async ip => {
    const [mac, hostname] = await Promise.all([getMac(ip), resolveHostname(ip)]);
    const mdns   = mdnsResults.get(ip);
    const ssdp   = ssdpResults.get(ip);
    const nbName = null; // skip for quick scan
    const isIPv6 = ip.includes(':');

    // For IPv6 hosts, try to get MAC from NDP table
    const macFinal = (isIPv6 && ipv6Neighbors.has(ip)) ? (ipv6Neighbors.get(ip).mac || mac) : mac;
    const fp = fingerprintDevice({ ip, mac: macFinal, hostname, ports: [], mdnsServices: mdns?.services || [], ssdpInfo: ssdp || null, netbiosName: nbName });

    const localIp = networks.find(n => ip.startsWith(n.baseIp))?.localIp || net0.localIp;
    const name = hostname || (ip === localIp ? 'This Device' : `Device-${ip.split('.').pop()}`);
    return {
      ip, mac: macFinal, name, hostname,
      vendor: fp.vendor,
      deviceType: fp.deviceType,
      osGuess: fp.osGuess,
      confidence: fp.confidence,
      fingerprint: fp,
      ports: [],
      services: {},
      mdnsServices: mdns?.services || [],
      ssdpInfo: ssdp || null,
      netbiosName: nbName,
      isIPv6,
      latencyMs: null,
    };
  }));

  report(`Quick scan complete — ${devices.length} device(s) inventoried.`);
  return devices;
}

async function standardScan(progressCb, gentle = false) {
  const report = msg => progressCb?.(msg);
  const networks = getLocalNetworks();
  const net0 = networks[0];

  report(`Standard scan — subnet ${net0.cidr}`);
  report(`Launching mDNS and SSDP discovery...`);

  const [mdnsResults, ssdpResults] = await Promise.all([
    discoverMDNS(3000).catch(() => new Map()),
    discoverSSDP(3000).catch(() => new Map()),
  ]);

  report(`Multi-cast discovery complete. Starting ping sweep...`);

  const ips   = Array.from({ length: 254 }, (_, i) => `${net0.baseIp}.${i + 1}`);
  const alive = [];
  const BATCH = gentle ? 15 : 25;
  const latencyMap = {};

  for (let i = 0; i < ips.length; i += BATCH) {
    const hits = await Promise.all(
      ips.slice(i, i + BATCH).map(ip => pingHost(ip, gentle).then(r => {
        if (r.alive) { latencyMap[ip] = r.latencyMs; return ip; }
        return null;
      }))
    );
    alive.push(...hits.filter(Boolean));
    report(`Ping sweep: ${Math.min(i + BATCH, 254)}/254 — ${alive.length} host(s) responding`);
    if (gentle) await new Promise(r => setTimeout(r, 30));
  }

  if (!alive.includes(net0.localIp)) alive.push(net0.localIp);
  report(`${alive.length} device(s) found. Probing ports and resolving identities...`);

  const devices = await Promise.all(alive.map(async ip => {
    const [ports, mac, hostname] = await Promise.all([
      scanPorts(ip, SCAN_PORTS, 40, gentle),
      getMac(ip),
      resolveHostname(ip),
    ]);

    const mdns  = mdnsResults.get(ip);
    const ssdp  = ssdpResults.get(ip);
    const fp    = fingerprintDevice({ ip, mac, hostname, ports, mdnsServices: mdns?.services || [], ssdpInfo: ssdp || null });

    const name = hostname || (ip === net0.localIp ? 'This Device' : `Device-${ip.split('.').pop()}`);
    return {
      ip, mac, name, hostname,
      vendor: fp.vendor,
      deviceType: fp.deviceType,
      osGuess: fp.osGuess,
      confidence: fp.confidence,
      fingerprint: fp,
      ports,
      services: buildServiceMap(ports),
      mdnsServices: mdns?.services || [],
      ssdpInfo: ssdp || null,
      netbiosName: null,
      latencyMs: latencyMap[ip] || null,
    };
  }));

  report(`Standard scan complete — ${devices.length} device(s) inventoried.`);
  return devices;
}

async function deepScan(progressCb, portProfile = 'common', gentle = false) {
  const report = msg => progressCb?.(msg);
  const networks = getLocalNetworks();
  const net0 = networks[0];
  const ports = PORT_PROFILES[portProfile] || SCAN_PORTS;

  report(`Deep scan — subnet ${net0.cidr} (${ports.length} ports, gentle=${gentle})`);
  report(`Launching multi-method discovery: mDNS, SSDP, NetBIOS...`);

  const [mdnsResults, ssdpResults] = await Promise.all([
    discoverMDNS(4000).catch(() => new Map()),
    discoverSSDP(4000).catch(() => new Map()),
  ]);

  report(`Discovery complete. Starting ping sweep...`);

  const ips   = Array.from({ length: 254 }, (_, i) => `${net0.baseIp}.${i + 1}`);
  const alive = [];
  const BATCH = gentle ? 10 : 20;
  const latencyMap = {};

  for (let i = 0; i < ips.length; i += BATCH) {
    const hits = await Promise.all(
      ips.slice(i, i + BATCH).map(ip => pingHost(ip, gentle).then(r => {
        if (r.alive) { latencyMap[ip] = r.latencyMs; return ip; }
        return null;
      }))
    );
    alive.push(...hits.filter(Boolean));
    report(`Ping sweep: ${Math.min(i + BATCH, 254)}/254 — ${alive.length} host(s) found`);
    if (gentle) await new Promise(r => setTimeout(r, 50));
  }

  if (!alive.includes(net0.localIp)) alive.push(net0.localIp);
  report(`${alive.length} device(s) found. Starting deep port scan + service detection...`);

  let completed = 0;
  const devices = [];
  for (const ip of alive) {
    const [openPorts, mac, hostname] = await Promise.all([
      scanPorts(ip, ports, gentle ? 20 : 40, gentle),
      getMac(ip),
      resolveHostname(ip),
    ]);

    // Banner grabbing on HTTP/SSH ports
    const banners = {};
    const bannerPorts = openPorts.filter(p => [21, 22, 25, 80, 8080, 110, 8008].includes(p));
    await Promise.all(bannerPorts.map(async p => {
      const banner = await grabBanner(ip, p, 1200);
      if (banner) banners[p] = banner;
    }));

    // TLS cert on HTTPS ports
    let tlsCert = null;
    const tlsPorts = openPorts.filter(p => [443, 8443, 9443].includes(p));
    if (tlsPorts.length > 0) {
      tlsCert = await getTLSCert(ip, tlsPorts[0]);
    }

    // NetBIOS lookup (if SMB/NetBIOS port open or hostname unknown)
    let netbiosName = null;
    if (openPorts.includes(139) || openPorts.includes(445) || !hostname) {
      netbiosName = await lookupNetBIOS(ip);
    }

    const mdns = mdnsResults.get(ip);
    const ssdp = ssdpResults.get(ip);
    const fp   = fingerprintDevice({ ip, mac, hostname, ports: openPorts, mdnsServices: mdns?.services || [], ssdpInfo: ssdp || null, netbiosName, banners });

    const name = netbiosName || hostname || (ip === net0.localIp ? 'This Device' : `Device-${ip.split('.').pop()}`);
    const services = buildServiceMap(openPorts, banners, tlsCert);

    devices.push({
      ip, mac, name, hostname,
      vendor: fp.vendor,
      deviceType: fp.deviceType,
      osGuess: fp.osGuess,
      confidence: fp.confidence,
      fingerprint: fp,
      ports: openPorts,
      services,
      banners,
      tlsCert,
      mdnsServices: mdns?.services || [],
      ssdpInfo: ssdp || null,
      netbiosName,
      latencyMs: latencyMap[ip] || null,
    });

    completed++;
    report(`Deep scan: ${completed}/${alive.length} device(s) processed — ${ip}`);
    if (gentle && completed < alive.length) await new Promise(r => setTimeout(r, 100));
  }

  report(`Deep scan complete — ${devices.length} device(s) fully analyzed.`);
  return devices;
}

function buildServiceMap(openPorts, banners = {}, tlsCert = null) {
  const services = {};
  for (const p of openPorts) {
    const name = PORT_NAMES[p] || `Port ${p}`;
    const banner = banners[p] || null;
    let version = null;

    if (banner) {
      // Extract version info from common banner formats
      const sshM = banner.match(/SSH-[\d.]+-(\S+)/);
      const httpM = banner.match(/Server:\s*(.+)/i);
      const ftpM  = banner.match(/220[- ](.+)/);
      if (sshM)  version = sshM[1];
      else if (httpM) version = httpM[1].trim();
      else if (ftpM)  version = ftpM[1].trim();
    }

    services[p] = { name, banner, version };
    if ((p === 443 || p === 8443) && tlsCert) {
      services[p].tlsCN     = tlsCert.cn;
      services[p].tlsExpiry = tlsCert.expiry;
      services[p].tlsOrg    = tlsCert.org;
      services[p].tlsIssuer = tlsCert.issuer;
    }
  }
  return services;
}

// ── IPv6 NDP neighbor discovery ───────────────────────────────────────────────
async function discoverIPv6Neighbors() {
  try {
    let output = '';
    if (process.platform === 'win32') {
      const { stdout } = await execPromise('netsh interface ipv6 show neighbors', { timeout: 6000 });
      output = stdout;
    } else if (process.platform === 'darwin') {
      try {
        const { stdout } = await execPromise('ndp -an 2>/dev/null', { timeout: 6000 });
        output = stdout;
      } catch {
        const { stdout } = await execPromise('ip -6 neigh show 2>/dev/null', { timeout: 6000 });
        output = stdout;
      }
    } else {
      const { stdout } = await execPromise('ip -6 neigh show 2>/dev/null', { timeout: 6000 });
      output = stdout;
    }

    const results = new Map(); // ipv6 -> { mac }
    for (const line of output.split('\n')) {
      // Linux: "2001:db8::1 dev eth0 lladdr aa:bb:cc:dd:ee:ff REACHABLE"
      const m = line.match(/^(\S+)\s+dev\s+\S+(?:\s+lladdr\s+([0-9a-f:]{11,17}))?/i);
      if (m && m[1] && !m[1].startsWith('fe80') && m[1] !== '::1') {
        results.set(m[1], { mac: m[2] || null });
      }
    }
    return results;
  } catch {
    return new Map();
  }
}

// ── Multi-subnet IP range builder ─────────────────────────────────────────────
function buildSubnetIPs(networks) {
  const allIPs = new Set();
  for (const net of networks) {
    const parts = net.localIp.split('.');
    const base = parts.slice(0, 3).join('.');
    for (let i = 1; i <= 254; i++) {
      allIPs.add(`${base}.${i}`);
    }
  }
  return [...allIPs];
}

// ── Main entry point ──────────────────────────────────────────────────────────
async function scanNetwork(progressCallback, opts = {}) {
  const { mode = 'standard', gentle = false, portProfile = 'common' } = opts;
  if (mode === 'quick')    return quickScan(progressCallback);
  if (mode === 'deep')     return deepScan(progressCallback, portProfile, gentle);
  return standardScan(progressCallback, gentle);
}

module.exports = {
  scanNetwork, analyzeAnomalies, getAnomalyExplanation,
  quickScan, standardScan, deepScan,
  discoverMDNS, discoverSSDP, lookupNetBIOS,
  discoverIPv6Neighbors, buildSubnetIPs,
  fingerprintDevice, lookupVendor,
  grabBanner, getTLSCert,
  getLocalNetwork, getLocalNetworks,
  RISKY_PORTS, VM_PORTS, VM_MAC_PREFIXES, PORT_NAMES, PORT_PROFILES,
};
