using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using TransparencyApp.Models;

namespace TransparencyApp.Services
{
    // ─── Port catalogue ────────────────────────────────────────────────────────
    internal static class PortInfo
    {
        public static readonly int[] CommonPorts =
        {
            21, 22, 23, 25, 53, 80, 110, 135, 139, 143,
            443, 445, 515, 587, 1433, 1723, 3306, 3389, 5900, 8080, 8443
        };

        public static readonly int[] RiskyPorts =
        {
            21, 23, 135, 139, 445, 1433, 1723, 3306, 3389, 5900
        };

        public static string ServiceName(int port) => port switch
        {
            21   => "FTP",
            22   => "SSH",
            23   => "Telnet",
            25   => "SMTP",
            53   => "DNS",
            80   => "HTTP",
            110  => "POP3",
            135  => "MS-RPC",
            139  => "NetBIOS",
            143  => "IMAP",
            443  => "HTTPS",
            445  => "SMB",
            515  => "LPD",
            587  => "SMTP/TLS",
            1433 => "MSSQL",
            1723 => "PPTP",
            3306 => "MySQL",
            3389 => "RDP",
            5900 => "VNC",
            8080 => "HTTP-Alt",
            8443 => "HTTPS-Alt",
            _    => $"Port {port}"
        };
    }

    // ─── Network Scanner ───────────────────────────────────────────────────────
    public class NetworkScanner
    {
        private List<Device> _lastSnapshot = new();
        private readonly SemaphoreSlim _concLimit = new(60);

        public async Task<(List<Device> devices, List<Anomaly> anomalies)> ScanAsync(
            IEnumerable<NetworkInterface>? interfaces = null,
            IProgress<string>? progress = null)
        {
            progress?.Report("Resolving local subnet…");
            var baseIp = GetLocalSubnet(interfaces);

            progress?.Report($"Pinging {baseIp}.1–254…");
            var liveHosts = await PingSweepAsync(baseIp);

            progress?.Report($"Discovered {liveHosts.Count} host(s) — reading ARP table…");
            var arpTable = await GetArpTableAsync();

            progress?.Report($"Scanning ports on {liveHosts.Count} host(s)…");
            var hostTasks = liveHosts.Select(ip => ScanHostAsync(ip, arpTable)).ToList();
            var devices   = (await Task.WhenAll(hostTasks))
                                .Where(d => d is not null)
                                .Select(d => d!)
                                .ToList();

            progress?.Report("Analyzing anomalies…");
            var anomalies = AnalyzeAnomalies(devices);
            _lastSnapshot = devices;

            progress?.Report($"Done — {devices.Count} device(s), {anomalies.Count} anomaly(ies).");
            return (devices, anomalies);
        }

