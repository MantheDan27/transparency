const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const Module = require('module');

test('saveJSON error path logs error to console', () => {
  const originalWriteFileSync = fs.writeFileSync;
  const originalConsoleError = console.error;
  const originalRequire = Module.prototype.require;

  let loggedMsg = null;
  let loggedErr = null;

  try {
    // Mock fs.writeFileSync to throw an error
    fs.writeFileSync = () => {
      throw new Error('mock write error');
    };

    // Mock console.error to capture arguments
    console.error = (msg, err) => {
      if (msg === '[save]') {
        loggedMsg = msg;
        loggedErr = err;
      }
    };

    // Mock electron module since it requires specific build binaries
    const mockElectron = {
      app: {
        getPath: () => '/tmp',
        whenReady: () => Promise.resolve(),
        on: () => {}
      },
      BrowserWindow: class { loadFile() { return Promise.resolve(); } static getAllWindows() { return []; } },
      ipcMain: { handle: () => {} },
      shell: { openExternal: () => {} },
      dialog: { showSaveDialog: () => {} }
    };

    Module.prototype.require = function(path) {
      if (path === 'electron') return mockElectron;
      return originalRequire.apply(this, arguments);
    };

    // Load main.js
    const main = require('./main.js');

    // Ensure saveJSON exists
    assert.ok(typeof main.saveJSON === 'function', 'saveJSON should be exported');

    // Trigger the function
    main.saveJSON('test-error.json', { a: 1 });

    // Assertions
    assert.strictEqual(loggedMsg, '[save]', 'Should log [save] prefix');
    assert.ok(loggedErr instanceof Error, 'Should log an Error instance');
    assert.strictEqual(loggedErr.message, 'mock write error', 'Error message should match mocked error');
  } finally {
    // Restore mocks
    fs.writeFileSync = originalWriteFileSync;
    console.error = originalConsoleError;
    Module.prototype.require = originalRequire;
  }
});
