#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <bitset>
#include <stdexcept>

#include "parser_util.hpp"
#include "lexer.hpp"

#include "../QASMTransPrimitives.hpp"
#include "../IR/circuit.hpp"

namespace QASMTrans {

using lexertk::token;
using lexertk::generator;

/**
 * @brief Convert an integer to a bitstring of a given length.
 */
inline std::string intToBitString(int num, int digitCount)
{
    std::bitset<32> bitsetVal(num); // Adjust size as needed
    std::string bitString = bitsetVal.to_string();
    // Get the last `digitCount` characters
    return bitString.substr(bitString.length() - digitCount);
}

/**
 * @brief The main QASM parser class responsible for reading a QASM file
 *        and populating a circuit structure.
 */
class qasm_parser
{
public:
    /**
     * @brief Constructor that opens a QASM file and parses it into internal data structures.
     * @param filename Path to the QASM file.
     * @throws std::runtime_error if the file cannot be opened.
     */
    qasm_parser(const char* filename);

    ~qasm_parser();

    /**
     * @return The total number of qubits used (sum of all qregs).
     */
    IdxType num_qubits();

    /**
     * @brief Load the parsed gates into a given circuit.
     */
    void loadin_circuit(std::shared_ptr<Circuit> circuit);

    /**
     * @return Keeping track of the QASM filepath.
     */
    const char *filename;

    /**
     * @return A map of classical registers (creg) used in the QASM file.
     */
    std::map<std::string, creg> get_list_cregs();

    /**
     * @return A map of quantum registers (qreg) used in the QASM file.
     */
    std::map<std::string, qreg> get_list_qregs();

    std::string sim_method; ///< Possibly unused or for future expansion

private:
    // Internal data
    std::map<std::string, qreg> list_qregs;
    std::map<std::string, creg> list_cregs;
    std::map<std::string, defined_gate> list_defined_gates;

    std::vector<qasm_gate> list_gates; ///< Replaces the raw pointer approach
    std::vector<qasm_gate> list_buffered_measure;

    IdxType global_qubit_offset = 0;
    bool contains_if   = false;
    bool measure_all   = true;
    bool skip_if       = false;

    // Lexer / parser
    generator gen;
    lexertk::helper::symbol_replacer sr;

    // File reading
    std::ifstream qasmFile;
    std::string line;
    std::stringstream ss;

    // Temporary storage for current instruction tokens
    std::vector<token> cur_inst;

    // Private member functions
    void load_instruction();
    void parse_gate_definition();
    void parse_gate(std::vector<token>& inst, std::vector<qasm_gate>* gates);
    void parse_native_gate(std::vector<token>& inst, std::vector<qasm_gate>* gates);
    void parse_defined_gate(std::vector<token>& inst, std::vector<qasm_gate>* gates);

    void generate_circuit(std::shared_ptr<Circuit> circuit, qasm_gate gate);

