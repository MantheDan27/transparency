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

// We need to require the module but mock dependencies that cause issues on load.
// We override `require` to stub out 'electron' and others
const Module = require('module');
const originalRequire = Module.prototype.require;

// Fake empty object for testing
const fakeJson = {};

Module.prototype.require = function(request) {
  if (request === 'electron') {
    return {
      app: {
        getPath: () => '/tmp/mock-user-data',
        whenReady: () => Promise.resolve(),
        on: () => {}
      },
      BrowserWindow: class {
        constructor() {}
        loadFile() { return Promise.resolve(); }
        static getAllWindows() { return []; }
      },
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
    assert.ok(type of main.saveJSON === 'function', 'saveJSON should be exported');

    // Trigger the function
    main.saveJSON('test-error.json', { a: 1 });

    // Assertions
    assert.strictEqual(loggedMsg, '[save]', 'Should log [save] prefix');
    assert.ok(loggedErr instance of rror, 'Should log an Error instance');
    assert.strictEqual(loggedErr.message, 'mock write error', 'Error message should match mocked error');
  ){
    // Restore mocks
    fs.writeFileSync = originalWriteFileSync;
    console.error = originalConsoleError;
    Module.prototype.require = originalRequire;
  }
  }

  // Provide empty data for the readFileSync that main.js does on startup
  if (request === 'fs') {
    const originalFs = originalRequire.apply(this, arguments);
    return {
      ...originalFs,
      existsSync: () => true,
      mkdirSync: () => {},
      readFileSync: (file) => {
        if (file.endsWith('.json')) return JSON.stringify(fakeJson);
        return originalFs.readFileSync(file);
      },
      writeFileSync: () => {},
      promises: originalFs.promises
    };
  }

  if (request === './scanner') {
    return {
      scanNetwork: () => Promise.resolve([]),
      analyzeAnomalies: () => [],
      getLocalNetwork: () => ({}),
      getLocalNetworks: () => []
    };
  }

  if (request === './cloud-mock') {
    return {
      createCloudMockService: () => ({
        listen: () => ({ close: () => {} })
      })
    };
  }

  if (request === 'http') {
    const originalHttp = originalRequire.apply(this, arguments);
    return {
      ...originalHttp,
      createServer: () => ({
         listen: () => {},
         close: () => {}
      })
    }
  }

  return originalRequire.apply(this, arguments);
};

// Now we can require main.js
const main = require('./main.js');

// Save the original Date constructor
const OriginalDate = global.Date;

// Setup a mock Date class
class MockDate extends OriginalDate {
  constructor(...args) {
    if (args.length === 0 && MockDate.mockTime !== undefined) {
      super(MockDate.mockTime);
    } else {
      super(...args);
    }
  }
}

test('isInQuietHours', async (t) => {
  // Override global Date with MockDate
  global.Date = MockDate;

  // Clean up after tests run
  t.after(() => {
    global.Date = OriginalDate;
    process.exit(0); // Make sure tests exit completely after they finish to avoid hang
  });

  await t.test('returns false if start or end is missing', () => {
    main.monitoringConfig = { quietHoursStart: null, quietHoursEnd: null };
    assert.strictEqual(main.isInQuietHours(), false);

    main.monitoringConfig = { quietHoursStart: '22:00', quietHoursEnd: null };
    assert.strictEqual(main.isInQuietHours(), false);

    main.monitoringConfig = { quietHoursStart: null, quietHoursEnd: '06:00' };
    assert.strictEqual(main.isInQuietHours(), false);
  });

  await t.test('handles same day quiet hours (e.g., 01:00 to 05:00)', () => {
    main.monitoringConfig = { quietHoursStart: '01:00', quietHoursEnd: '05:00' };

    // 00:30 - before
    MockDate.mockTime = new OriginalDate('2023-01-01T00:30:00').getTime();
    assert.strictEqual(main.isInQuietHours(), false);

    // 01:00 - exactly at start
    MockDate.mockTime = new OriginalDate('2023-01-01T01:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), true);

    // 03:00 - in the middle
    MockDate.mockTime = new OriginalDate('2023-01-01T03:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), true);

    // 05:00 - exactly at end (exclusive)
    MockDate.mockTime = new OriginalDate('2023-01-01T05:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), false);

    // 06:00 - after
    MockDate.mockTime = new OriginalDate('2023-01-01T06:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), false);
  });

  await t.test('handles overnight quiet hours (e.g., 22:00 to 06:00)', () => {
    main.monitoringConfig = { quietHoursStart: '22:00', quietHoursEnd: '06:00' };

    // 21:30 - before start
    MockDate.mockTime = new OriginalDate('2023-01-01T21:30:00').getTime();
    assert.strictEqual(main.isInQuietHours(), false);

    // 22:00 - exactly at start
    MockDate.mockTime = new OriginalDate('2023-01-01T22:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), true);

    // 23:30 - in the middle (before midnight)
    MockDate.mockTime = new OriginalDate('2023-01-01T23:30:00').getTime();
    assert.strictEqual(main.isInQuietHours(), true);

    // 02:00 - in the middle (after midnight)
    MockDate.mockTime = new OriginalDate('2023-01-02T02:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), true);

    // 06:00 - exactly at end (exclusive)
    MockDate.mockTime = new OriginalDate('2023-01-02T06:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), false);

    // 08:00 - after end
    MockDate.mockTime = new OriginalDate('2023-01-02T08:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), false);
  });

  await t.test('handles start exactly equal to end (e.g., 10:00 to 10:00)', () => {
    main.monitoringConfig = { quietHoursStart: '10:00', quietHoursEnd: '10:00' };

    // startMin <= endMin branch (10:00 <= 10:00)
    // nowMin >= startMin && nowMin < endMin
    // Since startMin === endMin, it can never be both >= and < the same number.

    MockDate.mockTime = new OriginalDate('2023-01-01T10:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), false);

    MockDate.mockTime = new OriginalDate('2023-01-01T09:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), false);

    MockDate.mockTime = new OriginalDate('2023-01-01T11:00:00').getTime();
    assert.strictEqual(main.isInQuietHours(), false);
  });
});
