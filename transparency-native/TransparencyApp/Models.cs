using System;
using System.Collections.Generic;

namespace TransparencyApp.Models
{
    public class Device
    {
        public string IpAddress { get; set; } = "";
        public string MacAddress { get; set; } = "Unknown";
        public string Name { get; set; } = "Unknown Device";
        public List<int> OpenPorts { get; set; } = new List<int>();
    }

    public class Anomaly
    {
        public string Type { get; set; } = "";
        public string Severity { get; set; } = "Low"; // Low, Medium, High
        public string DeviceIp { get; set; } = "";
        public string Description { get; set; } = "";
    }

    public class LedgerEntry
    {
        public DateTime Timestamp { get; set; }
        public string Action { get; set; } = "";
        public string Details { get; set; } = "";
    }
}
