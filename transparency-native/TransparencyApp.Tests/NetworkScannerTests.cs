using System;
using System.Collections.Generic;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using Moq;
using TransparencyApp.Services;
using Xunit;

namespace TransparencyApp.Tests;

public class NetworkScannerTests
{
    // A mock network interface for testing
    public class TestNetworkInterface : NetworkInterface
    {
        private readonly string _id;
        private readonly string _name;
        private readonly OperationalStatus _status;
        private readonly NetworkInterfaceType _type;
        private readonly TestIPInterfaceProperties _properties;

        public TestNetworkInterface(string id, string name, OperationalStatus status, NetworkInterfaceType type, TestIPInterfaceProperties properties)
        {
            _id = id;
            _name = name;
            _status = status;
            _type = type;
            _properties = properties;
        }

        public override string Id => _id;
        public override string Name => _name;
        public override OperationalStatus OperationalStatus => _status;
        public override NetworkInterfaceType NetworkInterfaceType => _type;
        public override IPInterfaceProperties GetIPProperties() => _properties;

        // Stubs for other required abstract members
        public override string Description => "Test Interface";
        public override long Speed => 1000000000;
        public override bool IsReceiveOnly => false;
        public override bool SupportsMulticast => true;
        public override PhysicalAddress GetPhysicalAddress() => PhysicalAddress.None;
        public override bool Supports(NetworkInterfaceComponent networkInterfaceComponent) => false;
        public override IPv4InterfaceStatistics GetIPv4Statistics() => throw new System.NotImplementedException();
    }

    public class TestIPInterfaceProperties : IPInterfaceProperties
    {
        private readonly TestUnicastIPAddressInformationCollection _unicastAddresses;

        public TestIPInterfaceProperties(TestUnicastIPAddressInformationCollection unicastAddresses)
        {
            _unicastAddresses = unicastAddresses;
        }

        public override UnicastIPAddressInformationCollection UnicastAddresses => _unicastAddresses;

        // Other required abstract members
        public override bool IsDnsEnabled => false;
        public override string DnsSuffix => "";
        public override bool IsDynamicDnsEnabled => false;
        public override IPAddressInformationCollection AnycastAddresses => throw new System.NotImplementedException();
        public override MulticastIPAddressInformationCollection MulticastAddresses => throw new System.NotImplementedException();
        public override IPAddressCollection DnsAddresses => throw new System.NotImplementedException();
        public override GatewayIPAddressInformationCollection GatewayAddresses => throw new System.NotImplementedException();
        public override IPAddressCollection DhcpServerAddresses => throw new System.NotImplementedException();
        public override IPAddressCollection WinsServersAddresses => throw new System.NotImplementedException();
        public override IPv4InterfaceProperties GetIPv4Properties() => throw new System.NotImplementedException();
        public override IPv6InterfaceProperties GetIPv6Properties() => throw new System.NotImplementedException();
    }

    public class TestUnicastIPAddressInformationCollection : UnicastIPAddressInformationCollection
    {
        private readonly List<UnicastIPAddressInformation> _addresses = new List<UnicastIPAddressInformation>();

        public void AddInternal(UnicastIPAddressInformation address)
        {
            _addresses.Add(address);
        }

        public override int Count => _addresses.Count;
        public override bool IsReadOnly => false;

        public override IEnumerator<UnicastIPAddressInformation> GetEnumerator()
        {
            return _addresses.GetEnumerator();
        }

        public override void Add(UnicastIPAddressInformation address)
        {
            _addresses.Add(address);
        }
    }

    [Fact]
    public void GetLocalSubnet_ReturnsDefault_WhenNoInterfacesProvided()
    {
        var result = NetworkScanner.GetLocalSubnet(new List<NetworkInterface>());
        Assert.Equal("192.168.1", result);
    }

    [Fact]
    public void GetLocalSubnet_ReturnsDefault_WhenInterfaceIsDown()
    {
        var properties = new TestIPInterfaceProperties(new TestUnicastIPAddressInformationCollection());
        var mockInterface = new TestNetworkInterface("1", "Eth0", OperationalStatus.Down, NetworkInterfaceType.Ethernet, properties);

        var result = NetworkScanner.GetLocalSubnet(new[] { mockInterface });
        Assert.Equal("192.168.1", result);
    }

