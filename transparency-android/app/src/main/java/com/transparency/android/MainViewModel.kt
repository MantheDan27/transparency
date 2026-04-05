package com.transparency.android

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.launch

class MainViewModel(app: Application) : AndroidViewModel(app) {

    private val scanner = NetworkScanner(app)

    private val _devices = MutableLiveData<List<Device>>(emptyList())
    val devices: LiveData<List<Device>> = _devices

    private val _alerts = MutableLiveData<List<Alert>>(emptyList())
    val alerts: LiveData<List<Alert>> = _alerts

    private val _scanning = MutableLiveData(false)
    val scanning: LiveData<Boolean> = _scanning

    private val _gateway = MutableLiveData("--")
    val gateway: LiveData<String> = _gateway

    private val _ssid = MutableLiveData("")
    val ssid: LiveData<String> = _ssid

    private val _bssid = MutableLiveData("")
    val bssid: LiveData<String> = _bssid

    private val _toolOutput = MutableLiveData("")
    val toolOutput: LiveData<String> = _toolOutput

    fun quickScan() {
        if (_scanning.value == true) return
        _scanning.value = true
        viewModelScope.launch {
            try {
                val result = scanner.quickScan()
                processResult(result)
            } catch (e: Exception) {
                _toolOutput.value = "Scan failed: ${e.message}"
            } finally {
                _scanning.value = false
            }
        }
    }

    fun deepScan() {
        if (_scanning.value == true) return
        _scanning.value = true
        viewModelScope.launch {
            try {
                val result = scanner.deepScan()
                processResult(result)
            } catch (e: Exception) {
                _toolOutput.value = "Scan failed: ${e.message}"
            } finally {
                _scanning.value = false
            }
        }
    }

    private fun processResult(result: ScanResult) {
        val oldDevices = _devices.value.orEmpty().associateBy { it.ip }
        val merged = result.devices.map { new ->
            oldDevices[new.ip]?.let { old ->
                new.copy(
                    trust = old.trust,
                    customName = old.customName,
                    notes = old.notes,
                    firstSeen = old.firstSeen
                )
            } ?: new
        }
        // Generate alerts for new devices
        val newAlerts = mutableListOf<Alert>()
        merged.forEach { device ->
            if (device.ip !in oldDevices) {
                newAlerts.add(
                    Alert(
                        type = "new_device",
                        title = "New device discovered",
                        description = "${device.displayName} (${device.ip})",
                        severity = AlertSeverity.INFO,
                        deviceIp = device.ip
                    )
                )
            }
        }
        if (newAlerts.isNotEmpty()) {
            _alerts.value = newAlerts + _alerts.value.orEmpty()
        }
        _devices.value = merged
        _gateway.value = result.gateway
        _ssid.value = result.ssid
        _bssid.value = result.bssid
    }

    fun runPing(host: String) {
        viewModelScope.launch {
            _toolOutput.value = "Pinging $host...\n"
            _toolOutput.value = scanner.ping(host)
        }
    }

    fun runDns(host: String) {
        viewModelScope.launch {
            _toolOutput.value = "Looking up $host...\n"
            _toolOutput.value = scanner.dnsLookup(host)
        }
    }

    fun runTraceroute(host: String) {
        viewModelScope.launch {
            _toolOutput.value = "Tracing route to $host...\n"
            _toolOutput.value = scanner.traceroute(host)
        }
    }

    fun runPortScan(host: String) {
        viewModelScope.launch {
            _toolOutput.value = "Scanning ports on $host...\n"
            _toolOutput.value = scanner.portScan(host)
        }
    }

    fun clearAlerts() {
        _alerts.value = emptyList()
    }

    fun updateDeviceTrust(ip: String, trust: TrustState) {
        _devices.value = _devices.value?.map {
            if (it.ip == ip) it.copy(trust = trust) else it
        }
    }
}
