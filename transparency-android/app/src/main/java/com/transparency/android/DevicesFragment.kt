package com.transparency.android

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.LinearLayoutManager
import com.transparency.android.databinding.FragmentDevicesBinding

class DevicesFragment : Fragment() {

    private var _binding: FragmentDevicesBinding? = null
    private val binding get() = _binding!!
    private val vm: MainViewModel by activityViewModels()
    private val adapter = DeviceAdapter()
    private var currentFilter = "All"

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentDevicesBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.deviceList.layoutManager = LinearLayoutManager(requireContext())
        binding.deviceList.adapter = adapter

        val filters = listOf("All", "Owned", "Known", "Guest", "Unknown", "Blocked")
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
            binding.filterRow.addView(pill)
        }
        updatePillStates()

        vm.devices.observe(viewLifecycleOwner) { applyFilter() }
    }

    private fun applyFilter() {
        val all = vm.devices.value.orEmpty()
        val filtered = if (currentFilter == "All") all
        else all.filter { it.trust.name.equals(currentFilter, ignoreCase = true) }
        adapter.submitList(filtered)
    }

    private fun updatePillStates() {
        for (i in 0 until binding.filterRow.childCount) {
            val pill = binding.filterRow.getChildAt(i) as Button
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
