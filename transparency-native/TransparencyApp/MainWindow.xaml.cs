using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using TransparencyApp.Models;
using TransparencyApp.Services;

namespace TransparencyApp
{
    public partial class MainWindow : Window
    {
        private NetworkScanner _scanner = new NetworkScanner();
        private CloudService _cloud = new CloudService();

        public MainWindow()
        {
            InitializeComponent();
            DeviceGrid.ItemsSource = new List<Device>();
            AnomaliesList.ItemsSource = new List<Anomaly>();
        }

        private async void Scan_Click(object sender, RoutedEventArgs e)
        {
            LoadingOverlay.Visibility = Visibility.Visible;
            try
            {
                var (devices, anomalies) = await _scanner.ScanAsync();
                DeviceGrid.ItemsSource = devices;
                AnomaliesList.ItemsSource = anomalies;

                TxtTotalDevices.Text = devices.Count.ToString();
                TxtActiveThreats.Text = anomalies.Count.ToString();
            }
            finally
            {
                LoadingOverlay.Visibility = Visibility.Collapsed;
            }
        }

        private async void Enrich_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn && btn.DataContext is Device device)
            {
                var guidance = await _cloud.EnrichDeviceAsync(device);
                MessageBox.Show(guidance, $"Enrichment for {device.IpAddress}", MessageBoxButton.OK, MessageBoxImage.Information);
                UpdateLedger();
                TxtCloudSyncs.Text = _cloud.Ledger.Count(l => l.Action == "ENRICH").ToString();
            }
        }

        private async void DeleteData_Click(object sender, RoutedEventArgs e)
        {
            var result = MessageBox.Show(
                "Are you sure you want to delete all your history from the cloud?",
                "Confirm Deletion", MessageBoxButton.YesNo, MessageBoxImage.Warning);

            if (result == MessageBoxResult.Yes)
            {
                await _cloud.DeleteCloudDataAsync();
                MessageBox.Show("All cloud data has been deleted.", "Success", MessageBoxButton.OK, MessageBoxImage.Information);
                UpdateLedger();
            }
        }

        private void Nav_Click(object sender, RoutedEventArgs e)
        {
            if (sender is Button btn)
            {
                string target = btn.Tag.ToString()!;
                ViewDashboard.Visibility = target == "Dashboard" ? Visibility.Visible : Visibility.Collapsed;
                ViewAnomalies.Visibility = target == "Anomalies" ? Visibility.Visible : Visibility.Collapsed;
                ViewLedger.Visibility   = target == "Ledger"    ? Visibility.Visible : Visibility.Collapsed;
                ViewSettings.Visibility = target == "Settings"  ? Visibility.Visible : Visibility.Collapsed;

                if (target == "Ledger") UpdateLedger();
            }
        }

        private void UpdateLedger()
        {
            LedgerList.Items.Clear();
            foreach (var entry in _cloud.Ledger)
            {
                LedgerList.Items.Add($"[{entry.Timestamp:HH:mm:ss}] {entry.Action}: {entry.Details}");
            }
        }
    }
}
