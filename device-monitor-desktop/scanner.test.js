const test = require('node:test');
const assert = require('node:assert');
const os = require('os');
const { getLocalNetworks } = require('./scanner');

test('getLocalNetworks', async (t) => {
  await t.test('returns valid external IPv4 interfaces', () => {
    t.mock.method(os, 'networkInterfaces', () => ({
      lo: [
        { address: '127.0.0.1', family: 'IPv4', internal: true, netmask: '255.0.0.0' }
      ],
      eth0: [
        { address: '192.168.1.50', family: 'IPv4', internal: false, netmask: '255.255.255.0' },
        { address: 'fe80::1ff:fe23:4567:890a', family: 'IPv6', internal: false, netmask: 'ffff:ffff:ffff:ffff::' }
      ],
      wlan0: [
        { address: '10.0.0.100', family: 'IPv4', internal: false, netmask: '255.255.255.0' }
      ]
    }));

    const networks = getLocalNetworks();

    assert.strictEqual(networks.length, 2);

    assert.strictEqual(networks[0].name, 'eth0');
    assert.strictEqual(networks[0].localIp, '192.168.1.50');
    assert.strictEqual(networks[0].baseIp, '192.168.1');
    assert.strictEqual(networks[0].netmask, '255.255.255.0');
    assert.strictEqual(networks[0].cidr, '192.168.1.0/24');

    assert.strictEqual(networks[1].name, 'wlan0');
    assert.strictEqual(networks[1].localIp, '10.0.0.100');
    assert.strictEqual(networks[1].baseIp, '10.0.0');
    assert.strictEqual(networks[1].netmask, '255.255.255.0');
    assert.strictEqual(networks[1].cidr, '10.0.0.0/24');
  });

  await t.test('ignores IPv6 and internal interfaces', () => {
    t.mock.method(os, 'networkInterfaces', () => ({
      lo: [
        { address: '127.0.0.1', family: 'IPv4', internal: true, netmask: '255.0.0.0' },
        { address: '::1', family: 'IPv6', internal: true, netmask: 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' }
      ],
      docker0: [
        { address: '172.17.0.1', family: 'IPv4', internal: true, netmask: '255.255.0.0' }
      ],
      eth0: [
        { address: 'fe80::1ff:fe23:4567:890a', family: 'IPv6', internal: false, netmask: 'ffff:ffff:ffff:ffff::' }
      ]
    }));

    const networks = getLocalNetworks();

    // Should fallback to default because no valid external IPv4 interfaces exist
    assert.strictEqual(networks.length, 1);
    assert.strictEqual(networks[0].name, 'default');
    assert.strictEqual(networks[0].localIp, '192.168.1.100');
    assert.strictEqual(networks[0].baseIp, '192.168.1');
    assert.strictEqual(networks[0].cidr, '192.168.1.0/24');
  });

  await t.test('returns default fallback when no interfaces exist', () => {
    t.mock.method(os, 'networkInterfaces', () => ({}));

    const networks = getLocalNetworks();

    assert.strictEqual(networks.length, 1);
    assert.strictEqual(networks[0].name, 'default');
    assert.strictEqual(networks[0].localIp, '192.168.1.100');
    assert.strictEqual(networks[0].baseIp, '192.168.1');
    assert.strictEqual(networks[0].cidr, '192.168.1.0/24');
  });
});
