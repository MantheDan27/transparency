const test = require('node:test');
const assert = require('node:assert');
const { lookupVendor } = require('../scanner.js');

test('lookupVendor', async (t) => {
  await t.test('returns null for falsy, empty, or Unknown inputs', () => {
    assert.strictEqual(lookupVendor(null), null);
    assert.strictEqual(lookupVendor(undefined), null);
    assert.strictEqual(lookupVendor(''), null);
    assert.strictEqual(lookupVendor('Unknown'), null);
  });

  await t.test('returns vendor for known OUI prefixes (uppercase)', () => {
    assert.strictEqual(lookupVendor('00:03:93:11:22:33'), 'Apple');
    assert.strictEqual(lookupVendor('00:07:AB:CD:EF:00'), 'Samsung');
  });

  await t.test('returns vendor for known OUI prefixes (lowercase)', () => {
    assert.strictEqual(lookupVendor('00:03:93:aa:bb:cc'), 'Apple');
  });

  await t.test('returns vendor when MAC only contains prefix', () => {
    assert.strictEqual(lookupVendor('00:03:93'), 'Apple');
  });

  await t.test('returns null for unknown OUI prefixes', () => {
    assert.strictEqual(lookupVendor('FF:FF:FF:FF:FF:FF'), null);
  });

  await t.test('handles short invalid inputs gracefully', () => {
    assert.strictEqual(lookupVendor('00:0'), null);
    assert.strictEqual(lookupVendor('invalid_mac'), null);
  });
});
