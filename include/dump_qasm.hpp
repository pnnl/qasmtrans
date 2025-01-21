#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "QASMTransPrimitives.hpp"
#include "IR/gate.hpp"
#include "IR/circuit.hpp"

namespace QASMTrans {

/**
 * @brief Helper to transform a string to lowercase.
 */
inline std::string toLowerCase(const std::string &str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

/**
 * @brief Builds the output file path based on a default directory and the transpiler mode.
 * 
 * If 'basePath' is something other than the default, it simply returns that. Otherwise,
 * it uses 'mode' to build a "transpiled_XXX_originalFileName" in 'basePath'.
 *
 * @param basePath   The user-requested path or default
 * @param fileName   Original input file name (used to generate final output name)
 * @param mode       The hardware mode (0=IBMQ, 1=IonQ, etc.)
 * @return A string with the final output file path.
 */
inline std::string buildOutputFilename(const std::string &basePath,
                                       const std::string &fileName,
                                       IdxType mode)
{
    // If the user-supplied path is not the default, use it verbatim
    if (basePath != "../data/output_qasm_file/") {
        return basePath; // user wants exactly this path
    }

    // Otherwise, build a name
    std::ostringstream oss;
    oss << basePath << "transpiled_";

    switch(mode) {
        case 0: oss << "IBMQ_";        break;
        case 1: oss << "IonQ_";        break;
        case 2: oss << "Quantinuum_";  break;
        case 3: oss << "Rigetti_";     break;
        case 4: oss << "Quafu_";       break;
        default:
            throw std::runtime_error("dumpQASM: invalid mode=" + std::to_string(mode));
    }
    oss << fileName;
    return oss.str();
}

/**
 * @brief Writes a QASM file with the specified circuit gates, classical registers, and mapping.
 *
 * @param circuit     The circuit to output
 * @param filename    The original input file name (used in building output name)
 * @param output_path The user-specified (or default) output path
 * @param debug_level Debug verbosity
 * @param mode        The hardware mode (0=IBMQ, 1=IonQ, 2=Quantinuum, etc.)
 */
inline void dumpQASM(std::shared_ptr<Circuit> circuit,
                     const char *filename,
                     std::string &output_path,
                     IdxType debug_level,
                     IdxType mode)
{
    // Extract the final filename portion from the full path
    std::string p(filename ? filename : "");
    std::size_t pos = p.find_last_of("/\\");
    std::string baseName = (pos == std::string::npos) ? p : p.substr(pos + 1);

    IdxType n_qubits = static_cast<IdxType>(circuit->num_qubits());
    auto gate_info   = circuit->get_gates();
    auto cregs       = circuit->get_cregs();
    auto mapping     = circuit->get_mapping(); // "final" mapping
    std::map<std::string, IdxType> basis_gate_counts;

    // Build final output file name
    std::string finalOutput = buildOutputFilename(output_path, baseName, mode);

    // Attempt to open the file
    std::ofstream qasm_file(finalOutput);
    if (!qasm_file.is_open()) {
        std::cerr << "Error: Unable to open file for writing: " << finalOutput << "\n";
        return;
    }

    // Write header
    qasm_file << "OPENQASM 2.0;\n"
              << "include \"qelib1.inc\";\n"
              << "qreg q[" << n_qubits << "];\n";

    // Write classical registers
    for (const auto &cr : cregs)
    {
        // e.g., "creg c0[4];"
        qasm_file << "creg " << toLowerCase(cr.first)
                  << "[" << cr.second.width << "];\n";
    }

    // Write gates
    for (auto &g : gate_info)
    {
        if (std::strcmp(OP_NAMES[g.op_name], "MA") == 0) {
            // Skip measurement placeholders in gate_info if you handle them separately
            continue;
        }

        std::string gateStr = g.gateToString();
        if (!gateStr.empty())
        {
            // Write gate line in lower case, e.g. "cx q[0],q[1];"
            qasm_file << toLowerCase(gateStr) << ";\n";

            // Update basis gate counts
            std::string gate_name = toLowerCase(OP_NAMES[g.op_name]);
            basis_gate_counts[gate_name]++;
        }
    }

    // Write measurement lines
    // We assume each classical register has a list of qubit_indices
    // and we measure in the final (physical) qubit = mapping[creg_index].
    // You can adapt if you prefer measuring the "logical" qubit.
    IdxType creg_index = 0;
    for (const auto &cr : cregs)
    {
        // For each qubit mapped to this classical register
        for (auto qIdx : cr.second.qubit_indices)
        {
            // Check bounds
            if (creg_index >= mapping.size()) {
                std::cerr << "Warning: creg_index=" << creg_index
                          << " exceeds mapping size=" << mapping.size() << "\n";
                break;
            }
            // Measure physical qubit mapped from the original index
            IdxType physicalQ = mapping[creg_index];
            if (physicalQ >= n_qubits) {
                std::cerr << "Warning: physical qubit " << physicalQ
                          << " out of range [0," << n_qubits << ")\n";
                continue;
            }
            qasm_file << "measure q[" << physicalQ << "] -> "
                      << toLowerCase(cr.first) << "[" << creg_index << "];\n";
            ++creg_index;
        }
    }

    // Done!
    qasm_file.close();

    // If debug, show summary
    if (debug_level > 0)
    {
        IdxType n_gates = circuit->num_gates();
        std::cout << "QASM saved to: " << finalOutput << "\n"
                  << "Total gates: " << n_gates << ". Basis gate breakdown: ";
        for (const auto &entry : basis_gate_counts)
        {
            std::cout << entry.first << ":" << entry.second << ", ";
        }
        std::cout << "\n";
    }

    // Update the output_path reference argument to reflect the final file used
    output_path = finalOutput;
}

} // namespace QASMTrans