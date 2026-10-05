#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <utility>   // for std::make_pair
#include <algorithm> // for toLowerCase
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <cstring>
#include <cctype>

#include "QASMTransPrimitives.hpp"
#include "IR/gate.hpp"
#include "IR/circuit.hpp"

using namespace QASMTrans;

namespace
{
inline std::string mode_prefix(IdxType mode)
{
    switch (mode)
    {
    case 0:
        return "transpiled_IBMQ_";
    case 1:
        return "transpiled_IonQ_";
    case 2:
        return "transpiled_Quantinuum_";
    case 3:
        return "transpiled_Rigetti_";
    case 4:
        return "transpiled_Quafu_";
    case 5:
        return "transpiled_IQM_";
    default:
        return "transpiled_";
    }
}

inline void append_lower_ascii(std::string &out, const std::string &value)
{
    size_t start = out.size();
    out += value;
    std::transform(out.begin() + static_cast<std::ptrdiff_t>(start), out.end(),
                   out.begin() + static_cast<std::ptrdiff_t>(start),
                   [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
}

inline std::string build_qasm_text(std::shared_ptr<QASMTrans::Circuit> circuit,
                                   const std::vector<QASMTrans::Gate> &gate_info,
                                   IdxType debug_level,
                                   std::map<std::string, IdxType> *basis_gate_counts)
{
    std::string qasm_text;
    qasm_text.reserve(gate_info.size() * 18 + 4096);

    qasm_text += "OPENQASM 2.0;\n";
    qasm_text += "include \"qelib1.inc\";\n";
    const auto &routed_initial_layout = circuit->routed_initial_mapping_view();
    if (!routed_initial_layout.empty())
    {
        qasm_text += "// qasmtrans_initial_layout ";
        for (std::size_t idx = 0; idx < routed_initial_layout.size(); ++idx)
        {
            if (idx != 0)
            {
                qasm_text += " ";
            }
            qasm_text += std::to_string(routed_initial_layout[idx]);
        }
        qasm_text += "\n";
    }
    qasm_text += "qreg q[";
    qasm_text += std::to_string(circuit->num_qubits());
    qasm_text += "];\n";

    for (const auto &creg : circuit->list_cregs)
    {
        qasm_text += "creg ";
        append_lower_ascii(qasm_text, creg.first);
        qasm_text += "[";
        qasm_text += std::to_string(creg.second.width);
        qasm_text += "];\n";
    }

    const bool collect_basis_gate_counts = debug_level > 0 && basis_gate_counts != nullptr;
    for (const auto &g : gate_info)
    {
        if (std::strcmp(QASMTrans::OP_NAMES[g.op_name], "MA") == 0)
        {
            continue;
        }
        std::string gate_text = g.gateToString();
        if (gate_text.empty())
        {
            continue;
        }
        append_lower_ascii(qasm_text, gate_text);
        qasm_text += ";\n";
        if (collect_basis_gate_counts)
        {
            std::string gate_name = g.lower_name();
            (*basis_gate_counts)[gate_name] += 1;
        }
    }

    IdxType creg_index = 0;
    for (const auto &creg : circuit->list_cregs)
    {
        for (std::size_t bit_index = 0; bit_index < creg.second.qubit_indices.size(); ++bit_index)
        {
            const IdxType logical_qubit = creg.second.qubit_indices[bit_index];
            IdxType mapped = UN_DEF;

            if (creg_index < static_cast<IdxType>(circuit->initial_mapping.size()))
            {
                mapped = circuit->initial_mapping[creg_index];
            }

            // Some pipelines keep initial_mapping as logical->physical rather than
            // the fully expanded per-classical-bit measurement mapping.
            if ((mapped == UN_DEF || creg_index >= static_cast<IdxType>(circuit->initial_mapping.size())) &&
                logical_qubit != UN_DEF &&
                logical_qubit >= 0 &&
                logical_qubit < static_cast<IdxType>(circuit->initial_mapping.size()))
            {
                mapped = circuit->initial_mapping[logical_qubit];
            }

            if (mapped >= 0)
            {
                qasm_text += "measure q[";
                qasm_text += std::to_string(mapped);
                qasm_text += "] -> ";
                append_lower_ascii(qasm_text, creg.first);
                qasm_text += "[";
                qasm_text += std::to_string(bit_index);
                qasm_text += "];\n";
            }
            ++creg_index;
        }
    }
    return qasm_text;
}

inline std::filesystem::path resolve_qasm_output_path(const char *filename,
                                                      const std::string &requested_output_path,
                                                      IdxType mode)
{
    namespace fs = std::filesystem;
    const std::string input_name = filename ? fs::path(filename).filename().string() : "circuit.qasm";
    const bool treat_as_directory = requested_output_path.empty() ||
                                    requested_output_path.back() == '/' ||
                                    requested_output_path.back() == '\\' ||
                                    fs::is_directory(fs::path(requested_output_path));

    if (treat_as_directory)
    {
        fs::path directory = requested_output_path.empty() ? fs::path("../data/output_qasm_file") : fs::path(requested_output_path);
        if (directory.filename() == ".")
        {
            directory = directory.parent_path();
        }
        std::error_code ec;
        fs::create_directories(directory, ec);
        if (ec)
        {
            std::cerr << "Warning: failed to create directory '" << directory << "' (" << ec.message() << ")" << std::endl;
        }
        return directory / (mode_prefix(mode) + input_name);
    }

    fs::path final_path = fs::path(requested_output_path);
    fs::path parent = final_path.parent_path();
    if (!parent.empty())
    {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec)
        {
            std::cerr << "Warning: failed to create directory '" << parent << "' (" << ec.message() << ")" << std::endl;
        }
    }
    return final_path;
}

inline std::string write_qasm_file(std::shared_ptr<QASMTrans::Circuit> circuit,
                                   const std::vector<QASMTrans::Gate> &gate_info,
                                   const char *filename,
                                   const std::string &requested_output_path,
                                   IdxType debug_level,
                                   IdxType mode)
{
    const std::filesystem::path final_path = resolve_qasm_output_path(filename, requested_output_path, mode);
    std::map<std::string, IdxType> basis_gate_counts;

    std::ofstream qasm_file(final_path, std::ios::binary);
    if (!qasm_file.is_open())
    {
        std::cerr << "Error: unable to open QASM output file '" << final_path << "'" << std::endl;
        return final_path.string();
    }

    std::string qasm_text = build_qasm_text(circuit,
                                            gate_info,
                                            debug_level,
                                            debug_level > 0 ? &basis_gate_counts : nullptr);
    qasm_file.write(qasm_text.data(), static_cast<std::streamsize>(qasm_text.size()));
    qasm_file.close();

    if (debug_level > 0)
    {
        IdxType n_gates = circuit->num_gates();
        std::cout << "Transpiled circuit contains " << circuit->num_qubits() << " qubits and " << n_gates << " basis gates: ";
        for (auto &gate : basis_gate_counts)
        {
            std::cout << gate.first << ":" << gate.second << ", ";
        }
        std::cout << std::endl;
    }

    return final_path.string();
}
} // namespace

// Function to write QASM file
std::string dumpQASM(std::shared_ptr<QASMTrans::Circuit> circuit, const char *filename, const std::string &requested_output_path, IdxType debug_level, IdxType mode)
{
    return write_qasm_file(circuit, circuit->gate_list(), filename, requested_output_path, debug_level, mode);
}

std::string dumpQASMFromGates(std::shared_ptr<QASMTrans::Circuit> circuit,
                              const std::vector<QASMTrans::Gate> &gate_info,
                              const char *filename,
                              const std::string &requested_output_path,
                              IdxType debug_level,
                              IdxType mode)
{
    return write_qasm_file(circuit, gate_info, filename, requested_output_path, debug_level, mode);
}
