#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../include/QASMTransPrimitives.hpp"
#include "../include/cli_pipeline.hpp"
#include "../include/cli_support.hpp"

using namespace QASMTrans;
using namespace QASMTrans::cli;

namespace QASMTrans
{
    std::unordered_set<std::string> g_device_basis_gates;
    std::unordered_map<std::string, std::string> g_merged_gate_aliases;
}

namespace
{
    void print_help()
    {
        std::cout << "Usage: ./qasmtrans [options]" << std::endl;
        std::cout << "Option            Description" << std::endl;
        std::cout << "-i <path>         Input QASM file (repeat -i for multiple circuits)" << std::endl;
        std::cout << "-c <backend>      Path to backend configuration json file" << std::endl;
        std::cout << "-limited          Restrict physical qubit usage to the circuit size; may add routing cost" << std::endl;
        std::cout << "-backend_list     Print the available device backends" << std::endl;
        std::cout << "-m <name>         Set the transpiler targeted device (ibmq, ionq, quantinuum, rigetti, quafu, iqm), default is ibmq" << std::endl;
        std::cout << "-v <0/1/2>        Set the output level, default is 0" << std::endl;
        std::cout << "-full_fidelity    Score Mapomatic candidates on the entire circuit instead of its critical path" << std::endl;
        std::cout << "-cp_mode <product|hybrid>  Choose scoring strategy (default product)" << std::endl;
        std::cout << "-mapomatic_limit <N>       Limit the number of candidate embeddings Mapomatic evaluates (default 1000)" << std::endl;
        std::cout << "--enable_mapomatic        Run the calibration-aware Mapomatic pass" << std::endl;
        std::cout << "--disable_mapomatic       Skip the calibration-aware Mapomatic pass (default)" << std::endl;
        std::cout << "-o <path>         Set the output file, default is data/output/transpiled_modename_filename.qasm" << std::endl;
        std::cout << "-p <path>         Pulse template json (optional; enables pulse dumping)" << std::endl;
        std::cout << "--merge-allow-params     Include parameterised logical gates as merge candidates (default)" << std::endl;
        std::cout << "--merge-disallow-params  Exclude parameterised logical gates from merge candidate analysis" << std::endl;
        std::cout << "--optimize-1q            Enable simple single-qubit consolidation pass" << std::endl;
        std::cout << "--identity-layout        Use identity logical-to-physical layout" << std::endl;
        std::cout << "--routing-mode <default|exec-window>  Select routing heuristic (default: default)" << std::endl;
        std::cout << "-h                print the help function" << std::endl;
    }

    void print_backend_list()
    {
        static const std::vector<std::string> backends = {
            "ibmq_toronto (27 qubits)",
            "ibmq_jakarta (7 qubits)",
            "ibmq_guadalupe (16 qubits)",
            "ibm_seattle (433 qubits)",
            "ibm_cairo (27 qubits)",
            "ibm_brisbane (127 qubits)",
            "aspen_m3 (80 qubits)",
            "h1_2 (12 qubits)",
            "h1_1 (20 qubits)",
            "dummy_ibmq12 (12 qubits)",
            "dummy_ibmq14 (14 qubits)",
            "dummy_ibmq15 (15 qubits)",
            "dummy_ibmq16 (16 qubits)",
            "dummy_ibmq30 (30 qubits)",
        };

        std::cout << "The available backends are:" << std::endl;
        for (const auto &backend : backends)
        {
            std::cout << backend << std::endl;
        }
        std::cout << "You can manually add new machine in json file at data/device" << std::endl;
    }

