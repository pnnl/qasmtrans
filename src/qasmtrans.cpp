#include <iostream>
#include <memory>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>

#include "../include/QASMTransPrimitives.hpp"
#include "../include/IR/chip.hpp"
#include "../include/parser/parser_util.hpp"
#include "../include/parser/qasm_parser.hpp"
#include "../include/circuit_passes/transpiler.hpp"

using namespace QASMTrans;

/**
 * @brief Enum for modes to improve readability
 */
enum Mode { IBMQ = 0, IonQ, Quantinuum, Rigetti, Quafu };

/**
 * @brief Prints help/usage information.
 */
void print_help()
{
    std::cout << "Usage: ./qasmtrans [options]\n"
              << "Options:\n"
              << "  -i                Input qasm circuit file\n"
              << "  -c <backend>      Path to backend configuration JSON file\n"
              << "  -limited          Limit qubit usage for the circuit\n"
              << "  -backend_list     Print the available device backends\n"
              << "  -m <name>         Set the target device (default: ibmq)\n"
              << "  -v <0/1/2>        Set the verbosity level (default: 0)\n"
              << "  -o <path>         Set the output file path (default: data/output/transpiled_modename_filename.qasm)\n"
              << "  -h                Print this help message\n";
}

/**
 * @brief Prints a list of known backends.
 */
void print_backends(const std::map<std::string, IdxType>& machineQubits)
{
    std::cout << "Available backends:\n";
    for (const auto &[name, qubits] : machineQubits)
    {
        std::cout << "  " << name << " (" << qubits << " qubits)\n";
    }
    std::cout << "You can manually add new devices in the JSON file at data/device\n";
}

/**
 * @brief Converts a string mode name to its corresponding enum.
 */
Mode parse_mode(const std::string &mode_name)
{
    static const std::map<std::string, Mode> modeMap = {
        {"ibmq", IBMQ}, {"IBMQ", IBMQ},
        {"ionq", IonQ}, {"IonQ", IonQ},
        {"quantinuum", Quantinuum}, {"Quantinuum", Quantinuum},
        {"rigetti", Rigetti}, {"Rigetti", Rigetti},
        {"quafu", Quafu}, {"Quafu", Quafu}
    };

    auto it = modeMap.find(mode_name);
    if (it == modeMap.end())
    {
        throw std::invalid_argument("Invalid mode name: " + mode_name);
    }
    return it->second;
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        print_help();
        return 0;
    }

    // Variables for config
    bool run_with_limit = false;
    Mode mode = Mode::IBMQ;
    std::string mode_name = "ibmq";
    IdxType debug_level = 0;
    std::string output_path = "../data/output/";

    // Known backends
    std::map<std::string, IdxType> machineQubits = {
        {"ibmq_toronto", 27},
        {"ibmq_jakarta", 7},
        {"ibmq_guadalupe", 16},
        {"ibm_seattle", 433},
        {"ibm_cairo", 27},
        {"ibm_brisbane", 127},
        {"ibmq_dummy12", 12},
        {"ibmq_dummy14", 14},
        {"ibmq_dummy15", 15},
        {"ibmq_dummy16", 16},
        {"ibmq_dummy30", 30},
        {"rigetti_aspen_m3", 80},
        {"quantinuum_h1_2", 12},
        {"quantinuum_h1_1", 20}
    };

    // Process command-line arguments
    if (cmdOptionExists(argv, argv + argc, "-h"))
    {
        print_help();
        return 0;
    }
    if (cmdOptionExists(argv, argv + argc, "-backend_list"))
    {
        print_backends(machineQubits);
        return 0;
    }
    if (cmdOptionExists(argv, argv + argc, "-limited"))
    {
        run_with_limit = true;
    }
    if (cmdOptionExists(argv, argv + argc, "-v"))
    {
        try
        {
            debug_level = std::stoi(getCmdOption(argv, argv + argc, "-v"));
        }
        catch (const std::exception &e)
        {
            std::cerr << "Invalid verbosity level: " << e.what() << "\n";
            return 1;
        }
    }
    if (cmdOptionExists(argv, argv + argc, "-o"))
    {
        output_path = getCmdOption(argv, argv + argc, "-o");
    }
    if (cmdOptionExists(argv, argv + argc, "-m"))
    {
        try
        {
            mode_name = getCmdOption(argv, argv + argc, "-m");
            mode = parse_mode(mode_name);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    // Check required arguments
    if (!cmdOptionExists(argv, argv + argc, "-i") || !cmdOptionExists(argv, argv + argc, "-c"))
    {
        std::cerr << "Error: Missing required option(s) '-i' (input file) or '-c' (backend file).\n";
        print_help();
        return 1;
    }

    const char *filename    = getCmdOption(argv, argv + argc, "-i");
    const char *backendpath = getCmdOption(argv, argv + argc, "-c");

    // Validate input file existence
    {
        std::ifstream infile(filename);
        if (!infile)
        {
            std::cerr << "Error: Input file '" << filename << "' not found or inaccessible.\n";
            return 1;
        }
    }
    // Validate backend file existence
    {
        std::ifstream backendfile(backendpath);
        if (!backendfile)
        {
            std::cerr << "Error: Backend file '" << backendpath << "' not found or inaccessible.\n";
            return 1;
        }
    }

    try
    {
        // =====================
        //  Parsing
        // =====================
        qasm_parser parser(filename);
        IdxType n_qubits = parser.num_qubits();
        auto circuit = std::make_shared<Circuit>(n_qubits);
        parser.loadin_circuit(circuit);

        auto chip = constructChip(n_qubits, backendpath, run_with_limit, debug_level);

        // Optional debug info
        if (debug_level > 0)
        {
            std::cout << "======== QASMTrans ========\n"
                      << "Input circuit: " << filename << " (" << n_qubits << " qubits)\n"
                      << "Backend: " << backendpath << " (" << chip->chip_qubit_num << " qubits)\n"
                      << "Limit mode: " << (run_with_limit ? "True" : "False") << "\n";
        }

        if (circuit->is_empty())
        {
            throw std::runtime_error("Input circuit is empty.");
        }

        // =====================
        //  Transpilation
        // =====================
        transpiler(circuit, chip, parser.get_list_cregs(), debug_level, mode);

        // =====================
        //  Output
        // =====================
        dumpQASM(circuit, filename, output_path, debug_level, mode);
        std::cout << "Output saved to: " << output_path << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during processing: " << e.what() << "\n";
        return 1;
    }

    return 0;
}