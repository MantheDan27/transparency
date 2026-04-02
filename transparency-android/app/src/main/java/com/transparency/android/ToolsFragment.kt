package com.transparency.android

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import com.transparency.android.databinding.FragmentToolsBinding

class ToolsFragment : Fragment() {

    private var _binding: FragmentToolsBinding? = null
    private val binding get() = _binding!!
    private val vm: MainViewModel by activityViewModels()

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        _binding = FragmentToolsBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val getHost = { binding.toolInput.text.toString().trim() }

        binding.btnPing.setOnClickListener {
            val host = getHost()
            if (host.isNotBlank()) vm.runPing(host)
        }
        binding.btnDns.setOnClickListener {
            val host = getHost()
            if (host.isNotBlank()) vm.runDns(host)
        }
        binding.btnTraceroute.setOnClickListener {
            val host = getHost()
            if (host.isNotBlank()) vm.runTraceroute(host)
        }
        binding.btnPortScan.setOnClickListener {
            val host = getHost()
            if (host.isNotBlank()) vm.runPortScan(host)
        }

        vm.toolOutput.observe(viewLifecycleOwner) {
            binding.toolOutput.text = it
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
