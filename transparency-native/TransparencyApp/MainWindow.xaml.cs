using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
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
        private List<Device> _lastDevices = new();
        private List<Anomaly> _lastAnomalies = new();

        private static readonly Brush NavInactiveFg = new SolidColorBrush(
            (Color)ColorConverter.ConvertFromString("#A8B6CE"));
        private static readonly Brush NavActiveFg = new SolidColorBrush(
            (Color)ColorConverter.ConvertFromString("#EAF1FF"));
        private static readonly Brush NavActiveBg = new SolidColorBrush(
            (Color)ColorConverter.ConvertFromString("#4A17243D"));
        private static readonly Brush NavActiveBorder = new SolidColorBrush(
            (Color)ColorConverter.ConvertFromString("#88658ECF"));

        public MainWindow()
        {
            InitializeComponent();
            DeviceGrid.ItemsSource    = new List<Device>();
            AnomaliesList.ItemsSource = new List<Anomaly>();
            SetActiveNav(NavDashboard);
            TxtCloudSyncs.Text = "0";
            TxtSidebarPosture.Text = "Awaiting scan";
        }

        // ── Navigation ────────────────────────────────────────────────────────
        private void Nav_Click(object sender, RoutedEventArgs e)
        {
            if (sender is not Button btn) return;
            var target = btn.Tag?.ToString() ?? "Dashboard";
            ShowView(target, btn);
        }

        private void ShowView(string target, Button? selectedNav = null)
        {
            ViewDashboard.Visibility = target == "Dashboard" ? Visibility.Visible : Visibility.Collapsed;
            ViewAnomalies.Visibility = target == "Anomalies" ? Visibility.Visible : Visibility.Collapsed;
            ViewLedger.Visibility    = target == "Ledger"    ? Visibility.Visible : Visibility.Collapsed;
            ViewSettings.Visibility  = target == "Settings"  ? Visibility.Visible : Visibility.Collapsed;

            if (target == "Ledger") RefreshLedger();
            if (selectedNav is not null) SetActiveNav(selectedNav);
        }

        private void SetActiveNav(Button btn)
        {
            // Reset previous
            if (_activeNav is not null)
            {
                _activeNav.Foreground  = NavInactiveFg;
                _activeNav.Background  = Brushes.Transparent;
                _activeNav.BorderBrush = Brushes.Transparent;
            }

            _activeNav = btn;
            btn.Foreground  = NavActiveFg;
            btn.Background  = NavActiveBg;
            btn.BorderBrush = NavActiveBorder;
        }

        private static int ComputePostureScore(List<Device> devices, List<Anomaly> anomalies)
        {
            var high    = anomalies.Count(a => string.Equals(a.Severity, "High",   StringComparison.OrdinalIgnoreCase));
            var medium  = anomalies.Count(a => string.Equals(a.Severity, "Medium", StringComparison.OrdinalIgnoreCase));
            var low     = anomalies.Count(a => string.Equals(a.Severity, "Low",    StringComparison.OrdinalIgnoreCase));
            var unknown = devices.Count(d => d.MacAddress == "Unknown");

            var score = 100 - (high * 18) - (medium * 9) - (low * 4) - (unknown * 2);
            return Math.Clamp(score, 0, 100);
        }

        private void UpdatePostureAndSummary(List<Device> devices, List<Anomaly> anomalies)
        {
            var score = ComputePostureScore(devices, anomalies);
            TxtPostureScore.Text = score.ToString();

            TxtSidebarPosture.Text = score switch
            {
                >= 85 => "Healthy",
                >= 70 => "Guarded",
                >= 50 => "Elevated risk",
                _     => "Critical"
            };

            if (anomalies.Count == 0)
            {
                TxtTriageSummary.Text = "Auto triage ready: no current anomalies, continue periodic scans.";
                return;
            }

            var high = anomalies.FirstOrDefault(a => a.Severity == "High");
            if (high is not null)
            {
                TxtTriageSummary.Text =
                    $"Auto triage: prioritize {high.Type} on {high.DeviceIp} (high severity).";
                return;
            }

            var medium = anomalies.FirstOrDefault(a => a.Severity == "Medium");
            if (medium is not null)
            {
                TxtTriageSummary.Text =
                    $"Auto triage: review {medium.Type} on {medium.DeviceIp} (medium severity).";
                return;
            }

            TxtTriageSummary.Text =
                $"Auto triage: {anomalies.Count} low-severity finding(s) pending validation.";
        }

        private static int SeverityRank(string severity) => severity switch
        {
            "High"   => 3,
            "Medium" => 2,
            _        => 1
        };

        private void SelectDeviceInGrid(Device device)
        {
            DeviceGrid.SelectedItem = device;
            DeviceGrid.ScrollIntoView(device);
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
                _lastDevices = devices;
                _lastAnomalies = anomalies;

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
                UpdatePostureAndSummary(devices, anomalies);
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
                TxtTriageSummary.Text = $"Enrichment guidance retrieved for {device.IpAddress}.";
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
            TxtCloudSyncs.Text = _cloud.Ledger.Count(l => l.Action == "ENRICH").ToString();
            TxtTriageSummary.Text = "Cloud enrichment history has been purged.";
            MessageBox.Show("All cloud data has been deleted.",
                            "Deleted", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void AutoTriage_Click(object sender, RoutedEventArgs e)
        {
            // Open anomalies using the same path as the anomalies nav button.
            Nav_Click(NavAnomalies, new RoutedEventArgs(Button.ClickEvent));

            var anomalies = (AnomaliesList.ItemsSource as IEnumerable<Anomaly>)?.ToList()
                            ?? AnomaliesList.Items.Cast<object>().OfType<Anomaly>().ToList();
            var totalCount = anomalies.Count;

            if (totalCount == 0)
            {
                TxtTriageSummary.Text = "Auto triage complete: no findings to review.";
                MessageBox.Show(
                    "No anomalies are currently available to triage.",
                    "Auto Triage",
                    MessageBoxButton.OK,
                    MessageBoxImage.Information);
                return;
            }

            var highCount = anomalies.Count(a =>
                string.Equals(a.Severity, "High", StringComparison.OrdinalIgnoreCase));
            var mediumCount = anomalies.Count(a =>
                string.Equals(a.Severity, "Medium", StringComparison.OrdinalIgnoreCase));
            var lowCount = totalCount - highCount - mediumCount;

            if (highCount > 0)
            {
                TxtTriageSummary.Text = $"Auto triage: {highCount} high-severity finding(s) need immediate review.";
                MessageBox.Show(
                    $"Auto triage summary:\n\n" +
                    $"Total anomalies: {totalCount}\n" +
                    $"High: {highCount}\n" +
                    $"Medium: {mediumCount}\n" +
                    $"Low: {lowCount}\n\n" +
                    "High-severity anomalies are present. Review immediately.",
                    "Auto Triage - Immediate Review",
                    MessageBoxButton.OK,
                    MessageBoxImage.Warning);
                return;
            }

            TxtTriageSummary.Text = $"Auto triage: {mediumCount} medium and {lowCount} low findings in queue.";
            MessageBox.Show(
                $"Auto triage summary:\n\n" +
                $"Total anomalies: {totalCount}\n" +
                $"Medium: {mediumCount}\n" +
                $"Low: {lowCount}\n\n" +
                "No high-severity anomalies detected.",
                "Auto Triage Summary",
                MessageBoxButton.OK,
                MessageBoxImage.Information);
        }

        private void CopySnapshot_Click(object sender, RoutedEventArgs e)
        {
            if (_lastDevices.Count == 0 && _lastAnomalies.Count == 0)
            {
                MessageBox.Show("No scan data available yet.",
                                "Incident Snapshot", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            var score = ComputePostureScore(_lastDevices, _lastAnomalies);
            var high = _lastAnomalies.Count(a => a.Severity == "High");
            var medium = _lastAnomalies.Count(a => a.Severity == "Medium");
            var low = _lastAnomalies.Count(a => a.Severity == "Low");
            var unknownDevices = _lastDevices.Count(d => d.MacAddress == "Unknown");

            var sb = new StringBuilder();
            sb.AppendLine("{");
            sb.AppendLine($"  \"capturedAt\": \"{DateTime.Now:yyyy-MM-ddTHH:mm:ss}\",");
            sb.AppendLine($"  \"postureScore\": {score},");
            sb.AppendLine($"  \"deviceCount\": {_lastDevices.Count},");
            sb.AppendLine($"  \"anomalies\": {{ \"high\": {high}, \"medium\": {medium}, \"low\": {low} }},");
            sb.AppendLine($"  \"unknownDevices\": {unknownDevices},");
            sb.AppendLine("  \"topFindings\": [");

            var top = _lastAnomalies
                .OrderByDescending(a => SeverityRank(a.Severity))
                .ThenBy(a => a.Type)
                .Take(5)
                .ToList();

            for (var i = 0; i < top.Count; i++)
            {
                var finding = top[i];
                var comma = i < top.Count - 1 ? "," : "";
                sb.AppendLine(
                    $"    {{ \"severity\": \"{finding.Severity}\", \"type\": \"{finding.Type}\", \"deviceIp\": \"{finding.DeviceIp}\" }}{comma}");
            }

            sb.AppendLine("  ]");
            sb.AppendLine("}");

            Clipboard.SetText(sb.ToString());
            TxtTriageSummary.Text = "Incident snapshot copied to clipboard.";
            MessageBox.Show("Incident snapshot copied to clipboard.",
                            "Incident Snapshot", MessageBoxButton.OK, MessageBoxImage.Information);
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
