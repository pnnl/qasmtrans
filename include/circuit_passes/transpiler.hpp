#pragma once

#include <algorithm>
#include <bitset>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "../QASMTransPrimitives.hpp"

#include "../IR/gate.hpp"
#include "../IR/circuit.hpp"

#include "../cli_support.hpp"
#include "../dump_qasm.hpp"

#include "routing_mapping.hpp"
#include "decompose.hpp"
#include "optimize_1q.hpp"
#include "mapomatic.hpp"
#include "remapping.hpp"

using namespace QASMTrans;
using namespace std;

inline std::string logical_label_for_gate(const Gate &gate)
{
    std::string name = OP_NAMES[gate.op_name];
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    std::ostringstream oss;
    oss << name;
    auto append_param = [&](const char *key, ValType value)
    {
        if (value != 0.0)
        {
            oss << "[" << key << "=" << std::setprecision(10) << value << "]";
        }
    };
    switch (gate.op_name)
    {
    case OP::RX:
    case OP::RY:
    case OP::RZ:
    case OP::RI:
    case OP::P:
    case OP::CRX:
    case OP::CRY:
    case OP::CRZ:
    case OP::CP:
    case OP::U:
    case OP::CU:
    case OP::RXX:
    case OP::RYY:
    case OP::RZZ:
    case OP::RZX:
        append_param("theta", gate.theta);
        append_param("phi", gate.phi);
        append_param("lambda", gate.lam);
        append_param("gamma", gate.gamma);
        break;
    default:
        break;
    }
    return oss.str();
}

inline void assign_logical_gate_ids(std::vector<Gate> &gates)
{
    IdxType next_id = 0;
    for (auto &gate : gates)
    {
        gate.logical_gate_id = next_id++;
        gate.logical_label.clear();
    }
}

inline void materialize_logical_labels(std::vector<Gate> &gates)
{
    for (auto &gate : gates)
    {
        if (gate.logical_gate_id >= 0 && gate.logical_label.empty())
        {
            gate.logical_label = logical_label_for_gate(gate);
        }
    }
}

inline bool enforce_cx_direction(std::shared_ptr<Circuit> circuit,
                                 const std::shared_ptr<Chip> &chip,
                                 const std::unordered_set<std::string> &basis_gates)
{
    if (!circuit || !chip || chip->directed_edge_list.empty())
    {
        return false;
    }
    if (basis_gates.empty())
    {
        return false;
    }
    const bool has_h = basis_gates.find("h") != basis_gates.end();
    const bool has_rz = basis_gates.find("rz") != basis_gates.end();
    const bool has_sx = basis_gates.find("sx") != basis_gates.end();
    if (!has_h && !(has_rz && has_sx))
    {
        return false;
    }
    const std::vector<Gate> &gates = circuit->gate_list();
    if (gates.empty())
    {
        return false;
    }

    auto needs_flip = [&](const Gate &gate)
    {
        if (gate.op_name != OP::CX || gate.has_custom_name())
        {
            return false;
        }
        const IdxType ctrl = gate.ctrl;
        const IdxType tgt = gate.qubit;
        bool allowed = false;
        if (ctrl >= 0 && ctrl < static_cast<IdxType>(chip->directed_edge_list.size()))
        {
            const auto &targets = chip->directed_edge_list[static_cast<std::size_t>(ctrl)];
            allowed = targets.find(tgt) != targets.end();
        }
        return !allowed && ctrl >= 0 && tgt >= 0;
    };

    if (!std::any_of(gates.begin(), gates.end(), needs_flip))
    {
        return false;
    }

    auto emit_h = [&](std::vector<Gate> &out, IdxType qubit)
    {
        if (has_h)
        {
            out.emplace_back(OP::H, qubit);
            return;
        }
        out.emplace_back(OP::RZ, qubit, -1, -1, 1, PI / 2);
        out.emplace_back(OP::SX, qubit);
        out.emplace_back(OP::RZ, qubit, -1, -1, 1, PI / 2);
    };

    std::vector<Gate> out;
    out.reserve(gates.size());
    for (const auto &gate : gates)
    {
        if (needs_flip(gate))
        {
            const IdxType ctrl = gate.ctrl;
            const IdxType tgt = gate.qubit;
            emit_h(out, ctrl);
            emit_h(out, tgt);
            Gate flipped = gate;
            flipped.ctrl = tgt;
            flipped.qubit = ctrl;
            out.push_back(flipped);
            emit_h(out, ctrl);
            emit_h(out, tgt);
            continue;
        }
        out.push_back(gate);
    }
    circuit->set_gates(std::move(out));
    return true;
}

