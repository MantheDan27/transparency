const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');

// Mock electron so we can require main.js
const Module = require('module');
const originalRequire = Module.prototype.require;
Module.prototype.require = function(request) {
  if (request === 'electron') {
    return {
      app: {
        getPath: () => '/tmp/mock-userData',
        whenReady: async () => ({ then: () => {} }),
        on: () => {}
      },
      BrowserWindow: class {
        static getAllWindows() { return []; }
      },
      ipcMain: { handle: () => {} },
      shell: {},
      dialog: {}
    };
  }
  return originalRequire.apply(this, arguments);
};

test('loadJSON correctly parses valid JSON', () => {
  const { loadJSON, saveJSON, dataFile } = require('../main.js');

  const testData = { foo: 'bar' };
  saveJSON('test.json', testData);

  const result = loadJSON('test.json', {});
  assert.deepStrictEqual(result, testData);

  // Cleanup
  if (fs.existsSync(dataFile('test.json'))) {
    fs.unlinkSync(dataFile('test.json'));
  }
});

test('loadJSON returns default on missing file', () => {
  const { loadJSON, dataFile } = require('../main.js');

  const missingFile = 'missing.json';
  if (fs.existsSync(dataFile(missingFile))) {
    fs.unlinkSync(dataFile(missingFile));
  }

  const def = { default: 'value' };
  const result = loadJSON(missingFile, def);
  assert.deepStrictEqual(result, def);
});

test('loadJSON returns default on invalid JSON', () => {
  const { loadJSON, dataFile } = require('../main.js');

  const invalidFile = 'invalid.json';
  fs.writeFileSync(dataFile(invalidFile), '{ invalid: json }');

  const def = { default: 'value' };
  const result = loadJSON(invalidFile, def);
  assert.deepStrictEqual(result, def);

  // Cleanup
  if (fs.existsSync(dataFile(invalidFile))) {
    fs.unlinkSync(dataFile(invalidFile));
  }
});

test('loadJSON returns default on read error (mocked)', () => {
  const { loadJSON } = require('../main.js');

  // Mock fs.readFileSync
  const originalReadFileSync = fs.readFileSync;
  fs.readFileSync = () => {
    throw new Error('EACCES: permission denied');
  };

  try {
    const def = { fallback: true };
    const result = loadJSON('anything.json', def);
    assert.deepStrictEqual(result, def);
  } finally {
    // Restore fs.readFileSync
    fs.readFileSync = originalReadFileSync;
  }
});
