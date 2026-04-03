package com.transparency.android

import java.time.Instant

enum class TrustState { OWNED, KNOWN, GUEST, UNKNOWN, BLOCKED, WATCHLIST }

data class Device(
    val ip: String,
    val mac: String = "",
    val hostname: String = "",
    val vendor: String = "",
    val deviceType: String = "Unknown",
    val confidence: Int = 0,
    var trust: TrustState = TrustState.UNKNOWN,
    val openPorts: List<Int> = emptyList(),
    val latencyMs: Long = -1,
    val firstSeen: Instant = Instant.now(),
    val lastSeen: Instant = Instant.now(),
    var customName: String = "",
    var notes: String = ""
) {
    val displayName: String
        get() = customName.ifBlank { hostname.ifBlank { ip } }
}

enum class AlertSeverity { INFO, WARNING, CRITICAL }

data class Alert(
    val id: Long = System.currentTimeMillis(),
    val type: String,
    val title: String,
    val description: String,
    val severity: AlertSeverity = AlertSeverity.INFO,
    val timestamp: Instant = Instant.now(),
    val deviceIp: String = ""
)

data class ScanResult(
    val devices: List<Device>,
    val gateway: String,
    val ssid: String,
    val bssid: String,
    val durationMs: Long
)
