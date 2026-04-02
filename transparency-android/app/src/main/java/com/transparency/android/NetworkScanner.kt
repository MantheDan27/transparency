package com.transparency.android

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.wifi.WifiManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.withContext
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import java.time.Instant

class NetworkScanner(private val context: Context) {

    private val wifiManager by lazy {
        context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
    }

    fun getWifiInfo(): Pair<String, String> {
        val info = wifiManager.connectionInfo
        val ssid = info.ssid?.replace("\"", "") ?: "Unknown"
        val bssid = info.bssid ?: ""
        return Pair(ssid, bssid)
    }

    fun getGatewayIp(): String {
        val dhcp = wifiManager.dhcpInfo
        val gw = dhcp.gateway
        return "${gw and 0xff}.${gw shr 8 and 0xff}.${gw shr 16 and 0xff}.${gw shr 24 and 0xff}"
    }

    fun getLocalSubnet(): String {
        val dhcp = wifiManager.dhcpInfo
        val ip = dhcp.ipAddress
        return "${ip and 0xff}.${ip shr 8 and 0xff}.${ip shr 16 and 0xff}"
    }

    suspend fun quickScan(): ScanResult = withContext(Dispatchers.IO) {
        val start = System.currentTimeMillis()
        val subnet = getLocalSubnet()
        val (ssid, bssid) = getWifiInfo()
        val gateway = getGatewayIp()

        val devices = coroutineScope {
            (1..254).map { i ->
                async {
                    val ip = "$subnet.$i"
                    pingHost(ip, 300)
                }
            }.awaitAll().filterNotNull()
        }

        ScanResult(
            devices = devices,
            gateway = gateway,
            ssid = ssid,
            bssid = bssid,
            durationMs = System.currentTimeMillis() - start
        )
    }

    suspend fun deepScan(): ScanResult = withContext(Dispatchers.IO) {
        val start = System.currentTimeMillis()
        val subnet = getLocalSubnet()
        val (ssid, bssid) = getWifiInfo()
        val gateway = getGatewayIp()

        // Phase 1: ICMP ping sweep
        val reachableIps = coroutineScope {
            (1..254).map { i ->
                async {
                    val ip = "$subnet.$i"
                    if (isReachable(ip, 500)) ip else null
                }
            }.awaitAll().filterNotNull()
        }

        // Phase 2: TCP probe common ports on reachable hosts
        val commonPorts = listOf(22, 53, 80, 443, 445, 554, 8080, 8443, 9100)
        val devices = coroutineScope {
            reachableIps.map { ip ->
                async {
                    val openPorts = commonPorts.filter { port -> tcpProbe(ip, port, 400) }
                    val hostname = resolveHostname(ip)
                    val latency = measureLatency(ip)
                    Device(
                        ip = ip,
                        hostname = hostname,
                        openPorts = openPorts,
                        latencyMs = latency,
                        deviceType = classifyDevice(openPorts),
                        confidence = if (openPorts.isNotEmpty()) 75 else 40,
                        firstSeen = Instant.now(),
                        lastSeen = Instant.now()
                    )
                }
            }.awaitAll()
        }

        ScanResult(
            devices = devices,
            gateway = gateway,
            ssid = ssid,
            bssid = bssid,
            durationMs = System.currentTimeMillis() - start
        )
    }

    private fun pingHost(ip: String, timeoutMs: Int): Device? {
        return try {
            val addr = InetAddress.getByName(ip)
            if (addr.isReachable(timeoutMs)) {
                Device(
                    ip = ip,
                    hostname = if (addr.canonicalHostName != ip) addr.canonicalHostName else "",
                    latencyMs = measureLatency(ip),
                    firstSeen = Instant.now(),
                    lastSeen = Instant.now()
                )
            } else null
        } catch (_: Exception) { null }
    }

    private fun isReachable(ip: String, timeoutMs: Int): Boolean {
        return try {
            InetAddress.getByName(ip).isReachable(timeoutMs)
        } catch (_: Exception) { false }
    }

    private fun tcpProbe(ip: String, port: Int, timeoutMs: Int): Boolean {
        return try {
            Socket().use { socket ->
                socket.connect(InetSocketAddress(ip, port), timeoutMs)
                true
            }
        } catch (_: Exception) { false }
    }

    private fun resolveHostname(ip: String): String {
        return try {
            val addr = InetAddress.getByName(ip)
            if (addr.canonicalHostName != ip) addr.canonicalHostName else ""
        } catch (_: Exception) { "" }
    }

    private fun measureLatency(ip: String): Long {
        return try {
            val start = System.nanoTime()
            InetAddress.getByName(ip).isReachable(500)
            (System.nanoTime() - start) / 1_000_000
        } catch (_: Exception) { -1 }
    }

    private fun classifyDevice(ports: List<Int>): String {
        return when {
            ports.containsAll(listOf(80, 443)) && ports.contains(22) -> "Server"
            ports.contains(554) -> "IP Camera"
            ports.contains(9100) -> "Printer"
            ports.contains(445) -> "PC/NAS"
            ports.containsAll(listOf(80, 443)) -> "Web Device"
            ports.contains(53) -> "Router/DNS"
            ports.contains(22) -> "Linux/IoT"
            ports.contains(8080) || ports.contains(8443) -> "Smart Device"
            else -> "Unknown"
        }
    }

    suspend fun ping(host: String, count: Int = 4): String = withContext(Dispatchers.IO) {
        try {
            val process = Runtime.getRuntime().exec(arrayOf("ping", "-c", "$count", host))
            val reader = BufferedReader(InputStreamReader(process.inputStream))
            val output = reader.readText()
            process.waitFor()
            output.ifBlank { "No response from $host" }
        } catch (e: Exception) {
            "Ping failed: ${e.message}"
        }
    }

    suspend fun dnsLookup(host: String): String = withContext(Dispatchers.IO) {
        try {
            val addresses = InetAddress.getAllByName(host)
            addresses.joinToString("\n") { "${it.hostAddress}  ${it.canonicalHostName}" }
        } catch (e: Exception) {
            "DNS lookup failed: ${e.message}"
        }
    }

    suspend fun traceroute(host: String): String = withContext(Dispatchers.IO) {
        try {
            val process = Runtime.getRuntime().exec(arrayOf("traceroute", "-m", "15", host))
            val reader = BufferedReader(InputStreamReader(process.inputStream))
            val output = reader.readText()
            process.waitFor()
            output.ifBlank { "No traceroute output" }
        } catch (e: Exception) {
            "Traceroute failed: ${e.message}"
        }
    }

    suspend fun portScan(host: String, ports: List<Int> = (1..1024).toList()): String =
        withContext(Dispatchers.IO) {
            val open = coroutineScope {
                ports.map { port ->
                    async { if (tcpProbe(host, port, 300)) port else null }
                }.awaitAll().filterNotNull()
            }
            if (open.isEmpty()) "No open ports found on $host"
            else "Open ports on $host:\n${open.joinToString("\n") { "  $it/tcp  open" }}"
        }
}
