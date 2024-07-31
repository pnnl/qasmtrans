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

std::string toLowerCase(const std::string &str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
            [](unsigned char c)
            { return std::tolower(c); });
    return result;
}
// Function to write QASM file
void dumpQASM(std::shared_ptr<QASMTrans::Circuit> circuit, const char *filename, std::string &output_path, IdxType debug_level, IdxType mode)
{
    std::string p(filename);
    std::size_t pos = p.find_last_of("/\\");
    std::string new_file = p.substr(pos + 1);
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
        switch (mode)
        {
            case 0: //IBMQ
                file_name << output_path << "transpiled_IBMQ_" << new_file;
                output_path = output_path + "transpiled_IBMQ_" + new_file;
                break;
            case 1: //IonQ
                file_name << output_path << "transpiled_IonQ_" << new_file;
                output_path = output_path + "transpiled_IonQ_" + new_file;
                break;
            case 2: //Quantinuum
                file_name << output_path << "transpiled_Quantinuum_" << new_file;
                output_path = output_path + "transpiled_Quantinuum_" + new_file;
                break;
            case 3: //Rigetti
                file_name << output_path << "transpiled_Rigetti_" << new_file;
                output_path = output_path + "transpiled_Rigetti_" + new_file;
                break;
            case 4: //Quafu
                file_name << output_path << "transpiled_Quafu_" << new_file;
                output_path = output_path + "transpiled_Quafu_" + new_file;
                break;
            default:
                std::cerr << "Error: unspecified mode!" << endl;
                exit(1);
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
                if (g.gateToString() != "")
                {
                    qasm_file << toLowerCase(g.gateToString()) << "; \n";
                    std::string str_out = toLowerCase(g.gateToString());
                    std::string err_string = "rz q[1]";
                    if (str_out.find(err_string) != std::string::npos)
                    {
                        cout << "ERROR: " << g.op_name << endl;
                    }
                    // add gate name and count to map
                    std::string gate_name = toLowerCase(QASMTrans::OP_NAMES[g.op_name]);
                    if (basis_gate_counts.find(gate_name) == basis_gate_counts.end())
                    {
                        basis_gate_counts.insert(std::make_pair(gate_name, 1));
                    }
                    else
                    {
                        basis_gate_counts[gate_name] += 1;
                    }
                }
            }
        }
        IdxType creg_index = 0;
        for (auto &creg : cregs)
        {
            for (auto &qubit : creg.second.qubit_indices)
            {
                qasm_file << "measure q[" << circuit->initial_mapping[creg_index] << "] -> " << toLowerCase(creg.first) << "[" << creg_index << "];\n";
                ++creg_index;
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
        IdxType n_gates = circuit->num_gates();
        std::cout << "In total, " << n_gates << " basis gates in transpiled circuit: ";
        for (auto &gate : basis_gate_counts)
        {
            std::cout << gate.first << ":" << gate.second << ", ";
        }
        std::cout << std::endl;
    }
}
