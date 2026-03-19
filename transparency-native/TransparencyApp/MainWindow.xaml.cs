using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using TransparencyApp.Models;
using TransparencyApp.Services;

namespace TransparencyApp
{
    public partial class MainWindow : Window
    {
        private readonly NetworkScanner   _scanner = new();
        private readonly CloudMockService _cloud   = new();
        private Button? _activeNav;

        public MainWindow()
        {
            InitializeComponent();
            DeviceGrid.ItemsSource    = new List<Device>();
            AnomaliesList.ItemsSource = new List<Anomaly>();
            SetActiveNav(NavDashboard);
        }

        // ── Navigation ────────────────────────────────────────────────────────
        private void Nav_Click(object sender, RoutedEventArgs e)
        {
            if (sender is not Button btn) return;
            var target = btn.Tag?.ToString() ?? "Dashboard";

            ViewDashboard.Visibility = target == "Dashboard" ? Visibility.Visible : Visibility.Collapsed;
            ViewAnomalies.Visibility = target == "Anomalies" ? Visibility.Visible : Visibility.Collapsed;
            ViewLedger.Visibility    = target == "Ledger"    ? Visibility.Visible : Visibility.Collapsed;
            ViewSettings.Visibility  = target == "Settings"  ? Visibility.Visible : Visibility.Collapsed;

            if (target == "Ledger") RefreshLedger();
            SetActiveNav(btn);
        }

        private void SetActiveNav(Button btn)
        {
            // Reset previous
            if (_activeNav is not null)
                _activeNav.Foreground = new SolidColorBrush(
                    (Color)ColorConverter.ConvertFromString("#8B90A0"));

            _activeNav = btn;
            btn.Foreground = new SolidColorBrush(
                (Color)ColorConverter.ConvertFromString("#5B8DEF"));
        }

        // ── Scan ──────────────────────────────────────────────────────────────
        private async void Scan_Click(object sender, RoutedEventArgs e)
        {
            ScanBtn.IsEnabled     = false;
            LoadingOverlay.Visibility = Visibility.Visible;

            try
            {
                var progress = new Progress<string>(msg =>
                {
                    TxtScanStatus.Text    = msg;
                    TxtLoadingDetail.Text = msg;
                });

                var (devices, anomalies) = await _scanner.ScanAsync(progress: progress);

                // Dashboard
                DeviceGrid.ItemsSource = devices;
                TxtTotalDevices.Text   = devices.Count.ToString();
                TxtActiveThreats.Text  = anomalies.Count.ToString();
                TxtCloudSyncs.Text     = _cloud.Ledger.Count(l => l.Action == "ENRICH").ToString();
                TxtLastScanned.Text    = $"Last scan: {DateTime.Now:HH:mm:ss}  —  {devices.Count} device(s) found";

                // Anomalies view
                AnomaliesList.ItemsSource      = anomalies;
                AnomalyEmptyState.Visibility   = anomalies.Count == 0
                    ? Visibility.Visible : Visibility.Collapsed;
                TxtAnomalyCount.Text = $"{anomalies.Count} anomaly(ies) found";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Scan failed:\n{ex.Message}", "Scan Error",
                                MessageBoxButton.OK, MessageBoxImage.Warning);
            }
            finally
            {
                LoadingOverlay.Visibility = Visibility.Collapsed;
                ScanBtn.IsEnabled         = true;
            }
        }

        // ── Enrich ────────────────────────────────────────────────────────────
        private async void Enrich_Click(object sender, RoutedEventArgs e)
        {
            if (sender is not Button btn) return;
            if (btn.DataContext is not Device device) return;

            btn.IsEnabled = false;
            try
            {
                var guidance = await _cloud.EnrichDeviceAsync(device);
                MessageBox.Show(guidance,
                    $"Cloud Guidance — {device.IpAddress}",
                    MessageBoxButton.OK, MessageBoxImage.Information);

                TxtCloudSyncs.Text = _cloud.Ledger.Count(l => l.Action == "ENRICH").ToString();
                RefreshLedger();
            }
            finally
            {
                btn.IsEnabled = true;
            }
        }

        // ── Settings / deletion ───────────────────────────────────────────────
        private async void DeleteData_Click(object sender, RoutedEventArgs e)
        {
            var result = MessageBox.Show(
                "Delete all device enrichment history from the cloud service?",
                "Confirm Deletion",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);

            if (result != MessageBoxResult.Yes) return;

            await _cloud.DeleteCloudDataAsync();
            RefreshLedger();
            MessageBox.Show("All cloud data has been deleted.",
                            "Deleted", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        // ── Ledger ────────────────────────────────────────────────────────────
        private void RefreshLedger()
        {
            LedgerList.Items.Clear();

            if (_cloud.Ledger.Count == 0)
            {
                LedgerList.Items.Add("No activity recorded yet.");
                return;
            }

            foreach (var entry in _cloud.Ledger.OrderByDescending(l => l.Timestamp))
            {
                var prefix = entry.Action switch
                {
                    "ENRICH" => "[SEND  ]",
                    "PURGE"  => "[PURGE ]",
                    _        => "[INFO  ]"
                };
                LedgerList.Items.Add(
                    $"{entry.Timestamp:HH:mm:ss}  {prefix}  {entry.Details}");
            }
        }
    }
}
