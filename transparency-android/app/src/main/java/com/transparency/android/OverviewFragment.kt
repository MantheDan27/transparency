package com.transparency.android

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.LinearLayoutManager
import com.transparency.android.databinding.FragmentOverviewBinding

class OverviewFragment : Fragment() {

    private var _binding: FragmentOverviewBinding? = null
    private val binding get() = _binding!!
    private val vm: MainViewModel by activityViewModels()
    private val adapter = DeviceAdapter()

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentOverviewBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.recentDevicesList.layoutManager = LinearLayoutManager(requireContext())
        binding.recentDevicesList.adapter = adapter

        binding.btnScanQuick.setOnClickListener { vm.quickScan() }
        binding.btnScanDeep.setOnClickListener { vm.deepScan() }

        vm.devices.observe(viewLifecycleOwner) { devices ->
            binding.kpiDeviceCount.text = devices.size.toString()
            adapter.submitList(devices.take(5))
            binding.emptyState.visibility = if (devices.isEmpty()) View.VISIBLE else View.GONE
            binding.recentDevicesList.visibility = if (devices.isEmpty()) View.GONE else View.VISIBLE
        }

        vm.gateway.observe(viewLifecycleOwner) { binding.kpiGateway.text = it }
        vm.ssid.observe(viewLifecycleOwner) { binding.wifiSsid.text = it }
        vm.bssid.observe(viewLifecycleOwner) { binding.wifiBssid.text = it }

        vm.scanning.observe(viewLifecycleOwner) { scanning ->
            binding.scanProgress.visibility = if (scanning) View.VISIBLE else View.GONE
            binding.btnScanQuick.isEnabled = !scanning
            binding.btnScanDeep.isEnabled = !scanning
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
