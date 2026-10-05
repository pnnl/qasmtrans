#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <ostream>
#include <unistd.h>

#include "QASMTransPrimitives.hpp"
#include "cli_support.hpp"
#include "circuit_passes/transpiler.hpp"
#include "dump_pulses.hpp"
#include "dump_qasm.hpp"
#include "IR/chip.hpp"
#include "parser/qasm_parser.hpp"

namespace py = pybind11;
namespace fs = std::filesystem;

// Module-local globals for shared headers.
namespace QASMTrans
{
    std::unordered_set<std::string> g_device_basis_gates;
    std::unordered_map<std::string, std::string> g_merged_gate_aliases;
}

struct TranspileOptions
{
    std::string mode = "ibmq";
    std::string backend_config;       // Path to backend JSON (required)
    std::string pulse_template_path;  // Optional pulse template JSON
    std::string output_path;          // Optional QASM output path (file or directory)
    std::string pulse_output_path;    // Optional pulse JSON output path
    bool limited_qubits = false;
    bool disable_mapomatic = true;
    bool full_fidelity = false;
    bool allow_parameterized_merge = true;
    bool optimize_1q = false;
    std::string routing_mode;
    std::size_t mapomatic_limit = 1000;
    std::vector<IdxType> initial_layout;
    int verbose = 0;
};

struct TranspileResult
{
    std::string output_qasm;      // Transpiled OpenQASM text
    std::string output_qasm_path; // Where the QASM was written
    std::string pulse_schedule;   // JSON pulse schedule (when available)
    py::object pulse_doc;         // Parsed pulse JSON (None if not produced)
    std::string pulse_path;       // Where the pulse JSON was written (if any)
    std::string log;              // Aggregated log/output
    std::vector<IdxType> initial_layout_used; // Logical -> physical mapping at routing start
    std::vector<IdxType> logical_to_physical;  // Logical -> physical mapping after routing
    std::vector<IdxType> measurement_mapping;  // Mapping used for final measurement ordering

    TranspileResult()
        : pulse_doc(py::none())
    {
    }
};

namespace
{
    using QASMTrans::g_device_basis_gates;
    using QASMTrans::g_merged_gate_aliases;

    struct StreamCapture
    {
        std::ostringstream out;
        std::ostringstream err;
        std::streambuf *old_out = nullptr;
        std::streambuf *old_err = nullptr;

        StreamCapture()
        {
            old_out = std::cout.rdbuf(out.rdbuf());
            old_err = std::cerr.rdbuf(err.rdbuf());
        }

        StreamCapture(const StreamCapture &) = delete;
        StreamCapture &operator=(const StreamCapture &) = delete;

        ~StreamCapture()
        {
            stop();
        }

        void stop()
        {
            if (old_out)
            {
                std::cout.rdbuf(old_out);
                old_out = nullptr;
            }
            if (old_err)
            {
                std::cerr.rdbuf(old_err);
                old_err = nullptr;
            }
        }
    };

    struct TempQasmInput
    {
        fs::path path;
        bool temporary = false;

        explicit TempQasmInput(const std::string &qasm_source)
        {
            const bool looks_like_path = qasm_source.find('\n') == std::string::npos &&
                                         qasm_source.find('\r') == std::string::npos &&
                                         fs::exists(qasm_source);
            if (looks_like_path)
            {
                path = fs::absolute(qasm_source);
                return;
            }

            auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            auto pid = static_cast<long long>(::getpid());
            std::ostringstream name;
            name << "qasmtrans_input_" << pid << "_" << now << ".qasm";
            path = fs::temp_directory_path() / fs::path(name.str());

            std::ofstream temp_out(path);
            if (!temp_out.is_open())
            {
                throw std::runtime_error("Failed to create temporary QASM file at " + path.string());
            }
            temp_out << qasm_source;
            temporary = true;
        }

        TempQasmInput(const TempQasmInput &) = delete;
        TempQasmInput &operator=(const TempQasmInput &) = delete;

        ~TempQasmInput()
        {
            if (temporary)
            {
                std::error_code ec;
                fs::remove(path, ec);
            }
        }
    };

    std::string derive_pulse_output_path(const std::string &qasm_output_path,
                                         const std::string &supplied)
    {
        if (!supplied.empty())
        {
            return supplied;
        }
        return QASMTrans::cli::derive_pulse_output_path(qasm_output_path);
    }