    bool has_option(int argc, char **argv, const std::string &option)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (argv[i] && option == argv[i])
            {
                return true;
            }
        }
        return false;
    }

    std::string lower_ascii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch)
                       { return static_cast<char>(std::tolower(ch)); });
        return value;
    }

    const char *require_value(int argc, char **argv, int &index, const std::string &option, int &exit_code)
    {
        if (index + 1 >= argc)
        {
            std::cerr << "Error: " << option << " requires a value." << std::endl;
            exit_code = 1;
            return nullptr;
        }
        return argv[++index];
    }

    bool parse_cli(int argc, char **argv, CliConfig &config, int &exit_code)
    {
        if (argc == 1 || has_option(argc, argv, "-h"))
        {
            print_help();
            exit_code = 0;
            return false;
        }

        for (int argi = 1; argi < argc; ++argi)
        {
            std::string current = argv[argi];
            if (current == "-backend_list")
            {
                print_backend_list();
                exit_code = 0;
                return false;
            }
            if (current == "-limited")
            {
                config.run_with_limit = true;
                continue;
            }
            if (current == "-full_fidelity")
            {
                config.use_full_fidelity = true;
                continue;
            }
            if (current == "--enable_mapomatic")
            {
                config.disable_mapomatic = false;
                continue;
            }
            if (current == "--disable_mapomatic")
            {
                config.disable_mapomatic = true;
                continue;
            }
            if (current == "--merge-disallow-params")
            {
                config.allow_parameterized_merge_candidates = false;
                continue;
            }
            if (current == "--merge-allow-params")
            {
                config.allow_parameterized_merge_candidates = true;
                continue;
            }
            if (current == "--optimize-1q")
            {
                config.enable_1q_opt = true;
                continue;
            }
            if (current == "--identity-layout")
            {
                config.force_identity_layout = true;
                continue;
            }

            auto next_value = [&]() -> const char *
            {
                return require_value(argc, argv, argi, current, exit_code);
            };

            if (current == "-i")
            {
                const char *value = next_value();
                if (!value)
                {
                    return false;
                }
                config.input_files.emplace_back(value);
                continue;
            }
            if (current == "-c")
            {
                const char *value = next_value();
                if (!value)
                {
                    return false;
                }
                config.backendpath = value;
                continue;
            }
            if (current == "-m")
            {
                const char *value = next_value();
                if (!value)
                {
                    return false;
                }
                config.mode_name = value;
                try
                {
                    config.mode = mode_from_string(config.mode_name);
                }
                catch (const std::exception &)
                {
                    std::cout << "Invalid mode name, please check" << std::endl;
                    exit_code = 0;
                    return false;
                }
                continue;
            }
            if (current == "-o")
            {
                const char *value = next_value();
                if (!value)
                {
                    return false;
                }
                config.output_path = value;
                continue;
            }
            if (current == "-p")
            {
                const char *value = next_value();
                if (!value)
                {
                    return false;
                }
                config.pulse_template_path = value;
                continue;
            }
            if (current == "-v")
            {
                const char *value = next_value();
                if (!value)
                {
                    return false;
                }
                config.debug_level = IdxType(std::stoi(value));
                continue;
            }
            if (current == "-cp_mode")
            {
                const char *value = next_value();
                if (!value)
                {
                    return false;
                }
                std::string lowered = lower_ascii(value);
                if (lowered == "product" || lowered == "log" || lowered == "multiplicative")
                {
                    config.cp_mode = CliCriticalPathMode::LogProduct;
                }
                else if (lowered == "hybrid")
                {
                    config.cp_mode = CliCriticalPathMode::Hybrid;
                }
                else
                {
                    std::cerr << "Error: unknown -cp_mode value '" << value
                              << "'. Expected 'product' or 'hybrid'." << std::endl;
                    exit_code = 1;
                    return false;
                }
                continue;
            }
            if (current == "-mapomatic_limit")
            {
                const char *value = next_value();
                if (!value)
                {
                    return false;
                }
                try
                {
                    long long parsed = std::stoll(value);
                    if (parsed <= 0)
                    {
                        std::cerr << "Error: -mapomatic_limit must be greater than zero (got " << parsed << ")." << std::endl;
                        exit_code = 1;
                        return false;
                    }
                    config.mapomatic_limit = static_cast<std::size_t>(parsed);
                }
                catch (const std::exception &)
                {
                    std::cerr << "Error: failed to parse -mapomatic_limit argument '" << value << "'." << std::endl;
                    exit_code = 1;
                    return false;
                }
                continue;
            }
            if (current == "--routing-mode")
            {
                const char *value = next_value();
                if (!value)
                {
                    return false;
                }
                std::string lowered = lower_ascii(value);
                if (lowered == "default" || lowered == "sabre")
                {
                    config.routing_mode = CliRoutingMode::Sabre;
                }
                else if (lowered == "exec-window" || lowered == "exec_window")
                {
                    config.routing_mode = CliRoutingMode::ExecWindow;
                }
                else
                {
                    std::cerr << "Error: unknown --routing-mode value '" << value
                              << "'. Expected 'default' or 'exec-window'." << std::endl;
                    exit_code = 1;
                    return false;
                }
                continue;
            }
        }

        if (config.input_files.empty())
        {
            std::cerr << "Error: missing input QASM file(s) (-i)." << std::endl;
            exit_code = 1;
            return false;
        }
        if (config.backendpath.empty())
        {
            std::cerr << "Error: missing machine backend file via -c" << std::endl;
            exit_code = 1;
            return false;
        }
        exit_code = 0;
        return true;
    }
} // namespace

int main(int argc, char **argv)
{
    CliConfig config;
    int early_exit = 0;
    if (!parse_cli(argc, argv, config, early_exit))
    {
        return early_exit;
    }

    try
    {
        cpu_timer backend_metadata_timer;
        backend_metadata_timer.start_timer();
        ingest_backend_metadata(config.backendpath, g_device_basis_gates, g_merged_gate_aliases);
        backend_metadata_timer.stop_timer();

        ExecutionPipeline pipeline = select_execution_pipeline(config);
        if (pipeline == ExecutionPipeline::BatchEnhanced)
        {
            return run_batch_pipeline(config);
        }
        return run_single_circuit_pipeline(config, pipeline, backend_metadata_timer.measure());
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
