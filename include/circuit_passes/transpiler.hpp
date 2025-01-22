#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <bitset>
#include <stdexcept>

// Project includes
#include "../QASMTransPrimitives.hpp"
#include "../IR/gate.hpp"
#include "../IR/circuit.hpp"
#include "../dump_qasm.hpp"
#include "routing_mapping.hpp"
#include "decompose.hpp"
#include "remapping.hpp"

namespace QASMTrans {

inline void transpiler(std::shared_ptr<Circuit> circuit,
                       std::shared_ptr<Chip> chip,
                       std::map<std::string, creg> list_cregs,
                       IdxType debug_level,
                       IdxType mode)
{
    // Associate classical register info
    circuit->set_creg(list_cregs);

    IdxType n_qubits     = static_cast<IdxType>(circuit->num_qubits());
    IdxType chip_n_qubit = chip->chip_qubit_num;

    // Check device/circuit size
    if (n_qubits > chip_n_qubit)
    {
        // If the chip has fewer qubits than the circuit needs, we cannot proceed
        // Throw an exception and let the caller handle it
        throw std::logic_error{"Chip qubit number (" + std::to_string(chip_n_qubit) +
                               ") is smaller than the circuit (" + std::to_string(n_qubits) +
                               "). Transpilation aborted."};
    }

    // === STEP 1: Decompose any three-qubit gates (CCX, CSWAP, etc.) ===
    cpu_timer initial_decompose_timer;
    initial_decompose_timer.start_timer();

    Decompose_three_to_two(circuit);

    initial_decompose_timer.stop_timer();
    double initial_decompose_time = initial_decompose_timer.measure();

    if (debug_level > 0) {
        std::cout << "STEP-1: Initial gate decomposition time: "
                  << static_cast<IdxType>(initial_decompose_time) << "ms\n";
    }

    // === STEP 2: Routing and Mapping (SABRE or other) ===
    cpu_timer routing_timer;
    routing_timer.start_timer();

    Routing(circuit, chip, debug_level);

    routing_timer.stop_timer();
    double routing_time = routing_timer.measure();

    if (debug_level > 0) {
        std::cout << "STEP-2: Routing and mapping time: "
                  << static_cast<IdxType>(routing_time) << "ms\n";
    }
    if (debug_level > 1) {
        // Print circuit details after routing
        std::cout << circuit->to_string() << std::endl;
    }

    // === STEP 3: Basis Gate Decomposition (into hardware-supported gates) ===
    cpu_timer decompose_timer;
    decompose_timer.start_timer();

    Decompose(circuit, mode);

    decompose_timer.stop_timer();
    double decompose_time = decompose_timer.measure();

    if (debug_level > 0)
    {
        std::cout << "STEP-3: Basis gate decomposition time: "
                  << static_cast<IdxType>(decompose_time) << "ms\n";

        double total_time = initial_decompose_time + routing_time + decompose_time;
        std::cout << "Total transpilation time: "
                  << static_cast<IdxType>(total_time) << "ms\n";
    }
}

} // namespace QASMTrans