    TranspileResult transpile_qasm(const std::string &qasm_source, const TranspileOptions &options)
    {
        if (options.backend_config.empty())
        {
            throw std::invalid_argument("backend_config is required");
        }

        IdxType mode = QASMTrans::cli::mode_from_string(options.mode);
        QASMTrans::cli::ingest_backend_metadata(options.backend_config,
                                                g_device_basis_gates,
                                                g_merged_gate_aliases,
                                                true);

        TempQasmInput input(qasm_source);
        const fs::path &input_path = input.path;

        TranspileResult result;
        std::ostringstream log;
        StreamCapture capture;

        qasm_parser parser(input_path.c_str());
        IdxType n_qubits = parser.num_qubits();
        auto circuit = std::make_shared<Circuit>(n_qubits);
        parser.loadin_circuit(circuit);
        auto cregs = parser.get_list_cregs();
        if (!options.initial_layout.empty())
        {
            if (options.initial_layout.size() != static_cast<std::size_t>(n_qubits))
            {
                throw std::invalid_argument("initial_layout must have exactly one entry per logical qubit");
            }
            std::vector<IdxType> seen(n_qubits, 0);
            for (IdxType physical : options.initial_layout)
            {
                if (physical < 0 || physical >= n_qubits)
                {
                    throw std::invalid_argument("initial_layout contains an out-of-range physical qubit index");
                }
                if (seen[physical] != 0)
                {
                    throw std::invalid_argument("initial_layout must be a permutation without duplicates");
                }
                seen[physical] = 1;
            }
            circuit->set_mapping(options.initial_layout);
        }

        const bool needs_enhanced_pipeline =
            !options.disable_mapomatic ||
            !options.pulse_template_path.empty();
        auto chip = constructChip(n_qubits,
                                  options.backend_config,
                                  options.limited_qubits,
                                  options.verbose,
                                  needs_enhanced_pipeline ? ChipMetadataMode::Full
                                                          : ChipMetadataMode::TopologyOnly);
        if (!chip)
        {
            throw std::runtime_error("Failed to construct device from backend config");
        }

        CriticalPathHeuristicMode cp_mode = CriticalPathHeuristicMode::LogProduct;
        RoutingMode routing_mode = RoutingMode::Sabre;
        if (!options.routing_mode.empty())
        {
            std::string mode_value = options.routing_mode;
            std::transform(mode_value.begin(), mode_value.end(), mode_value.begin(),
                           [](unsigned char ch)
                           { return static_cast<char>(std::tolower(ch)); });
            if (mode_value != "sabre" && mode_value != "fast")
            {
                throw std::invalid_argument("Unknown routing_mode '" + options.routing_mode + "'. Use 'sabre'.");
            }
        }
        transpiler(circuit,
                   chip,
                   cregs,
                   options.verbose,
                   mode,
                   options.full_fidelity,
                   cp_mode,
                   options.disable_mapomatic,
                   options.mapomatic_limit,
                   options.optimize_1q,
                   routing_mode,
                   needs_enhanced_pipeline ? TranspilerProfile::Enhanced
                                           : TranspilerProfile::Baseline,
                   g_device_basis_gates);

        result.initial_layout_used = circuit->routed_initial_mapping_view();
        result.logical_to_physical = circuit->get_mapping();

        std::vector<IdxType> measurement = QASMTrans::cli::build_measurement_mapping(cregs, circuit->get_mapping());
        if (!measurement.empty())
        {
            circuit->set_mapping(measurement);
            result.measurement_mapping = measurement;
        }

        std::vector<QASMTrans::Gate> expanded_gates;
        bool rigetti_mode = false;
        if (!options.pulse_template_path.empty())
        {
            expanded_gates = QASMTrans::pulses::expandGatesForPulseDump(circuit,
                                                                         options.backend_config,
                                                                         options.pulse_template_path,
                                                                         options.allow_parameterized_merge,
                                                                         &rigetti_mode);
        }

        std::string qasm_path;
        if (!expanded_gates.empty() && rigetti_mode)
        {
            qasm_path = dumpQASMFromGates(circuit,
                                          expanded_gates,
                                          input_path.filename().c_str(),
                                          options.output_path,
                                          options.verbose,
                                          mode);
        }
        else
        {
            qasm_path = dumpQASM(circuit,
                                 input_path.filename().c_str(),
                                 options.output_path,
                                 options.verbose,
                                 mode);
        }
        result.output_qasm_path = qasm_path;
        {
            std::ifstream qasm_in(qasm_path);
            std::stringstream buffer;
            buffer << qasm_in.rdbuf();
            result.output_qasm = buffer.str();
        }

        if (!options.pulse_template_path.empty())
        {
            std::string pulse_path = derive_pulse_output_path(qasm_path, options.pulse_output_path);
            dumpPulses(circuit,
                       input_path.filename().c_str(),
                       options.backend_config,
                       options.pulse_template_path,
                       pulse_path,
                       options.verbose,
                       options.allow_parameterized_merge);
            result.pulse_path = pulse_path;
            std::ifstream pulse_in(pulse_path);
            std::stringstream buffer;
            buffer << pulse_in.rdbuf();
            result.pulse_schedule = buffer.str();
            try
            {
                result.pulse_doc = py::module::import("json").attr("loads")(result.pulse_schedule);
            }
            catch (const std::exception &)
            {
                result.pulse_doc = py::none();
            }
        }

        log << "Transpiled " << input_path.filename().string() << " with mode=" << options.mode
            << ", qubits=" << n_qubits;
        capture.stop();

        result.log = log.str();
        std::string captured = capture.out.str();
        if (!captured.empty())
        {
            result.log += "\n" + captured;
        }
        return result;
    }
} // namespace

