package com.transparency.android

import android.graphics.Color
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.transparency.android.databinding.ItemDeviceBinding

class DeviceAdapter(
    private val onClick: (Device) -> Unit = {}
) : ListAdapter<Device, DeviceAdapter.ViewHolder>(DIFF) {

    class ViewHolder(val binding: ItemDeviceBinding) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val binding = ItemDeviceBinding.inflate(
            LayoutInflater.from(parent.context), parent, false
        )
        return ViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val device = getItem(position)
        with(holder.binding) {
            deviceName.text = device.displayName
            deviceIp.text = device.ip
            deviceMac.text = device.mac.ifBlank { device.deviceType }
            deviceVendor.text = device.vendor.ifBlank {
                if (device.openPorts.isNotEmpty()) "Ports: ${device.openPorts.joinToString()}"
                else ""
            }

            trustBadge.text = device.trust.name
            val badgeColor = when (device.trust) {
                TrustState.OWNED -> Color.parseColor("#00E57A")
                TrustState.KNOWN -> Color.parseColor("#3D7FFF")
                TrustState.GUEST -> Color.parseColor("#FFC832")
                TrustState.UNKNOWN -> Color.parseColor("#8892A8")
                TrustState.BLOCKED -> Color.parseColor("#FF4060")
                TrustState.WATCHLIST -> Color.parseColor("#A855F7")
            }
            trustBadge.setTextColor(badgeColor)

            root.setOnClickListener { onClick(device) }
        }
    }

    companion object {
        private val DIFF = object : DiffUtil.ItemCallback<Device>() {
            override fun areItemsTheSame(a: Device, b: Device) = a.ip == b.ip
            override fun areContentsTheSame(a: Device, b: Device) = a == b
        }
    }
}
