using System;
using System.Collections.Generic;

namespace TransparencyApp.Models
{
    public class Device
    {
        public string IpAddress  { get; set; } = "";
        public string MacAddress { get; set; } = "Unknown";
        public string Hostname   { get; set; } = "";
        public List<int> OpenPorts { get; set; } = new();
        public List<int> PreviousPorts { get; set; } = new();

        // Computed display helpers
        public string DisplayName    => !string.IsNullOrEmpty(Hostname) ? Hostname : IpAddress;
        public string OpenPortsDisplay => OpenPorts.Count > 0
            ? string.Join(", ", OpenPorts)
            : "None detected";
    }

    public class Anomaly
    {
        public string Type        { get; set; } = "";
        public string Severity    { get; set; } = "Low";   // High | Medium | Low
        public string DeviceIp    { get; set; } = "";
        public string Description { get; set; } = "";
        public string Explanation { get; set; } = "";
        public string Remediation { get; set; } = "";
        public string TraceSource { get; set; } = "";
        public List<int> AffectedPorts { get; set; } = new();

        public string AffectedPortsDisplay => AffectedPorts.Count > 0
            ? string.Join(", ", AffectedPorts)
            : "N/A";
    }

    public class LedgerEntry
    {
        public DateTime Timestamp { get; set; }
        public string Action  { get; set; } = "";
        public string Details { get; set; } = "";
    }
}
