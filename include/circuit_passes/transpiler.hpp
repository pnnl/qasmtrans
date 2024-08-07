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

#include "../QASMTransPrimitives.hpp"

#include "../IR/gate.hpp"
#include "../IR/circuit.hpp"

#include "../dump_qasm.hpp"

#include "routing_mapping.hpp"
#include "decompose.hpp"
#include "remapping.hpp"

using namespace QASMTrans;
using namespace std;

void transpiler(shared_ptr<Circuit> circuit, shared_ptr<Chip> chip, map<string, creg> list_cregs, IdxType debug_level, IdxType mode)
{
    circuit->set_creg(list_cregs);
    IdxType n_qubits = IdxType(circuit->num_qubits());
    IdxType chip_n_qubit = chip->chip_qubit_num;

    if (n_qubits > chip_n_qubit)
    {
        std::cerr<<"Chip qubit number is smaller than the circuit."<<endl;
        std::cerr<<"No transpilation has been performed."<<endl;
        std::exit(1);
    }

    //======================================== STEP-1: Initial Gate Decomposition =====================================
    cpu_timer initial_decompose_timer;
    initial_decompose_timer.start_timer();
    Decompose_three_to_two(circuit);
    initial_decompose_timer.stop_timer();
    double initial_decompose_time = initial_decompose_timer.measure();
    if (debug_level > 0)
        cout << "STEP-1. Initial gate decomposition time: " << (IdxType)initial_decompose_time << "ms" << endl;

    //======================================== STEP-2: Routing and Mapping ============================================
    cpu_timer routing_timer;
    routing_timer.start_timer();
    Routing(circuit, chip, debug_level);
    routing_timer.stop_timer();
    double routing_time = routing_timer.measure();
    if (debug_level > 0)
        cout << "STEP-2. Routing and mapping time: " << (IdxType)routing_time << "ms" << endl;
    if (debug_level > 1)
        cout << circuit->to_string() << endl;
    //======================================== STEP-3: Basis Gate Decomposition =======================================
    cpu_timer decompose_timer;
    decompose_timer.start_timer();
    Decompose(circuit, mode);
    decompose_timer.stop_timer();
    double decompose_time = decompose_timer.measure();
    if (debug_level > 0)
    {
        cout << "STEP-3. Basis gate decomposition time: " << (IdxType)decompose_time << "ms" << endl;
        cout << " total QASMTrans time: " << (IdxType)(initial_decompose_time + routing_time + decompose_time) << "ms" << endl;
    }
}