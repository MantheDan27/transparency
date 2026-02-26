using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading.Tasks;
using TransparencyApp.Models;

namespace TransparencyApp.Services
{
    public class NetworkScanner
    {
        private List<Device> _lastSnapshot = new List<Device>();

        public async Task<(List<Device> devices, List<Anomaly> anomalies)> ScanAsync()
        {
            var devices = await DiscoverDevicesAsync();
            var anomalies = AnalyzeAnomalies(devices);
            _lastSnapshot = devices;
            return (devices, anomalies);
        }

        private async Task<List<Device>> DiscoverDevicesAsync()
        {
            // Simulate discovery logic (Ping + TCP probe fallback)
            // In a real native app, we'd use ARP and Ping sweeps
            await Task.Delay(2000); // Simulate network latency

            return new List<Device>
            {
                new Device { IpAddress = "192.168.1.1", MacAddress = "00:11:22:33:44:55", Name = "Gateway", OpenPorts = new List<int>{ 80, 443 } },
                new Device { IpAddress = "192.168.1.10", MacAddress = "AA:BB:CC:DD:EE:FF", Name = "Work Laptop", OpenPorts = new List<int>{ 22, 445 } },
                new Device { IpAddress = "192.168.1.15", MacAddress = "Unknown", Name = "IoT Camera", OpenPorts = new List<int>{ 8080, 23 } }
            };
        }

        private List<Anomaly> AnalyzeAnomalies(List<Device> currentDevices)
        {
            var anomalies = new List<Anomaly>();
            var riskyPorts = new[] { 23, 445, 3389 };

            foreach (var device in currentDevices)
            {
                // Risky Ports
                var exposed = device.OpenPorts.Intersect(riskyPorts).ToList();
                if (exposed.Any())
                {
                    anomalies.Add(new Anomaly {
                        Type = "Risky Port",
                        Severity = "High",
                        DeviceIp = device.IpAddress,
                        Description = $"Exposed risky ports: {string.Join(", ", exposed)}"
                    });
                }

                // Unknown MAC
                if (device.MacAddress == "Unknown")
                {
                    anomalies.Add(new Anomaly {
                        Type = "Unknown MAC",
                        Severity = "Medium",
                        DeviceIp = device.IpAddress,
                        Description = "Could not resolve MAC address for this device."
                    });
                }

                // New Device vs Snapshot
                if (_lastSnapshot.Count > 0 && !_lastSnapshot.Any(d => d.IpAddress == device.IpAddress))
                {
                    anomalies.Add(new Anomaly {
                        Type = "New Device",
                        Severity = "Low",
                        DeviceIp = device.IpAddress,
                        Description = "New device joined the network since last scan."
                    });
                }
            }

            return anomalies;
        }
    }

    public class CloudService
    {
        private List<LedgerEntry> _ledger = new List<LedgerEntry>();
        public IReadOnlyList<LedgerEntry> Ledger => _ledger;

        public async Task<string> EnrichDeviceAsync(Device device)
        {
            await Task.Delay(500);
            _ledger.Add(new LedgerEntry { 
                Timestamp = DateTime.Now, 
                Action = "ENRICH", 
                Details = $"Metadata for {device.IpAddress} sent to cloud." 
            });

            return $"Defensive Guidance: Ensure that services running on ports {string.Join(", ", device.OpenPorts)} are updated and authenticated.";
        }

        public async Task DeleteCloudDataAsync()
        {
            await Task.Delay(800);
            _ledger.Clear();
            _ledger.Add(new LedgerEntry { 
                Timestamp = DateTime.Now, 
                Action = "PURGE", 
                Details = "All user cloud history deleted." 
            });
        }
    }
}