PYBIND11_MODULE(qasmtrans_core, m)
{
    m.doc() = "Prototype Python bindings for QASMTrans.";

    py::class_<TranspileOptions>(m, "TranspileOptions", "Configuration options for the transpiler.")
        .def(py::init<>())
        .def_readwrite("mode", &TranspileOptions::mode)
        .def_readwrite("backend_config", &TranspileOptions::backend_config)
        .def_readwrite("pulse_template_path", &TranspileOptions::pulse_template_path)
        .def_readwrite("output_path", &TranspileOptions::output_path)
        .def_readwrite("pulse_output_path", &TranspileOptions::pulse_output_path)
        .def_readwrite("limited_qubits", &TranspileOptions::limited_qubits)
        .def_readwrite("disable_mapomatic", &TranspileOptions::disable_mapomatic)
        .def_readwrite("full_fidelity", &TranspileOptions::full_fidelity)
        .def_readwrite("allow_parameterized_merge", &TranspileOptions::allow_parameterized_merge)
        .def_readwrite("optimize_1q", &TranspileOptions::optimize_1q)
        .def_readwrite("routing_mode", &TranspileOptions::routing_mode)
        .def_readwrite("mapomatic_limit", &TranspileOptions::mapomatic_limit)
        .def_readwrite("initial_layout", &TranspileOptions::initial_layout)
        .def_readwrite("verbose", &TranspileOptions::verbose);

    py::class_<TranspileResult>(m, "TranspileResult", "Outputs from a transpilation run.")
        .def(py::init<>())
        .def_readonly("output_qasm", &TranspileResult::output_qasm)
        .def_readonly("output_qasm_path", &TranspileResult::output_qasm_path)
        .def_readonly("pulse_schedule", &TranspileResult::pulse_schedule)
        .def_readonly("pulse_doc", &TranspileResult::pulse_doc)
        .def_readonly("pulse_path", &TranspileResult::pulse_path)
        .def_readonly("log", &TranspileResult::log)
        .def_readonly("initial_layout_used", &TranspileResult::initial_layout_used)
        .def_readonly("logical_to_physical", &TranspileResult::logical_to_physical)
        .def_readonly("measurement_mapping", &TranspileResult::measurement_mapping);

    m.def(
        "transpile_qasm",
        &transpile_qasm,
        py::arg("qasm_source"),
        py::arg("options") = TranspileOptions{},
        R"pbdoc(
            Transpile an OpenQASM circuit (file path or raw string).

            Parameters
            ----------
            qasm_source:
                Either a path to a QASM file or the QASM program text.
            options:
                TranspileOptions controlling backend mode, qubit limits, and optional pulse emission paths.
        )pbdoc");
}