        // ── Subnet discovery ──────────────────────────────────────────────────
        internal static string GetLocalSubnet(IEnumerable<NetworkInterface>? interfaces = null)
        {
            interfaces ??= NetworkInterface.GetAllNetworkInterfaces();

            foreach (var iface in interfaces)
            {
                if (iface.OperationalStatus != OperationalStatus.Up)      continue;
                if (iface.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;

                foreach (var addr in iface.GetIPProperties().UnicastAddresses)
                {
                    if (addr.Address.AddressFamily != AddressFamily.InterNetwork) continue;
                    var ip = addr.Address.ToString();
                    if (ip.StartsWith("169.254")) continue; // skip APIPA
                    return string.Join(".", ip.Split('.').Take(3));
                }
            }
            return "192.168.1";
        }

        // ── Ping sweep ────────────────────────────────────────────────────────
        private static async Task<List<string>> PingSweepAsync(string baseIp)
        {
            var tasks = Enumerable.Range(1, 254)
                                  .Select(i => PingHostAsync($"{baseIp}.{i}"))
                                  .ToList();
            var results = await Task.WhenAll(tasks);
            return results.Where(ip => ip is not null).Select(ip => ip!).ToList();
        }

        private static async Task<string?> PingHostAsync(string ip)
        {
            try
            {
                using var ping  = new Ping();
                var reply = await ping.SendPingAsync(ip, 600);
                return reply.Status == IPStatus.Success ? ip : null;
            }
            catch { return null; }
        }

        // ── ARP table ─────────────────────────────────────────────────────────
        private static async Task<Dictionary<string, string>> GetArpTableAsync()
        {
            var table = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            try
            {
                var output = await Task.Run(() =>
                {
                    var psi = new ProcessStartInfo("arp", "-a")
                    {
                        RedirectStandardOutput = true,
                        UseShellExecute  = false,
                        CreateNoWindow   = true
                    };
                    using var proc = Process.Start(psi);
                    return proc?.StandardOutput.ReadToEnd() ?? "";
                });

                foreach (var line in output.Split('\n'))
                {
                    var parts = line.Trim()
                                    .Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                    if (parts.Length < 2) continue;
                    if (!IPAddress.TryParse(parts[0], out _)) continue;

                    var mac = parts[1].Replace('-', ':').ToUpper();
                    if (mac.Length == 17 && mac.Contains(':'))
                        table[parts[0]] = mac;
                }
            }
            catch { /* ARP unavailable — fall back to Unknown */ }
            return table;
        }

        // ── Host scan (ports + DNS) ───────────────────────────────────────────
        private async Task<Device?> ScanHostAsync(string ip, Dictionary<string, string> arpTable)
        {
            var device = new Device { IpAddress = ip };

            device.MacAddress = arpTable.TryGetValue(ip, out var mac) ? mac : "Unknown";

            // Reverse DNS
            try
            {
                var entry = await Dns.GetHostEntryAsync(ip);
                // Trim trailing dot that some resolvers return
                device.Hostname = entry.HostName.TrimEnd('.');
            }
            catch { device.Hostname = ""; }

            // TCP port probe
            var portTasks  = PortInfo.CommonPorts.Select(p => ProbePortAsync(ip, p)).ToList();
            var portOpen   = await Task.WhenAll(portTasks);
            device.OpenPorts = PortInfo.CommonPorts
                                       .Zip(portOpen, (port, open) => (port, open))
                                       .Where(x => x.open)
                                       .Select(x => x.port)
                                       .ToList();

            // Carry forward previous ports for diff
            var prev = _lastSnapshot.FirstOrDefault(d => d.IpAddress == ip);
            device.PreviousPorts = prev?.OpenPorts.ToList() ?? new List<int>();

            return device;
        }

        private async Task<bool> ProbePortAsync(string ip, int port)
        {
            await _concLimit.WaitAsync();
            try
            {
                using var client = new TcpClient();
                var connect = client.ConnectAsync(ip, port);
                if (await Task.WhenAny(connect, Task.Delay(350)) != connect) return false;
                return !connect.IsFaulted && client.Connected;
            }
            catch { return false; }
            finally { _concLimit.Release(); }
        }

        // ── Anomaly analysis ─────────────────────────────────────────────────
        private List<Anomaly> AnalyzeAnomalies(List<Device> current)
        {
            var anomalies = new List<Anomaly>();

            foreach (var device in current)
            {
                // Rule 1 — Risky ports
                foreach (var port in device.OpenPorts.Intersect(PortInfo.RiskyPorts))
                    anomalies.Add(BuildRiskyPortAnomaly(device, port));

                // Rule 2 — Unknown MAC
                if (device.MacAddress == "Unknown")
                    anomalies.Add(new Anomaly
                    {
                        Type        = "Unknown MAC Address",
                        Severity    = "Medium",
                        DeviceIp    = device.IpAddress,
                        Description = $"{device.IpAddress} has no resolvable MAC address in the ARP table.",
                        Explanation = "MAC addresses are hardware identifiers assigned to network interfaces. "
                                    + "An unknown MAC may indicate MAC address randomization (common on modern phones), "
                                    + "a very recently joined device, or a device that has not communicated "
                                    + "directly with this host. Without a stable MAC the device cannot be reliably "
                                    + "tracked or fingerprinted across scans.",
                        Remediation = "Open your router's DHCP client list to cross-reference the IP with a "
                                    + "registered device. If the device is unrecognised, isolate it on a guest "
                                    + "VLAN or block it via your router's MAC-based access control list.",
                        TraceSource  = "Ping sweep → ARP table lookup → MAC field empty",
                        AffectedPorts = new List<int>()
                    });

                // Rule 3 — New device vs snapshot
                if (_lastSnapshot.Count > 0 && !_lastSnapshot.Any(d => d.IpAddress == device.IpAddress))
                    anomalies.Add(new Anomaly
                    {
                        Type        = "New Device Detected",
                        Severity    = "Low",
                        DeviceIp    = device.IpAddress,
                        Description = $"{device.IpAddress} ({device.DisplayName}) appeared since the last scan.",
                        Explanation = "A device has joined your network between the previous and current scan. "
                                    + "This is often benign (a phone reconnecting, a guest device), but "
                                    + "unrecognised new devices can also indicate unauthorised network access.",
                        Remediation = "Verify the device belongs to a known user. Check your router's DHCP "
                                    + "lease log for the full MAC address and manufacturer prefix. If "
                                    + "unrecognised, consider enabling device allowlisting on your router.",
                        TraceSource  = "Ping sweep → IP not present in previous snapshot",
                        AffectedPorts = new List<int>()
                    });

                // Rule 4 — Ports changed vs snapshot
                if (device.PreviousPorts.Count > 0)
                {
                    var newlyOpen = device.OpenPorts.Except(device.PreviousPorts).ToList();
                    if (newlyOpen.Any())
                        anomalies.Add(new Anomaly
                        {
                            Type        = "New Open Ports Detected",
                            Severity    = "Medium",
                            DeviceIp    = device.IpAddress,
                            Description = $"{device.IpAddress} has newly opened port(s): "
                                        + string.Join(", ", newlyOpen.Select(p =>
                                              $"{p} ({PortInfo.ServiceName(p)})")),
                            Explanation = "Ports that were previously closed are now accepting connections. "
                                        + "This may indicate a new service was started, software was installed, "
                                        + "or a firewall rule was removed. Unexpected open ports are potential "
                                        + "entry points for attackers.",
                            Remediation = "Identify the process bound to each new port (use 'netstat -ano' or "
                                        + "Task Manager → Resource Monitor → Network). Disable or firewall any "
                                        + "service that should not be publicly accessible.",
                            TraceSource  = "TCP probe scan → comparison with previous snapshot port list",
                            AffectedPorts = newlyOpen
                        });
                }
            }

            return anomalies;
        }

        // ── Risky-port anomaly builder ────────────────────────────────────────
        private static Anomaly BuildRiskyPortAnomaly(Device device, int port)
        {
            var (shortName, explanation, remediation) = port switch
            {
                21  => ("FTP Exposed",
                        "FTP transmits credentials and file data in plain text. Any device on the same network "
                      + "can capture a full session, including usernames and passwords, using basic packet capture.",
                        "Disable FTP. Migrate to SFTP (SSH file transfer) or FTPS (FTP over TLS). "
                      + "If the device is embedded, check firmware for an SFTP option."),

                23  => ("Telnet Exposed",
                        "Telnet sends every keystroke — including passwords — over the wire with no encryption. "
                      + "It is trivially captured with tools available on any OS.",
                        "Disable the Telnet service immediately. Use SSH for all remote administration. "
                      + "For IoT/embedded devices, update firmware or replace the device."),

                135 => ("MS-RPC Exposed",
                        "Microsoft RPC (port 135) has been the vector for several critical worms (Blaster, Sasser). "
                      + "It enables DCOM object activation and is a common pivot point in lateral-movement attacks.",
                        "Block port 135 at the host firewall (Windows Defender Firewall). "
                      + "Disable DCOM if not required via Component Services → Computers → My Computer → Properties."),

                139 => ("NetBIOS Session Service Exposed",
                        "NetBIOS (port 139) exposes legacy Windows file and print sharing. It enables NTLM relay "
                      + "attacks and NetBIOS Name Service spoofing, which can lead to credential theft.",
                        "Disable NetBIOS over TCP/IP: Network Adapter → Properties → TCP/IP → Advanced → WINS "
                      + "→ Disable NetBIOS over TCP/IP. Migrate file sharing to SMBv3 on port 445 with signing."),

                445 => ("SMB Exposed",
                        "SMB (port 445) was exploited by EternalBlue to spread WannaCry and NotPetya ransomware "
                      + "across millions of systems. It is actively scanned for by automated exploit frameworks.",
                        "Disable SMBv1 (it is enabled by default on older Windows). Apply all Windows Updates. "
                      + "Block port 445 at the network perimeter. Enable SMB signing to prevent relay attacks."),

                1433 => ("MSSQL Database Exposed",
                         "A network-accessible SQL Server is a high-value target. Automated tools can brute-force "
                       + "the 'sa' account and use xp_cmdshell to achieve OS-level command execution.",
                         "Bind SQL Server to localhost (127.0.0.1) only. Disable the 'sa' account. "
                       + "Use Windows Authentication. Apply all SQL Server security patches."),

                1723 => ("PPTP VPN Exposed",
                         "PPTP is a deprecated VPN protocol. Its MS-CHAPv2 authentication can be fully broken "
                       + "offline in under 24 hours using commodity cloud computing resources.",
                         "Decommission PPTP. Migrate to WireGuard, OpenVPN (with TLS certificates), or IKEv2/IPSec."),

                3306 => ("MySQL Database Exposed",
                         "A network-accessible MySQL server can be brute-forced or exploited via unauthenticated "
                       + "CVEs. Production databases must never be directly reachable from untrusted networks.",
                         "Edit /etc/mysql/mysql.conf.d/mysqld.cnf: set bind-address = 127.0.0.1. "
                       + "Create dedicated application users with minimal GRANT privileges. Patch MySQL."),

                3389 => ("RDP Exposed",
                         "Remote Desktop Protocol is the most targeted service on the internet. "
                       + "Exposed RDP is scanned by automated botnets within minutes of IP assignment "
                       + "and is the primary delivery vector for ransomware operators.",
                         "Disable direct public RDP access. Access remotely only via VPN or an RD Gateway with MFA. "
                       + "Enable Network Level Authentication (NLA). Enforce account lockout policies."),

                5900 => ("VNC Exposed",
                         "VNC provides full graphical desktop control. It is commonly deployed with weak or no "
                       + "passwords and is actively exploited in targeted attacks and ransomware campaigns.",
                         "Disable VNC if not actively needed. If required, bind to localhost and tunnel over SSH. "
                       + "Enforce a strong VNC password and restrict access by firewall rule."),

                _   => ($"Port {port} Exposed",
                        $"Port {port} ({PortInfo.ServiceName(port)}) is flagged based on its service association.",
                        "Review what service is running on this port and ensure it requires authentication "
                      + "and is not unnecessarily exposed to the network.")
            };

            return new Anomaly
            {
                Type          = $"Risky Port — {shortName}",
                Severity      = "High",
                DeviceIp      = device.IpAddress,
                Description   = $"{device.IpAddress} ({device.DisplayName}) is exposing port {port} "
                              + $"({PortInfo.ServiceName(port)}).",
                Explanation   = explanation,
                Remediation   = remediation,
                TraceSource   = $"Ping sweep → TCP probe → port {port} ({PortInfo.ServiceName(port)}) accepted connection",
                AffectedPorts = new List<int> { port }
            };
        }
    }

