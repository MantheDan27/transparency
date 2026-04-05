package com.transparency.android

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.LinearLayoutManager
import com.transparency.android.databinding.FragmentAlertsBinding

class AlertsFragment : Fragment() {

    private var _binding: FragmentAlertsBinding? = null
    private val binding get() = _binding!!
    private val vm: MainViewModel by activityViewModels()
    private val adapter = AlertAdapter()
    private var currentFilter = "All"

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentAlertsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.alertList.layoutManager = LinearLayoutManager(requireContext())
        binding.alertList.adapter = adapter

        val filters = listOf("All", "Critical", "Warning", "Info")
        filters.forEach { label ->
            val pill = Button(requireContext()).apply {
                text = label
                setBackgroundResource(R.drawable.bg_glass_pill)
                setTextColor(resources.getColor(R.color.text_secondary, null))
                textSize = 11f
                isAllCaps = true
                setPadding(32, 8, 32, 8)
                val lp = ViewGroup.MarginLayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                )
                lp.marginEnd = 8
                layoutParams = lp
                setOnClickListener {
                    currentFilter = label
                    applyFilter()
                    updatePillStates()
                }
            }
            binding.alertFilterRow.addView(pill)
        }
        updatePillStates()

        vm.alerts.observe(viewLifecycleOwner) { applyFilter() }
    }

    private fun applyFilter() {
        val all = vm.alerts.value.orEmpty()
        val filtered = if (currentFilter == "All") all
        else all.filter { it.severity.name.equals(currentFilter, ignoreCase = true) }
        adapter.submitList(filtered)
    }

    private fun updatePillStates() {
        for (i in 0 until binding.alertFilterRow.childCount) {
            val pill = binding.alertFilterRow.getChildAt(i) as Button
            val active = pill.text.toString() == currentFilter
            pill.isSelected = active
            pill.setTextColor(
                resources.getColor(
                    if (active) R.color.accent_blue else R.color.text_secondary, null
                )
            )
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
