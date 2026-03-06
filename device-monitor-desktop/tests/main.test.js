const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');

// We need a way to mock the alertRules config because the default doesn't have a webhook URL
const originalReadFileSync = fs.readFileSync;
fs.readFileSync = function(path, encoding) {
  if (typeof path === 'string' && path.includes('alert-rules.json')) {
    return JSON.stringify([
      {
        id: 'rule-test-webhook',
        name: 'Test webhook rule',
        enabled: true,
        conditions: { deviceFilter: 'all', eventType: 'new_device', timeWindow: null },
        actions: { notification: true, webhook: 'http://example.com/webhook', severity: 'medium', debounceMinutes: 1 },
        createdAt: new Date().toISOString(),
      }
    ]);
  }
  return originalReadFileSync.apply(this, arguments);
};

// Mock dependencies
const requireMock = require('module');
const originalRequire = requireMock.prototype.require;

let axiosMock = {
  post: () => Promise.resolve()
};

requireMock.prototype.require = function(request) {
  if (request === 'axios') return axiosMock;
  if (request === 'electron') {
    return {
      app: {
        getPath: () => '/tmp',
        whenReady: () => new Promise(() => {}), // never resolves so app setup won't crash
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

// We don't want servers to start
const http = require('http');
http.createServer = () => ({ listen: () => {}, close: () => {} });

// Keep process from hanging
test.after(() => {
  process.exit(0);
});

const main = require('../main.js');

test('webhook error handling in triggerAlert', async (t) => {
  await t.test('should catch and ignore errors from axios.post', async () => {
    let postCalled = false;
    let errorCaught = false;

    // Track unhandled promise rejections
    const unhandledRejectionListener = (reason) => {
      errorCaught = true;
    };
    process.on('unhandledRejection', unhandledRejectionListener);

    // Mock axios to reject
    axiosMock.post = (url, data, config) => {
      postCalled = true;
      return Promise.reject(new Error('Network Error from webhook'));
    };

    // Call triggerAlert - it should use the mock alert-rules.json setup above
    main.triggerAlert(
      'rule-test-webhook',
      'Test Rule',
      'medium',
      'title',
      'message',
      '192.168.1.10',
      {}
    );

    // Wait for promise chain to resolve
    await new Promise(resolve => setTimeout(resolve, 50));

    assert.ok(postCalled, 'Axios post should be called');
    assert.strictEqual(errorCaught, false, 'Should not fire unhandledRejection because error should be caught');

    // Clean up listener
    process.removeListener('unhandledRejection', unhandledRejectionListener);
  });

  await t.test('should proceed normally when axios.post succeeds', async () => {
    let postCalled = false;

    axiosMock.post = (url, data, config) => {
      postCalled = true;
      return Promise.resolve({ data: 'ok' });
    };

    // Call triggerAlert with a different IP to avoid debouncing
    main.triggerAlert(
      'rule-test-webhook',
      'Test Rule',
      'medium',
      'title',
      'message',
      '192.168.1.11',
      {}
    );

    // Wait for promise chain to resolve
    await new Promise(resolve => setTimeout(resolve, 50));

    assert.ok(postCalled, 'Axios post should be called on success path');
  });
});
