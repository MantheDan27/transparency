using System.Linq;
using System.Threading.Tasks;
using TransparencyApp.Models;
using TransparencyApp.Services;
using Xunit;

namespace TransparencyApp.Tests
{
    public class CloudMockServiceTests
    {
        [Fact]
        public async Task EnrichDeviceAsync_WithValidDevice_ReturnsFormattedStringAndUpdatesLedger()
        {
            // Arrange
            var service = new CloudMockService();
            var device = new Device
            {
                IpAddress = "192.168.1.100",
                MacAddress = "00:1A:2B:3C:4D:5E",
                Hostname = "TestPC",
                OpenPorts = { 80, 443 }
            };

            // Act
            var result = await service.EnrichDeviceAsync(device);

            // Assert
            Assert.Contains("Defensive Guidance — 192.168.1.100", result);
            Assert.Contains("Hostname : TestPC", result);
            Assert.Contains("MAC      : 00:1A:2B:3C:4D:5E", result);
            Assert.Contains("Ports    : 80, 443", result);

            Assert.Single(service.Ledger);
            var entry = service.Ledger.First();
            Assert.Equal("ENRICH", entry.Action);
            Assert.Equal("192.168.1.100 (TestPC)  |  Ports: 80, 443", entry.Details);
        }

        [Fact]
        public async Task EnrichDeviceAsync_WithEmptyHostname_UsesIpAddressAsDisplayName()
        {
            // Arrange
            var service = new CloudMockService();
            var device = new Device
            {
                IpAddress = "10.0.0.5",
                MacAddress = "AA:BB:CC:DD:EE:FF",
                Hostname = "", // Empty hostname
                OpenPorts = { 22 }
            };

            // Act
            var result = await service.EnrichDeviceAsync(device);

            // Assert
            Assert.Contains("Hostname : Unresolved", result);

            Assert.Single(service.Ledger);
            var entry = service.Ledger.First();
            // DisplayName should fall back to IpAddress
            Assert.Equal("10.0.0.5 (10.0.0.5)  |  Ports: 22", entry.Details);
        }

        [Fact]
        public async Task EnrichDeviceAsync_WithNoOpenPorts_FormatsPortsAsNoneDetected()
        {
            // Arrange
            var service = new CloudMockService();
            var device = new Device
            {
                IpAddress = "172.16.0.50",
                MacAddress = "11:22:33:44:55:66",
                Hostname = "SecureDevice",
                // OpenPorts is empty by default
            };

            // Act
            var result = await service.EnrichDeviceAsync(device);

            // Assert
            Assert.Contains("Ports    : None detected", result);

            Assert.Single(service.Ledger);
            var entry = service.Ledger.First();
            Assert.Equal("172.16.0.50 (SecureDevice)  |  Ports: None detected", entry.Details);
        }
    }
}
