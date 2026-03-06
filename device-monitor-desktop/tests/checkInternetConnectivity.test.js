const test = require('node:test');
const assert = require('node:assert');
const proxyquire = require('proxyquire');

const mockElectron = {
  app: { getPath: () => '/tmp', whenReady: () => Promise.resolve(), on: () => {} },
  BrowserWindow: class { loadFile() { return Promise.resolve(); } static getAllWindows() { return []; } },
  ipcMain: { handle: () => {} },
  shell: {}, dialog: {}
};

test('checkInternetConnectivity', async (t) => {
  let axiosHeadCalled = false;
  let triggerAlertArgs = null;

  // Store original globals
  const originalDateNow = Date.now;
  const testTime = 1600000000000;
  Date.now = () => testTime;

  const mocks = {
    electron: mockElectron,
    axios: {
      head: async () => { axiosHeadCalled = true; return {}; },
      post: async () => {} // added post just in case
    },
    './scanner': { scanNetwork: async () => [], analyzeAnomalies: () => [], getLocalNetwork: () => {}, getLocalNetworks: () => [] },
    './cloud-mock': { createCloudMockService: () => ({ listen: () => ({ close: () => {} }) }) }
  };

  const app = proxyquire('../main.js', mocks);

  t.after(() => {
    Date.now = originalDateNow;
    setTimeout(() => process.exit(0), 100);
  });

  await t.test('internet recovers from offline state', async () => {
    axiosHeadCalled = false;
    app.clearTriggeredAlerts();
    app.setInternetStatus({ online: false, lastCheck: null, latencyMs: null });
    app.setMonitoringConfig({ alertOnHighLatency: false });

    await app.checkInternetConnectivity();

    assert.strictEqual(axiosHeadCalled, true);
    assert.strictEqual(app.getInternetStatus().online, true);
    assert.strictEqual(app.getInternetStatus().latencyMs, 0); // Date.now() is mocked to return same value

    const alerts = app.getTriggeredAlerts();
    assert.strictEqual(alerts.length, 1);
    assert.strictEqual(alerts[0].ruleId, 'monitor-internet-restored');
  });

  await t.test('internet high latency alert', async () => {
    axiosHeadCalled = false;
    app.clearTriggeredAlerts();
    app.setInternetStatus({ online: true, lastCheck: null, latencyMs: null });
    app.setMonitoringConfig({ alertOnHighLatency: true, highLatencyThresholdMs: 100 });

    // Make Date.now return different values to simulate high latency
    let time = 1000;
    Date.now = () => { const curr = time; time += 150; return curr; }; // 150ms latency

    await app.checkInternetConnectivity();

    assert.strictEqual(app.getInternetStatus().online, true);
    assert.strictEqual(app.getInternetStatus().latencyMs, 150);

    const alerts = app.getTriggeredAlerts();
    assert.strictEqual(alerts.length, 1);
    assert.strictEqual(alerts[0].ruleId, 'monitor-high-latency');
    assert.strictEqual(alerts[0].data.latencyMs, 150);

    Date.now = () => testTime; // Reset
  });

  await t.test('internet goes offline', async () => {
    app.clearTriggeredAlerts();
    app.setInternetStatus({ online: true, lastCheck: null, latencyMs: null });
    app.setMonitoringConfig({ alertOnOutage: true });

    // Temporarily replace axios.head to throw
    const originalHead = mocks.axios.head;
    mocks.axios.head = async () => { throw new Error('Network error'); };

    await app.checkInternetConnectivity();

    assert.strictEqual(app.getInternetStatus().online, false);
    assert.strictEqual(app.getInternetStatus().latencyMs, null);

    const alerts = app.getTriggeredAlerts();
    assert.strictEqual(alerts.length, 1);
    assert.strictEqual(alerts[0].ruleId, 'monitor-outage');

    mocks.axios.head = originalHead; // Reset
  });
});
