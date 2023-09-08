#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <utility>   // for std::make_pair
#include <algorithm> // for toLowerCase

#include "QASMTransPrimitives.hpp"
#include "IR/gate.hpp"
#include "IR/circuit.hpp"

using namespace QASMTrans;
using namespace std;
// Assuming creg is a struct or a class

std::string toLowerCase(const std::string &str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c)
                   { return std::tolower(c); });
    return result;
}
// Function to write QASM file
void dumpQASM(std::shared_ptr<QASMTrans::Circuit> circuit, const char *filename, std::string &output_path, IdxType debug_level, IdxType mode)
{
    std::string p(filename);
    std::size_t pos = p.find_last_of("/\\");
    std::string new_file = p.substr(pos + 1);
    IdxType n_gates = circuit->num_gates();
    IdxType n_qubits = IdxType(circuit->num_qubits());
    std::vector<QASMTrans::Gate> gate_info = circuit->get_gates();
    map<string, creg> cregs = circuit->get_cregs();
    std::vector<IdxType> initial_mapping = circuit->get_mapping();
    // std::cout<<"final mapping is"<<std::endl;
    // for (auto& qubit : initial_mapping) {
    //     std::cout<<qubit<<std::endl;
    // }
    std::stringstream file_name;
    std::map<std::string, IdxType> basis_gate_counts;
    std::ofstream qasm_file;
    if (output_path == "../data/output_qasm_file/")
    {
        if (mode == 0)
        {
            file_name << output_path << "transpiled_Qiskit_" << new_file;
            output_path = output_path + "transpiled_Qiskit_" + new_file;
        }
        else if (mode == 1)
        {
            file_name << output_path << "transpiled_IonQ_" << new_file;
            output_path = output_path + "transpiled_IonQ_" + new_file;
        }
        else if (mode == 2)
        {
            file_name << output_path << "transpiled_Quantinuum_" << new_file;
            output_path = output_path + "transpiled_Quantinuum_" + new_file;
        }
        else if (mode == 3)
        {
            file_name << output_path << "transpiled_Rigetti_" << new_file;
            output_path = output_path + "transpiled_Rigetti_" + new_file;
        }
        else if (mode == 4)
        {
            file_name << output_path << "transpiled_Quafu_" << new_file;
            output_path = output_path + "transpiled_Quafu_" + new_file;
        }
        qasm_file.open(file_name.str());
    }
    else
    {
        qasm_file.open(output_path);
    }
    // std::cout<<"output path is: "<<filename.str()<<std::endl;
    if (qasm_file.is_open())
    {
        qasm_file << "OPENQASM 2.0;\n";
        qasm_file << "include \"qelib1.inc\";\n";
        qasm_file << "qreg q[" << n_qubits << "];\n";

        for (auto &creg : cregs)
        {
            qasm_file << "creg " << toLowerCase(creg.first) << "[" << creg.second.width << "];\n";
        }
        for (auto g : gate_info)
        {
            if (std::strcmp(QASMTrans::OP_NAMES[g.op_name], "MA") != 0)
            {
                qasm_file << toLowerCase(g.gateToString()) << "; \n";
                // add gate name and count to map
                if (basis_gate_counts.find(toLowerCase(QASMTrans::OP_NAMES[g.op_name])) == basis_gate_counts.end())
                {
                    basis_gate_counts.insert(std::make_pair(toLowerCase(QASMTrans::OP_NAMES[g.op_name]), 1));
                }
                else
                {
                    basis_gate_counts[toLowerCase(QASMTrans::OP_NAMES[g.op_name])] += 1;
                }
            }
        }
        IdxType creg_index = 0;
        for (auto &creg : cregs)
        {
            for (auto &qubit : creg.second.qubit_indices)
            {
                qasm_file << "measure q[" << circuit->initial_mapping[creg_index] << "] -> " << toLowerCase(creg.first) << "[" << creg_index++ << "];\n";
            }
        }
        // Close the file
        qasm_file.close();
    }
    else
    {
        std::cerr << "Unable to open file";
    }
    if (debug_level > 0)
    {
        std::cout << "Basis gates in transpiled circuit: ";
        for (auto &gate : basis_gate_counts)
        {
            std::cout << gate.first << ":" << gate.second << ", ";
        }
        std::cout << std::endl;
    }
}