    // ─── Cloud Mock Service ────────────────────────────────────────────────────
    public class CloudMockService
    {
        private readonly List<LedgerEntry> _ledger = new();
        public IReadOnlyList<LedgerEntry> Ledger => _ledger;

        public async Task<string> EnrichDeviceAsync(Device device)
        {
            await Task.Delay(300);
            _ledger.Add(new LedgerEntry
            {
                Timestamp = DateTime.Now,
                Action    = "ENRICH",
                Details   = $"{device.IpAddress} ({device.DisplayName})  |  Ports: {device.OpenPortsDisplay}"
            });

            return $"Defensive Guidance — {device.IpAddress}\n\n"
                 + $"Hostname : {(string.IsNullOrEmpty(device.Hostname) ? "Unresolved" : device.Hostname)}\n"
                 + $"MAC      : {device.MacAddress}\n"
                 + $"Ports    : {device.OpenPortsDisplay}\n\n"
                 + "Recommendations:\n"
                 + "• Patch all services exposed on open ports.\n"
                 + "• Disable any service not explicitly required.\n"
                 + "• Use strong, unique credentials for all remote-access services.\n"
                 + "• Consider segmenting IoT or guest devices onto a dedicated VLAN.";
        }

        public async Task DeleteCloudDataAsync()
        {
            await Task.Delay(400);
            _ledger.Clear();
            _ledger.Add(new LedgerEntry
            {
                Timestamp = DateTime.Now,
                Action    = "PURGE",
                Details   = "All device history permanently deleted from cloud storage."
            });
        }
    }
}
