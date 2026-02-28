'use strict';

const express = require('express');

// ── Per-port defensive guidance ───────────────────────────────────────────────
const PORT_GUIDANCE = {
  23:   { service: 'Telnet',   riskLevel: 'Critical', guidance: 'Replace Telnet with SSH immediately. Disable the telnetd daemon and remove any related packages. Rotate all credentials that may have been transmitted in plaintext.' },
  135:  { service: 'MS-RPC',   riskLevel: 'High',     guidance: 'Block MS-RPC (port 135) at the perimeter firewall. Apply all available Windows security patches, especially MS03-026. Use Windows Firewall to restrict RPC to trusted internal hosts.' },
  139:  { service: 'NetBIOS',  riskLevel: 'High',     guidance: 'Disable NetBIOS over TCP/IP in adapter settings. Block ports 137–139 at the network boundary. Migrate file sharing to SMBv2/v3 over port 445 with proper ACLs.' },
  445:  { service: 'SMB',      riskLevel: 'Critical', guidance: 'Apply MS17-010 patch (KB4012212) immediately. Disable SMBv1 via PowerShell: Set-SmbServerConfiguration -EnableSMB1Protocol $false. Restrict port 445 to trusted subnets only.' },
  3389: { service: 'RDP',      riskLevel: 'Critical', guidance: 'Move RDP behind a VPN gateway — never expose it directly. Enable NLA and account lockout policies. Use a 20+ character password. Consider IP allowlisting for RDP access.' },
  5900: { service: 'VNC',      riskLevel: 'High',     guidance: 'Tunnel VNC through SSH (ssh -L 5901:localhost:5900). Set a strong password and enable encryption. Block port 5900 externally. Consider switching to a modern encrypted solution.' }
};

function buildEnrichment(device) {
  const risky = (device.ports || []).filter(p => PORT_GUIDANCE[p]);

  if (risky.length === 0) {
    return {
      riskLevel: 'Informational',
      summary:   'No immediately dangerous services detected.',
      guidance:  'Keep all software up to date, use strong unique passwords, and ensure a host-based firewall is active. Continue periodic scans to detect configuration drift.',
      services:  []
    };
  }

  const services   = risky.map(p => ({ port: p, ...PORT_GUIDANCE[p] }));
  const hasCrit    = services.some(s => s.riskLevel === 'Critical');
  const riskLevel  = hasCrit ? 'Critical' : 'High';

  return {
    riskLevel,
    summary:  `${risky.length} high-risk service(s) detected. Immediate remediation recommended.`,
    guidance: `This device exposes ${risky.length} dangerous service(s). Follow the per-service guidance below and apply firewall restrictions as a priority.`,
    services
  };
}

// ── Factory ───────────────────────────────────────────────────────────────────
function createCloudMockService() {
  const app = express();
  app.use(express.json());

  let ledger      = [];              // chronological activity log
  let deviceStore = {};              // ip → [ledger entry ids]

  function addEntry(action, deviceIp, details, meta = {}) {
    const entry = {
      id:             `${action}-${Date.now()}-${Math.random().toString(36).slice(2,6)}`,
      action,
      deviceIp,
      details,
      timestamp:      new Date().toISOString(),
      // Transparency fields
      endpoint:       meta.endpoint       || null,
      dataCategories: meta.dataCategories || [],
      dataPreview:    meta.dataPreview    || null,
      reason:         meta.reason         || null,
    };
    ledger.push(entry);
    return entry;
  }

  // POST /enrich — send device metadata, receive defensive guidance
  app.post('/enrich', (req, res) => {
    const { device } = req.body;
    if (!device?.ip) return res.status(400).json({ error: 'device.ip required' });

    const result = buildEnrichment(device);
    const entry  = addEntry(
      'SEND',
      device.ip,
      `Enrichment — ${device.ip} (${device.name || 'Unknown'}) — Risk: ${result.riskLevel}`,
      {
        endpoint:       'POST /enrich',
        dataCategories: ['network_metadata', 'device_identity', 'service_inventory'],
        dataPreview: {
          ip:       device.ip,
          hostname: device.name || null,
          ports:    device.ports || [],
        },
        reason: 'User-initiated cloud enrichment for defensive risk assessment and remediation guidance',
      }
    );

    if (!deviceStore[device.ip]) deviceStore[device.ip] = [];
    deviceStore[device.ip].push(entry.id);

    res.json({ ...result, ledgerEntryId: entry.id });
  });

  // GET /ledger — full activity log (newest first)
  app.get('/ledger', (_req, res) => {
    res.json([...ledger].reverse());
  });

  // GET /devices — list of IPs that have stored data
  app.get('/devices', (_req, res) => {
    res.json(Object.keys(deviceStore));
  });

  // DELETE /data — wipe everything
  app.delete('/data', (_req, res) => {
    const count = ledger.length;
    ledger      = [];
    deviceStore = {};
    addEntry('DELETE_ALL', null, `All data purged — ${count} record(s) deleted`, {
      endpoint:       'DELETE /data',
      dataCategories: ['network_metadata', 'device_identity', 'service_inventory'],
      reason:         'User-requested deletion of all cloud enrichment records',
    });
    res.json({ success: true, deleted: count, message: `${count} record(s) permanently removed from cloud storage.` });
  });

  // DELETE /data/:ip — wipe a specific device
  app.delete('/data/:ip', (req, res) => {
    const ip    = req.params.ip;
    const ids   = new Set(deviceStore[ip] || []);
    const count = ids.size;

    ledger = ledger.filter(e => !ids.has(e.id));
    delete deviceStore[ip];

    addEntry('DELETE_DEVICE', ip, `Device data purged — ${ip} — ${count} record(s) deleted`, {
      endpoint:       `DELETE /data/${ip}`,
      dataCategories: ['network_metadata', 'device_identity', 'service_inventory'],
      reason:         `User-requested deletion of enrichment records for device ${ip}`,
    });
    res.json({ success: true, deleted: count, message: `${count} record(s) for ${ip} permanently removed.` });
  });

  return app;
}

module.exports = { createCloudMockService };
