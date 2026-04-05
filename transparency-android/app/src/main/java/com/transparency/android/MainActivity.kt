package com.transparency.android

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import com.transparency.android.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var vm: MainViewModel

    private val overviewFragment = OverviewFragment()
    private val devicesFragment = DevicesFragment()
    private val alertsFragment = AlertsFragment()
    private val toolsFragment = ToolsFragment()
    private var activeFragment: Fragment = overviewFragment

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { /* permissions granted or denied — scanning works either way, just less info */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        vm = ViewModelProvider(this)[MainViewModel::class.java]

        // Request permissions
        val needed = arrayOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_WIFI_STATE,
            Manifest.permission.CHANGE_WIFI_STATE
        ).filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }.toTypedArray()
        if (needed.isNotEmpty()) permissionLauncher.launch(needed)

        // Set up fragments
        supportFragmentManager.beginTransaction()
            .add(R.id.contentFrame, toolsFragment, "tools").hide(toolsFragment)
            .add(R.id.contentFrame, alertsFragment, "alerts").hide(alertsFragment)
            .add(R.id.contentFrame, devicesFragment, "devices").hide(devicesFragment)
            .add(R.id.contentFrame, overviewFragment, "overview")
            .commit()

        binding.bottomNav.setOnItemSelectedListener { item ->
            val target = when (item.itemId) {
                R.id.nav_overview -> overviewFragment
                R.id.nav_devices -> devicesFragment
                R.id.nav_alerts -> alertsFragment
                R.id.nav_tools -> toolsFragment
                else -> overviewFragment
            }
            switchFragment(target)
            true
        }

        // Show Wi-Fi info in top bar
        vm.ssid.observe(this) { ssid ->
            binding.wifiInfo.text = ssid
        }
    }

    private fun switchFragment(target: Fragment) {
        if (target == activeFragment) return
        supportFragmentManager.beginTransaction()
            .hide(activeFragment)
            .show(target)
            .commit()
        activeFragment = target
    }
}
