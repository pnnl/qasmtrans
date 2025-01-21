#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "../QASMTransPrimitives.hpp"
#include "../IR/gate.hpp"
#include "../IR/circuit.hpp"

namespace QASMTrans {

/**
 * @brief Remaps the qubits of the given circuit based on gate usage. The main purpose at present is to improve the 
 * performance of simulating the circuits on NVIDIA GPUs. The reason is that a warp has 32 threads, mapping to 5 qubits.
 * Therefore, this remapping may possibly reduce the degree of warp divergence. 
 * 
 * The function:
 *  1) Counts how many times each qubit appears (ctrl or qubit) in a gate.
 *  2) Sorts qubits by ascending usage.
 *  3) Builds a new mapping, where the first 5 sorted qubits are placed at 
 *     [0..4] in the new mapping, and the rest are filled from the end.
 *  4) Applies the new mapping to the circuit's gates.
 *  5) If an initial mapping exists, composes the new mapping with the old mapping.
 *
 * @param circuit The circuit whose qubits will be remapped in-place.
 */
inline void Remap(std::shared_ptr<Circuit> circuit)
{
    // Basic checks
    if (!circuit) {
        std::cerr << "Remap: null circuit pointer.\n";
        return;
    }

    std::vector<IdxType> initial_mapping = circuit->get_mapping();
    std::vector<Gate> gate_info = circuit->get_gates();
    IdxType n_qubits = circuit->num_qubits();

    // Handle edge cases
    if (n_qubits == 0) {
        std::cerr << "Remap: circuit has 0 qubits.\n";
        return;
    }

    // 1) Count how many times each qubit is used
    // Using direct indexing is faster and simpler than repeated find_if
    std::vector<IdxType> qubit_use_count(n_qubits, 0);
    for (const auto &g : gate_info)
    {
        if (g.ctrl >= 0 && g.ctrl < n_qubits) {
            qubit_use_count[g.ctrl]++;
        }
        if (g.qubit >= 0 && g.qubit < n_qubits) {
            qubit_use_count[g.qubit]++;
        }
    }

    // 2) Create a vector of (qubit, usageCount) pairs
    std::vector<std::pair<IdxType, IdxType>> qubit_usage(n_qubits);
    for (IdxType i = 0; i < n_qubits; i++)
    {
        qubit_usage[i] = { i, qubit_use_count[i] };
    }

    // Sort by ascending usage
    std::sort(qubit_usage.begin(), qubit_usage.end(),
              [](auto &a, auto &b){
                  return a.second < b.second;
              });

    // 3) Build the new mapping
    std::vector<IdxType> new_mapping(n_qubits, 0);
    // If n_qubits < 5, adjust logic to avoid out-of-range
    IdxType pivot = std::min<IdxType>(5, n_qubits);

    // Assign first 'pivot' qubits
    for (IdxType i = 0; i < pivot; i++) {
        new_mapping[i] = qubit_usage[i].first;
    }
    // Assign the rest from the end
    for (IdxType i = pivot; i < n_qubits; i++)
    {
        new_mapping[n_qubits - 1 - (i - pivot)] = qubit_usage[i].first;
    }

    // 4) Apply the new mapping directly to the gate qubit indices
    for (auto &g : gate_info)
    {
        if (g.ctrl >= 0 && g.ctrl < n_qubits) {
            g.ctrl = new_mapping[g.ctrl];
        }
        if (g.qubit >= 0 && g.qubit < n_qubits) {
            g.qubit = new_mapping[g.qubit];
        }
    }

    // 5) If we had an initial mapping, compose them:
    // newMapping = newMapping[initialMapping[...]]
    if (!initial_mapping.empty())
    {
        // Build a composition
        std::vector<IdxType> composed(n_qubits, 0);
        for (IdxType i = 0; i < n_qubits; i++)
        {
            IdxType old_logical = initial_mapping[i];  // the old logical was mapped to old_logical
            if (old_logical < n_qubits) {
                composed[i] = new_mapping[old_logical];
            } else {
                std::cerr << "Remap: out-of-bounds in initial mapping.\n";
                composed[i] = i; // fallback
            }
        }
        new_mapping = composed;
    }

    // Update circuit
    circuit->set_mapping(new_mapping);
    circuit->set_gates(gate_info);
}

} // namespace QASMTrans