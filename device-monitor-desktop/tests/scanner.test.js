const { test, describe, beforeEach, afterEach } = require('node:test');
const assert = require('node:assert');
const cp = require('child_process');

// Before requiring scanner.js, we mock child_process.exec so that when util.promisify
// runs during module loading, it captures our mock wrapper.
const originalExec = cp.exec;
let execImplementation = originalExec;

cp.exec = function(cmd, options, cb) {
  return execImplementation(cmd, options, cb);
};

const scanner = require('../scanner.js');

describe('scanner getMac', () => {
  afterEach(() => {
    // Restore the implementation to the original after each test
    execImplementation = originalExec;
  });

  test('returns Unknown on exec throw', async () => {
    // Override the mock implementation for this specific test to return an error
    execImplementation = (cmd, options, cb) => {
      if (typeof options === 'function') {
        cb = options;
      }
      cb(new Error('Mocked exec failure'));
    };

    const mac = await scanner.getMac('192.168.1.100');
    assert.strictEqual(mac, 'Unknown', 'Should return "Unknown" when execPromise throws');
  });
});