enum class TranspilerProfile
{
    Baseline,
    Enhanced
};

inline void transpiler(shared_ptr<Circuit> circuit,
                       shared_ptr<Chip> chip,
                       map<string, creg> list_cregs,
                       IdxType debug_level,
                       IdxType mode,
                       bool use_full_fidelity,
                       CriticalPathHeuristicMode cp_mode,
                       bool disable_mapomatic,
                       std::size_t mapomatic_max_embeddings,
                       bool enable_1q_opt,
                       RoutingMode routing_mode,
                       TranspilerProfile profile,
                       const std::unordered_set<std::string> &basis_gates)
{
    circuit->set_creg(list_cregs);
    IdxType n_qubits = IdxType(circuit->num_qubits());
    IdxType chip_n_qubit = chip->chip_qubit_num;

    if (n_qubits > chip_n_qubit)
    {
        //std::cerr<<"Chip qubit number is smaller than the circuit."<<endl;
        //std::cerr<<"No transpilation has been performed."<<endl;
        throw std::logic_error{"Chip qubit number is smaller than the circuit. No transpilation has been performed."};
    }

    //======================================== STEP-1: Initial Gate Decomposition =====================================
    auto format_ms = [](double ms) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << ms;
        return oss.str();
    };
    const bool enhanced_profile = profile == TranspilerProfile::Enhanced;

    cpu_timer initial_decompose_timer;
    initial_decompose_timer.start_timer();
    Decompose_three_to_two(circuit);
    initial_decompose_timer.stop_timer();
    double initial_decompose_time = initial_decompose_timer.measure();
    if (enhanced_profile)
    {
        std::vector<Gate> logical_stage_gates = circuit->get_gates();
        assign_logical_gate_ids(logical_stage_gates);
        circuit->set_gates(logical_stage_gates);
    }
    if (debug_level > 0)
        cout << "STEP-1. Initial gate decomposition time: " << format_ms(initial_decompose_time) << "ms" << endl;

    //======================================== STEP-2: Routing and Mapping ============================================
    cpu_timer routing_timer;
    routing_timer.start_timer();
    Routing(circuit, chip, debug_level, routing_mode);
    {
        IdxType swap_count = 0;
        IdxType cx_count = 0;
        IdxType twoq_count = 0;
        for (const auto &gate : circuit->gate_list())
        {
            if (gate.op_name == OP::SWAP)
            {
                swap_count += 1;
            }
            if (gate.ctrl >= 0 && gate.qubit >= 0 && gate.extra < 0)
            {
                twoq_count += 1;
                if (gate.op_name == OP::CX)
                {
                    cx_count += 1;
                }
            }
        }
        circuit->set_routing_swap_count(swap_count);
        if (debug_level > 0)
        {
            std::cout << "STEP-2. Routing stats: twoq=" << twoq_count
                      << " cx=" << cx_count
                      << " swap=" << swap_count << std::endl;
        }
    }
    routing_timer.stop_timer();
    double routing_time = routing_timer.measure();
    if (debug_level > 0)
        cout << "STEP-2. Routing and mapping time: " << format_ms(routing_time) << "ms" << endl;
    if (debug_level > 1)
        cout << circuit->to_string() << endl;

    if (!enhanced_profile)
    {
        cpu_timer decompose_timer;
        decompose_timer.start_timer();
        Decompose(circuit, mode, enhanced_profile);
        if (enable_1q_opt)
        {
            QASMTrans::optimize::optimize_1q_gates_decomposition(circuit, chip, &basis_gates, debug_level, "baseline-single-pass");
        }
        decompose_timer.stop_timer();
        double decompose_time = decompose_timer.measure();
        if (debug_level > 0)
        {
            cout << "STEP-3. Basis gate decomposition time: " << format_ms(decompose_time) << "ms" << endl;
            cout << " total QASMTrans time: " << format_ms(initial_decompose_time + routing_time + decompose_time) << "ms" << endl;
        }
        return;
    }

    //======================================== STEP-3: Calibration-Aware Optimization =======================================
    double calib_time = 0.0;
    if (!disable_mapomatic)
    {
        cpu_timer calib_timer;
        calib_timer.start_timer();
        calibration_aware_optimization(circuit, chip, debug_level, use_full_fidelity, cp_mode, mapomatic_max_embeddings);
        calib_timer.stop_timer();
        calib_time = calib_timer.measure();
        if (debug_level > 0)
            cout << "STEP-3. Calibration-aware optimization time: " << format_ms(calib_time) << "ms" << endl;
        if (debug_level > 1)
            cout << circuit->to_string() << endl;
    }
    else if (debug_level > 0)
    {
        cout << "STEP-3. Calibration-aware optimization skipped (--disable_mapomatic)" << endl;
    }
    //======================================== STEP-4: Basis Gate Decomposition =======================================
    cpu_timer decompose_timer;
    decompose_timer.start_timer();
    double materialize_time = 0.0;
    double basis_decompose_time = 0.0;
    double opt_loop_time = 0.0;
    double enforce_direction_time = 0.0;
    cpu_timer step4_section_timer;
    {
        step4_section_timer.start_timer();
        std::vector<Gate> routed_gates = circuit->get_gates();
        materialize_logical_labels(routed_gates);
        circuit->set_gates(routed_gates);
        step4_section_timer.stop_timer();
        materialize_time = step4_section_timer.measure();
    }
    step4_section_timer.start_timer();
    Decompose(circuit, mode, enhanced_profile);
    step4_section_timer.stop_timer();
    basis_decompose_time = step4_section_timer.measure();
    if (enable_1q_opt)
    {
        step4_section_timer.start_timer();
        QASMTrans::optimize::optimize_1q_gates_decomposition(circuit, chip, &basis_gates, debug_level, "single-pass");
        step4_section_timer.stop_timer();
        opt_loop_time = step4_section_timer.measure();
    }

    step4_section_timer.start_timer();
    const bool direction_changed = enforce_cx_direction(circuit, chip, basis_gates);
    step4_section_timer.stop_timer();
    enforce_direction_time = step4_section_timer.measure();
    if (enable_1q_opt && direction_changed)
    {
        step4_section_timer.start_timer();
        QASMTrans::optimize::optimize_1q_gates_decomposition(circuit, chip, &basis_gates, debug_level, "post-direction");
        step4_section_timer.stop_timer();
        opt_loop_time += step4_section_timer.measure();
    }
    decompose_timer.stop_timer();
    double decompose_time = decompose_timer.measure();
    if (debug_level > 0)
    {
        if (debug_level > 1)
        {
            cout << "  [STEP-4 detail] materialize=" << format_ms(materialize_time) << "ms"
                 << " decompose=" << format_ms(basis_decompose_time) << "ms"
                 << " opt_loop=" << format_ms(opt_loop_time) << "ms"
                 << " enforce_direction=" << format_ms(enforce_direction_time) << "ms" << endl;
        }
        cout << "STEP-4. Basis gate decomposition time: " << format_ms(decompose_time) << "ms" << endl;
        cout << " total QASMTrans time: " << format_ms(initial_decompose_time + routing_time + decompose_time) << "ms" << endl;
    }

}

inline void transpiler(shared_ptr<Circuit> circuit,
                       shared_ptr<Chip> chip,
                       map<string, creg> list_cregs,
                       IdxType debug_level,
                       IdxType mode)
{
    static const std::unordered_set<std::string> empty_basis_gates;
    transpiler(circuit,
               chip,
               list_cregs,
               debug_level,
               mode,
               false,
               CriticalPathHeuristicMode::LogProduct,
               true,
               1000,
               false,
               RoutingMode::Sabre,
               TranspilerProfile::Baseline,
               empty_basis_gates);
}