    [Fact]
    public void GetLocalSubnet_ReturnsDefault_WhenInterfaceIsLoopback()
    {
        var properties = new TestIPInterfaceProperties(new TestUnicastIPAddressInformationCollection());
        var mockInterface = new TestNetworkInterface("1", "lo", OperationalStatus.Up, NetworkInterfaceType.Loopback, properties);

        var result = NetworkScanner.GetLocalSubnet(new[] { mockInterface });
        Assert.Equal("192.168.1", result);
    }

    [Fact]
    public void GetLocalSubnet_SkipsIpv6Address()
    {
        var unicastCollection = new TestUnicastIPAddressInformationCollection();
        var mockIpv6Info = new Mock<UnicastIPAddressInformation>();
        mockIpv6Info.Setup(i => i.Address).Returns(IPAddress.Parse("fe80::1"));
        unicastCollection.Add(mockIpv6Info.Object);

        var properties = new TestIPInterfaceProperties(unicastCollection);
        var mockInterface = new TestNetworkInterface("1", "Eth0", OperationalStatus.Up, NetworkInterfaceType.Ethernet, properties);

        var result = NetworkScanner.GetLocalSubnet(new[] { mockInterface });
        Assert.Equal("192.168.1", result);
    }

    [Fact]
    public void GetLocalSubnet_SkipsApipaAddress()
    {
        var unicastCollection = new TestUnicastIPAddressInformationCollection();
        var mockApipaInfo = new Mock<UnicastIPAddressInformation>();
        mockApipaInfo.Setup(i => i.Address).Returns(IPAddress.Parse("169.254.10.5"));
        unicastCollection.Add(mockApipaInfo.Object);

        var properties = new TestIPInterfaceProperties(unicastCollection);
        var mockInterface = new TestNetworkInterface("1", "Eth0", OperationalStatus.Up, NetworkInterfaceType.Ethernet, properties);

        var result = NetworkScanner.GetLocalSubnet(new[] { mockInterface });
        Assert.Equal("192.168.1", result);
    }

    [Fact]
    public void GetLocalSubnet_ReturnsSubnet_ForValidIpv4Address()
    {
        var unicastCollection = new TestUnicastIPAddressInformationCollection();
        var mockIpv4Info = new Mock<UnicastIPAddressInformation>();
        mockIpv4Info.Setup(i => i.Address).Returns(IPAddress.Parse("10.0.0.50"));
        unicastCollection.Add(mockIpv4Info.Object);

        var properties = new TestIPInterfaceProperties(unicastCollection);
        var mockInterface = new TestNetworkInterface("1", "Eth0", OperationalStatus.Up, NetworkInterfaceType.Ethernet, properties);

        var result = NetworkScanner.GetLocalSubnet(new[] { mockInterface });
        Assert.Equal("10.0.0", result);
    }

    [Fact]
    public async System.Threading.Tasks.Task ScanAsync_DoesNotThrow_WhenUsingMockInterfaces()
    {
        var unicastCollection = new TestUnicastIPAddressInformationCollection();
        var mockIpv4Info = new Mock<UnicastIPAddressInformation>();
        mockIpv4Info.Setup(i => i.Address).Returns(IPAddress.Parse("10.0.0.50"));
        unicastCollection.Add(mockIpv4Info.Object);

        var properties = new TestIPInterfaceProperties(unicastCollection);
        var mockInterface = new TestNetworkInterface("1", "Eth0", OperationalStatus.Up, NetworkInterfaceType.Ethernet, properties);

        var scanner = new NetworkScanner();

        // Ensure that executing ScanAsync with mock interfaces doesn't throw because of network parsing
        var result = await scanner.ScanAsync(new[] { mockInterface });

        Assert.NotNull(result.devices);
        Assert.NotNull(result.anomalies);
    }
}