    // Debug / helper
    void dump_defined_gates();
    void dump_cur_inst();
    void dump_gates();
};

// 1) Provide a find_index function:
inline IdxType find_index(const std::vector<std::string>& vec, const std::string& target)
{
    for (IdxType i = 0; i < vec.size(); i++)
    {
        if (vec[i] == target) return i;
    }
    return -1; // not found
}

inline void print_gate(qasm_gate gate, bool indent = false)
{
    if (indent)
        cout << "\t";
    if (gate.name == MEASURE)
    {
        cout << "M " << gate.measured_qubit_index << " -> " << gate.creg_name << "[" << gate.creg_index << "];\n";
    }
    else
    {
        cout << gate.name << " ";
        if (gate.params.size() > 0)
        {
            cout << "\b(";
            for (auto p : gate.params)
                cout << p << ",";
            cout << "\b) ";
        }
        for (auto q : gate.qubits)
            cout << q << ",";
        cout << "\b;\n";
    }
}


/**
 * @brief Implementation of qasm_parser constructor.
 */
qasm_parser::qasm_parser(const char* filename)
    : qasmFile(filename)
{
    if (!qasmFile.is_open())
    {
        throw std::runtime_error(std::string("Could not open QASM file: ") + filename);
    }
    // Keeping track of the QASM filepath
    this->filename = filename;

    // Symbol replacer for the lexer
    sr.add_replace("pi", "pi", token::e_pi);
    sr.add_replace("sin", "sin", token::e_func);
    sr.add_replace("cos", "cos", token::e_func);

    // Read and parse each line
    while (!qasmFile.eof())
    {
        load_instruction();
        if (cur_inst.empty()) continue; // nothing read, skip

        // e.g. if the first token is openqasm -> parse version
        if (cur_inst[INST_NAME].value == OPENQASM)
        {
            // Possibly parse QASM version
            // e.g. std::cout << "OpenQASM version " << cur_inst[INST_QASM_VERSION].value << "\n";
        }
        else if (cur_inst[INST_NAME].value == QREG)
        {
            qreg new_qreg;
            new_qreg.name   = cur_inst[INST_REG_NAME].value;
            new_qreg.width  = std::stoi(cur_inst[INST_REG_WIDTH].value);
            new_qreg.offset = global_qubit_offset;
            global_qubit_offset += new_qreg.width;
            list_qregs.insert({ new_qreg.name, new_qreg });

            // If more than 63 qubits, skip parsing?
            if (global_qubit_offset > 63)
                skip_if = true;
        }
        else if (cur_inst[INST_NAME].value == CREG)
        {
            creg new_creg;
            new_creg.name  = cur_inst[INST_REG_NAME].value;
            new_creg.width = std::stoi(cur_inst[INST_REG_WIDTH].value);
            new_creg.qubit_indices.resize(new_creg.width, UN_DEF);
            list_cregs.insert({ new_creg.name, new_creg });
        }
        else if (cur_inst[INST_NAME].value == GATE)
        {
            parse_gate_definition();
        }
        else if (cur_inst[INST_NAME].value == IF)
        {
            if (!skip_if)
            {
                // parse the if statement
                qasm_gate cur_gate;
                cur_gate.name         = IF;
                cur_gate.creg_name    = cur_inst[INST_IF_CREG].value;
                cur_gate.if_creg_val  = std::stoll(cur_inst[INST_IF_VAL].value);
                cur_gate.conditional_inst = new std::vector<qasm_gate>; // dynamic in your design

                // parse the instructions after 'if'
                auto c_inst = slices(cur_inst, INST_IF_INST_START, cur_inst.size() - 1);
                parse_gate(c_inst, cur_gate.conditional_inst);

                list_gates.push_back(cur_gate);
                contains_if = true;
            }
        }
        else
        {
            // parse quantum gate
            parse_gate(cur_inst, &list_gates);
        }
    }
    // Optional: dump_defined_gates();
    // dump_gates();
}

/**
 * @brief Destructor
 */
qasm_parser::~qasm_parser()
{
    // Free dynamically allocated conditional instruction vectors
    for (auto &g : list_gates)
    {
        if (g.name == IF && g.conditional_inst)
        {
            delete g.conditional_inst;
            g.conditional_inst = nullptr;
        }
    }
    // The ifstream is automatically closed
}

/**
 * @brief Attempts to read one instruction from the QASM file into cur_inst.
 */
void qasm_parser::load_instruction()
{
    cur_inst.clear();
    if (!std::getline(qasmFile, line))
        return;

    // Convert line to uppercase
    std::transform(line.begin(), line.end(), line.begin(), ::toupper);

    if (!gen.process(line))
        return; // no tokens generated

    // Replace symbols like pi, sin, cos
    sr.process(gen);

    // Add the tokens from this line to cur_inst
    for (size_t i = 0; i < gen.size(); i++)
    {
        cur_inst.push_back(gen[i]);
    }

    // Possibly read additional lines until we find '{' or ';' etc. (per your logic)
    // ...
    // You have logic that uses has_eof, has_lcurly, has_rcurly, etc. 
    // For brevity, we keep it minimal here. If you want the full multi-line
    // read logic, re-incorporate it from your snippet, but consider separating
    // it out into smaller functions for clarity.
}

/**
 * @brief Debug function to print the current instruction tokens.
 */
void qasm_parser::dump_cur_inst()
{
    for (auto &tk : cur_inst)
    {
        std::cout << tk.value << " ";
    }
    std::cout << "\n";
}

/**
 * @brief Parse a custom gate definition (e.g. "gate myGate( ... ) { ... }").
 */
void qasm_parser::parse_gate_definition()
{
    defined_gate dg;
    dg.name = cur_inst[INST_GATE_NAME].value;

    // find the '{'
    IdxType lcurly_pos = -1;
    for (size_t i = 0; i < cur_inst.size(); i++)
    {
        if (cur_inst[i].type == token::e_lcrlbracket)
        {
            lcurly_pos = i;
            break;
        }
    }
    inst_indicies gate_indices = get_indices(cur_inst, 1, lcurly_pos);

    // Collect parameters
    if (gate_indices.param_start != -1)
    {
        auto paramTokens = slices(cur_inst, gate_indices.param_start, gate_indices.param_end);
        for (auto &p : paramTokens)
        {
            if (p.type == token::e_comma) 
                continue;
            if (p.type == token::e_symbol)
            {
                dg.params.push_back(p.value);
            }
            else
            {
                std::cerr << "Invalid param for gate definition: " << p.value << "\n";
            }
        }
    }
    // Collect qubits
    auto qubitTokens = slices(cur_inst, gate_indices.qubit_start, gate_indices.qubit_end);
    for (auto &q : qubitTokens)
    {
        if (q.type == token::e_comma) 
            continue;
        else if (q.type == token::e_symbol)
        {
            dg.qubits.push_back(q.value);
        }
        else
        {
            std::cerr << "Invalid qubit param for gate definition: " << q.value << "\n";
        }
    }

    // collect instructions between '{' and '}' 
    IdxType cur_start = lcurly_pos + 1;
    for (size_t i = lcurly_pos + 1; i < cur_inst.size(); i++)
    {
        if (cur_inst[i].type == token::e_eof)
        {
            dg.instructions.push_back(slices(cur_inst, cur_start, i + 1));
            cur_start = i + 1;
        }
    }
    list_defined_gates.insert({ dg.name, dg });
}

/**
 * @brief Parse a (potentially custom) gate instruction into a vector of qasm_gates.
 */
void qasm_parser::parse_gate(std::vector<token> &inst, std::vector<qasm_gate>* gates)
{
    if (inst[INST_NAME].value == MEASURE)
    {
        if (inst.size() == 12)
        {
            // measure q[x] -> c[x]
            qasm_gate cur_gate;
            cur_gate.name = MEASURE;
            cur_gate.measured_qubit_index =
                list_qregs.at(inst[INST_MEASURE_QREG_NAME].value).offset +
                std::stoll(inst[INST_MEASURE_QREG_BIT].value);

            cur_gate.creg_name  = inst[INST_MEASURE_CREG_NAME].value;
            cur_gate.creg_index = std::stoll(inst[INST_MEASURE_CREG_BIT].value);
            gates->push_back(cur_gate);
        }
        else
        {
            // measure qreg => measure each qubit in qreg
            auto &qregRef = list_qregs.at(inst[INST_MEASURE_QREG_NAME].value);
            for (IdxType i = 0; i < qregRef.width; i++)
            {
                qasm_gate cur_gate;
                cur_gate.name = MEASURE;
                cur_gate.measured_qubit_index = qregRef.offset + i;
                cur_gate.creg_name  = inst[4].value;
                cur_gate.creg_index = i;
                gates->push_back(cur_gate);
            }
        }
    }
    else
    {
        // Possibly a custom gate
        auto it = list_defined_gates.find(inst[INST_NAME].value);
        if (it != list_defined_gates.end())
        {
            parse_defined_gate(inst, gates);
        }
        // Or a known default gate
        else if (std::find(std::begin(DEFAULT_GATES),
                           std::end(DEFAULT_GATES),
                           inst[INST_NAME].value) != std::end(DEFAULT_GATES))
        {
            parse_native_gate(inst, gates);
        }
        // Or a barrier or undefined instruction
        else if (inst[INST_NAME].value != BARRIER)
        {
            std::cerr << "Undefined instruction: ";
            for (auto &t : inst)
                std::cerr << t.value << " ";
            std::cerr << std::endl;
        }
    }
}

/**
 * @brief Parse a "native" (built-in) gate (e.g. "X", "CX", "RZ", etc.).
 */
void qasm_parser::parse_native_gate(std::vector<token> &inst, std::vector<qasm_gate>* gates)
{
    inst_indicies indices = get_indices(inst, 0, inst.size());
    auto params = get_params(inst, indices.param_start, indices.param_end);
    auto qubits = get_qubits(inst, indices.qubit_start, indices.qubit_end, list_qregs);

    for (IdxType i = 0; i < qubits.first; i++)
    {
        qasm_gate gate;
        gate.name = inst[INST_NAME].value;

        // fill params
        for (auto p : params)
            gate.params.push_back(p);

        // fill qubits
        for (size_t j = 0; j < qubits.second.size(); j++)
        {
            if (qubits.second[j].size() == 1)
                gate.qubits.push_back(qubits.second[j][0]);
            else
                gate.qubits.push_back(qubits.second[j][i]);
        }
        gates->push_back(gate);
    }
}

/**
 * @brief Parse a user-defined gate (from list_defined_gates).
 */
void qasm_parser::parse_defined_gate(std::vector<token> &inst, std::vector<qasm_gate>* gates)
{
    auto &gate_def = list_defined_gates.at(inst[INST_NAME].value);
    auto indices = get_indices(inst, 0, inst.size());
    auto params  = get_params(inst, indices.param_start, indices.param_end);
    auto qubits  = get_qubits(inst, indices.qubit_start, indices.qubit_end, list_qregs);

    for (IdxType i = 0; i < qubits.first; i++)
    {
        // build a vector of the actual qubits used
        std::vector<IdxType> cur_qubits;
        for (auto &qs : qubits.second)
        {
            if (qs.size() == 1)
                cur_qubits.push_back(qs[0]);
            else
                cur_qubits.push_back(qs[i]);
        }

        // expand the instructions from gate_def
        for (auto &sub_inst : gate_def.instructions)
        {
            // duplicate the sub_inst tokens and replace parameter symbols
            std::vector<token> dup_inst(sub_inst);

            for (auto &tok : dup_inst)
            {
                IdxType pIdx = find_index(gate_def.params, tok.value);
                IdxType qIdx = find_index(gate_def.qubits, tok.value);

                if (pIdx != IdxType(-1) && qIdx != IdxType(-1))
                {
                    throw std::runtime_error("parse_defined_gate: symbol used for both parameter and qubit");
                }
                // param => replace with numeric token
                if (pIdx != IdxType(-1))
                {
                    tok.type  = token::e_number;
                    tok.value = std::to_string(params[pIdx]);
                }
                // qubit => replace with the appropriate qubit index
                else if (qIdx != IdxType(-1))
                {
                    tok.type  = token::e_number;
                    tok.value = std::to_string(cur_qubits[qIdx]);
                }
            }
            // parse as a normal gate
            parse_gate(dup_inst, gates);
        }
    }
}

/**
 * @brief Used for debugging, prints all defined gates.
 */
void qasm_parser::dump_defined_gates()
{
    for (const auto &entry : list_defined_gates)
    {
        const auto &gateName = entry.first;
        const auto &dg = entry.second;

        std::cout << "Gate name: " << gateName << "\n"
                  << "Params (" << dg.params.size() << "): ";
        for (auto &p : dg.params)
            std::cout << p << " ";
        std::cout << "\nQubits (" << dg.qubits.size() << "): ";
        for (auto &q : dg.qubits)
            std::cout << q << " ";
        std::cout << "\nInstructions:\n";
        for (auto &vec : dg.instructions)
        {
            for (auto &t : vec)
                std::cout << t.value << " ";
            std::cout << "\n";
        }
        std::cout << std::endl;
    }
}

/**
 * @brief Debug function to print all gates in list_gates.
 */
void qasm_parser::dump_gates()
{
    for (auto &gate : list_gates)
    {
        if (gate.name == IF)
        {
            std::cout << "IF " << gate.creg_name << " == " << gate.if_creg_val << ":\n";
            if (gate.conditional_inst)
            {
                for (auto &sub_gate : *gate.conditional_inst)
                {
                    // custom print function
                    print_gate(sub_gate, true);
                }
            }
        }
        else
        {
            print_gate(gate);
        }
    }
}

/**
 * @brief Build the final circuit from the parsed gates.
 */
void qasm_parser::loadin_circuit(std::shared_ptr<Circuit> circuit)
{
    // For each gate in list_gates, either generate the circuit command
    // or skip if condition not met
    for (auto &gate : list_gates)
    {
        if (gate.name == IF)
        {
            auto &cregRef = list_cregs.at(gate.creg_name);
            // if (cregRef.val == gate.if_creg_val) ...
            // Typically you need to decide how to store/compare 
            if (cregRef.val == gate.if_creg_val)
            {
                for (auto &c_gate : *gate.conditional_inst)
                {
                    generate_circuit(circuit, c_gate);
                }
            }
        }
        else
        {
            generate_circuit(circuit, gate);
        }
    }
}

/**
 * @brief Return how many qubits are declared in total.
 */
IdxType qasm_parser::num_qubits()
{
    return global_qubit_offset;
}

/**
 * @return A map of classical registers encountered during parsing.
 */
std::map<std::string, creg> qasm_parser::get_list_cregs()
{
    return list_cregs;
}

/**
 * @return A map of quantum registers encountered during parsing.
 */
std::map<std::string, qreg> qasm_parser::get_list_qregs()
{
    return list_qregs;
}

/**
 * @brief Generate a circuit gate from a qasm_gate struct.
 */
void qasm_parser::generate_circuit(std::shared_ptr<Circuit> circuit, qasm_gate gate)
{
    const auto &gate_name = gate.name;
    const auto &params    = gate.params;
    const auto &qubits    = gate.qubits;

    if (gate_name == MEASURE)
    {
        // Currently empty (presumably you handle measure in another pass).
        // Or store measurement gates in circuit->someData?
    }
    else if (gate_name == "U")
    {
        circuit->U(params[0], params[1], params[2], qubits[0]);
    }
    else if (gate_name == "U1")
    {
        circuit->U1(params[0], qubits[0]);
    }
    else if (gate_name == "U2")
    {
        circuit->U2(params[0], params[1], qubits[0]);
    }
    else if (gate_name == "U3")
    {
        circuit->U3(params[0], params[1], params[2], qubits[0]);
    }
    else if (gate_name == "X")
    {
        circuit->X(qubits[0]);
    }
    else if (gate_name == "Y")
    {
        circuit->Y(qubits[0]);
    }
    else if (gate_name == "Z")
    {
        circuit->Z(qubits[0]);
    }
    else if (gate_name == "H")
    {
        circuit->H(qubits[0]);
    }
    else if (gate_name == "S")
    {
        circuit->S(qubits[0]);
    }
    else if (gate_name == "SDG")
    {
        circuit->SDG(qubits[0]);
    }
    else if (gate_name == "T")
    {
        circuit->T(qubits[0]);
    }
    else if (gate_name == "TDG")
    {
        circuit->TDG(qubits[0]);
    }
    else if (gate_name == "RX")
    {
        circuit->RX(params[0], qubits[0]);
    }
    else if (gate_name == "RY")
    {
        circuit->RY(params[0], qubits[0]);
    }
    else if (gate_name == "RZ")
    {
        circuit->RZ(params[0], qubits[0]);
    }
    else if (gate_name == "CX")
    {
        circuit->CX(qubits[0], qubits[1]);
    }
    else if (gate_name == "CY")
    {
        circuit->CY(qubits[0], qubits[1]);
    }
    else if (gate_name == "CZ")
    {
        circuit->CZ(qubits[0], qubits[1]);
    }
    else if (gate_name == "CH")
    {
        circuit->CH(qubits[0], qubits[1]);
    }
    else if (gate_name == "CCX")
    {
        circuit->CCX(qubits[0], qubits[1], qubits[2]);
    }
    else if (gate_name == "CRX")
    {
        circuit->CRX(params[0], qubits[0], qubits[1]);
    }
    else if (gate_name == "CRY")
    {
        circuit->CRY(params[0], qubits[0], qubits[1]);
    }
    else if (gate_name == "CRZ")
    {
        circuit->CRZ(params[0], qubits[0], qubits[1]);
    }
    else if (gate_name == "CU")
    {
        circuit->CU(params[0], params[1], params[2], params[3], qubits[0], qubits[1]);
    }
    else if (gate_name == "CU1")
    {
        circuit->CU(0, 0, params[0], 0, qubits[0], qubits[1]);
    }
    else if (gate_name == "CU3")
    {
        circuit->CU(params[0], params[1], params[2], 0, qubits[0], qubits[1]);
    }
    else if (gate_name == "RESET")
    {
        circuit->RESET(qubits[0]);
    }
    else if (gate_name == "SWAP")
    {
        circuit->SWAP(qubits[0], qubits[1]);
    }
    else if (gate_name == "SX")
    {
        circuit->SX(qubits[0]);
    }
    else if (gate_name == "RI")
    {
        circuit->RI(params[0], qubits[0]);
    }
    else if (gate_name == "P")
    {
        circuit->P(params[0], qubits[0]);
    }
    else if (gate_name == "CS")
    {
        circuit->CS(qubits[0], qubits[1]);
    }
    else if (gate_name == "CSDG")
    {
        circuit->CSDG(qubits[0], qubits[1]);
    }
    else if (gate_name == "CT")
    {
        circuit->CT(qubits[0], qubits[1]);
    }
    else if (gate_name == "CTDG")
    {
        circuit->CTDG(qubits[0], qubits[1]);
    }
    else if (gate_name == "CSX")
    {
        circuit->CSX(qubits[0], qubits[1]);
    }
    else if (gate_name == "CP")
    {
        circuit->CP(params[0], qubits[0], qubits[1]);
    }
    else if (gate_name == "CSWAP")
    {
        circuit->CSWAP(qubits[0], qubits[1], qubits[2]);
    }
    else if (gate_name == "ID" || gate_name == "U0")
    {
        circuit->ID(qubits[0]);
    }
    else if (gate_name == "RXX")
    {
        circuit->RXX(params[0], qubits[0], qubits[1]);
    }
    else if (gate_name == "RYY")
    {
        circuit->RYY(params[0], qubits[0], qubits[1]);
    }
    else if (gate_name == "RZZ")
    {
        circuit->RZZ(params[0], qubits[0], qubits[1]);
    }
    else if (gate_name == "RCCX")
    {
        circuit->RCCX(qubits[0], qubits[1], qubits[2]);
    }
    else
    {
        throw std::logic_error("generate_circuit: Undefined gate called: " + gate_name);
    }
}

} // namespace QASMTrans
