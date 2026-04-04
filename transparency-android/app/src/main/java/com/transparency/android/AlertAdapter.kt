package com.transparency.android

import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import com.transparency.android.databinding.ItemAlertBinding
import java.time.ZoneId
import java.time.format.DateTimeFormatter

class AlertAdapter : ListAdapter<Alert, AlertAdapter.ViewHolder>(DIFF) {

    private val timeFmt = DateTimeFormatter.ofPattern("HH:mm:ss")
        .withZone(ZoneId.systemDefault())

    class ViewHolder(val binding: ItemAlertBinding) : RecyclerView.ViewHolder(binding.root)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val binding = ItemAlertBinding.inflate(
            LayoutInflater.from(parent.context), parent, false
        )
        return ViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val alert = getItem(position)
        with(holder.binding) {
            alertTitle.text = alert.title
            alertDesc.text = alert.description
            alertTime.text = timeFmt.format(alert.timestamp)

            val dotColor = when (alert.severity) {
                AlertSeverity.CRITICAL -> Color.parseColor("#FF4060")
                AlertSeverity.WARNING -> Color.parseColor("#FFC832")
                AlertSeverity.INFO -> Color.parseColor("#3D7FFF")
            }
            (severityDot.background as? GradientDrawable)?.setColor(dotColor)
                ?: run { severityDot.setBackgroundColor(dotColor) }
        }
    }

    companion object {
        private val DIFF = object : DiffUtil.ItemCallback<Alert>() {
            override fun areItemsTheSame(a: Alert, b: Alert) = a.id == b.id
            override fun areContentsTheSame(a: Alert, b: Alert) = a == b
        }
    }
}
