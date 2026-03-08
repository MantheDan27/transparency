const { test } = require('node:test');
const assert = require('node:assert');
const { getAnomalyExplanation } = require('../scanner.js');

test('getAnomalyExplanation - Risky Port (Single)', () => {
  const explanation = getAnomalyExplanation('Risky Port', [23]);
  assert.strictEqual(explanation.category, 'exposure');
  assert.strictEqual(explanation.impact, 'Plaintext credential exposure can compromise the entire network.');
  assert.strictEqual(explanation.what, 'Telnet service actively listening on port 23.');
  assert.ok(explanation.risk.includes('Telnet'));
  assert.ok(explanation.steps.length > 0);
  assert.ok(explanation.runbookLinks.length > 0);
});

test('getAnomalyExplanation - Risky Port (Multiple)', () => {
  const explanation = getAnomalyExplanation('Risky Port', [23, 135]);
  assert.strictEqual(explanation.category, 'exposure');
  assert.strictEqual(explanation.impact, 'Plaintext credential exposure can compromise the entire network.');
  assert.ok(explanation.what.includes('Telnet service actively listening on port 23. | Microsoft Remote Procedure Call on port 135.'));
  assert.ok(explanation.risk.includes('Telnet transmits ALL data'));
  assert.ok(explanation.risk.includes('MS-RPC has been exploited'));
  assert.ok(explanation.steps.length > 0);
  assert.ok(explanation.runbookLinks.length > 0);
});

test('getAnomalyExplanation - Unknown MAC', () => {
  const explanation = getAnomalyExplanation('Unknown MAC');
  assert.strictEqual(explanation.category, 'identity');
  assert.strictEqual(explanation.what, 'The device\'s hardware (MAC) address could not be resolved via the ARP table.');
  assert.ok(explanation.steps.length > 0);
});

test('getAnomalyExplanation - New Device', () => {
  const explanation = getAnomalyExplanation('New Device');
  assert.strictEqual(explanation.category, 'inventory');
  assert.strictEqual(explanation.what, 'A device appeared on the network that was not present in the previous scan.');
  assert.ok(explanation.steps.length > 0);
});

test('getAnomalyExplanation - Virtual Machine Detected', () => {
  const explanation = getAnomalyExplanation('Virtual Machine Detected');
  assert.strictEqual(explanation.category, 'virtualization');
  assert.strictEqual(explanation.what, 'A device with a virtual network adapter (VM MAC OUI) was found on the network.');
  assert.ok(explanation.steps.length > 0);
});

test('getAnomalyExplanation - Hypervisor Detected', () => {
  const explanation = getAnomalyExplanation('Hypervisor Detected');
  assert.strictEqual(explanation.category, 'virtualization');
  assert.strictEqual(explanation.what, 'A device running hypervisor management services was found on the network.');
  assert.ok(explanation.steps.length > 0);
});

test('getAnomalyExplanation - Ports Changed', () => {
  const explanation = getAnomalyExplanation('Ports Changed');
  assert.strictEqual(explanation.category, 'drift');
  assert.strictEqual(explanation.what, 'The open port configuration on this device changed since the last scan.');
  assert.ok(explanation.steps.length > 0);
});

test('getAnomalyExplanation - Unknown Type', () => {
  const explanation = getAnomalyExplanation('Some Random Weird Type');
  assert.strictEqual(explanation.category, 'unknown');
  assert.strictEqual(explanation.what, 'Some Random Weird Type');
  assert.strictEqual(explanation.risk, 'Review this anomaly carefully.');
});
