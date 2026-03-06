using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Xunit;
using TransparencyApp.Services;

namespace TransparencyApp.Tests
{
    public class NetworkScannerTests
    {
        // Subclass to mock the ARP command execution
        private class TestNetworkScanner : NetworkScanner
        {
            public string MockArpOutput { get; set; } = "";
            public bool ThrowOnArp { get; set; } = false;

            protected internal override Task<string> RunArpCommandAsync()
            {
                if (ThrowOnArp)
                {
                    throw new Exception("Simulated ARP command failure");
                }
                return Task.FromResult(MockArpOutput);
            }
        }

        [Fact]
        public async Task GetArpTableAsync_ValidOutput_ParsesCorrectly()
        {
            // Arrange
            var scanner = new TestNetworkScanner
            {
                MockArpOutput = @"
Interface: 192.168.1.10 --- 0x12
  Internet Address      Physical Address      Type
  192.168.1.1           00-11-22-33-44-55     dynamic
  192.168.1.100         aa-bb-cc-dd-ee-ff     dynamic
"
            };

            // Act
            var table = await scanner.GetArpTableAsync();

            // Assert
            Assert.Equal(2, table.Count);
            Assert.True(table.ContainsKey("192.168.1.1"));
            Assert.Equal("00:11:22:33:44:55", table["192.168.1.1"]);
            Assert.True(table.ContainsKey("192.168.1.100"));
            Assert.Equal("AA:BB:CC:DD:EE:FF", table["192.168.1.100"]);
        }

        [Fact]
        public async Task GetArpTableAsync_CommandThrows_ReturnsEmptyDictionaryAndDoesNotThrow()
        {
            // Arrange
            var scanner = new TestNetworkScanner
            {
                ThrowOnArp = true
            };

            // Act
            var table = await scanner.GetArpTableAsync();

            // Assert
            Assert.NotNull(table);
            Assert.Empty(table);
        }

        [Fact]
        public async Task GetArpTableAsync_EmptyOutput_ReturnsEmptyDictionary()
        {
            // Arrange
            var scanner = new TestNetworkScanner
            {
                MockArpOutput = ""
            };

            // Act
            var table = await scanner.GetArpTableAsync();

            // Assert
            Assert.NotNull(table);
            Assert.Empty(table);
        }

        [Fact]
        public async Task GetArpTableAsync_InvalidMac_SkipsEntry()
        {
            // Arrange
            var scanner = new TestNetworkScanner
            {
                MockArpOutput = @"
Interface: 192.168.1.10 --- 0x12
  Internet Address      Physical Address      Type
  192.168.1.1           00-11-22-33-44-55     dynamic
  192.168.1.2           invalid-mac           dynamic
  192.168.1.3           00-11                 dynamic
"
            };

            // Act
            var table = await scanner.GetArpTableAsync();

            // Assert
            Assert.Single(table);
            Assert.True(table.ContainsKey("192.168.1.1"));
        }
    }
}
