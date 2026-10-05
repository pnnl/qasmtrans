#include "../include/cli_pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "../include/cli_support.hpp"
#include "../include/IR/chip.hpp"
#include "../include/circuit_passes/transpiler.hpp"
#include "../include/dump_pulses.hpp"
#include "../include/dump_qasm.hpp"
#include "../include/parser/qasm_parser.hpp"
#include "../include/util/chip_partition.hpp"

using namespace QASMTrans;
using namespace std;
namespace fs = std::filesystem;

namespace QASMTrans::cli
{
namespace
{
    struct SubchipArtifact
    {
        std::size_t index = 0;
        std::string circuit_prefix;
        std::string source_filename;
        std::shared_ptr<Chip> subchip;
        std::vector<IdxType> local_to_global;
        std::vector<Gate> local_gates;
        std::vector<IdxType> logical_mapping;
        std::vector<IdxType> measurement_mapping;
        std::map<std::string, creg> cregs;
    };

    struct CircuitBatch
    {
        std::vector<std::shared_ptr<Circuit>> circuits;
        std::vector<std::map<std::string, creg>> circuit_cregs;
        std::vector<IdxType> circuit_sizes;
    };

    struct CombinedArtifacts
    {
        std::vector<Gate> combined_gates;
        std::vector<IdxType> combined_mapping;
        std::map<std::string, creg> combined_cregs;
        std::vector<SubchipArtifact> subchip_artifacts;
        IdxType routing_swap_count = 0;
    };

    struct OutputArtifacts
    {
        std::string qasm_path;
        std::string pulses_path;
    };

    struct EmitTiming
    {
        double gate_expand_ms = 0.0;
        double qasm_dump_ms = 0.0;
        double pulse_dump_ms = 0.0;
        double total_ms = 0.0;
    };

    bool needs_enhanced_single_pipeline(const CliConfig &config)
    {
        return !config.disable_mapomatic ||
               !config.pulse_template_path.empty();
    }

    ::CriticalPathHeuristicMode to_transpiler_mode(CliCriticalPathMode mode)
    {
        return mode == CliCriticalPathMode::Hybrid
                   ? ::CriticalPathHeuristicMode::Hybrid
                   : ::CriticalPathHeuristicMode::LogProduct;
    }

    ::RoutingMode to_routing_mode(CliRoutingMode mode)
    {
        return mode == CliRoutingMode::ExecWindow
                   ? ::RoutingMode::ExecWindow
                   : ::RoutingMode::Sabre;
    }

    CircuitBatch load_circuits(const std::vector<std::string> &input_files)
    {
        CircuitBatch batch;
        batch.circuits.reserve(input_files.size());
        batch.circuit_cregs.reserve(input_files.size());
        batch.circuit_sizes.reserve(input_files.size());

        for (const auto &file : input_files)
        {
            qasm_parser parser(file.c_str());
            IdxType n_qubits = parser.num_qubits();
            auto circuit = make_shared<Circuit>(n_qubits);
            parser.loadin_circuit(circuit);
            batch.circuits.push_back(circuit);
            batch.circuit_cregs.push_back(parser.get_list_cregs());
            batch.circuit_sizes.push_back(n_qubits);
        }
        return batch;
    }

    std::vector<IdxType> compute_partition_sizes(const std::vector<IdxType> &circuit_sizes,
                                                 IdxType device_capacity,
                                                 IdxType total_requested_qubits)
    {
        std::vector<IdxType> partition_sizes = circuit_sizes;
        if (partition_sizes.empty())
        {
            return partition_sizes;
        }

        const double total_logical = static_cast<double>(total_requested_qubits);
        std::vector<double> remainders(partition_sizes.size(), 0.0);
        IdxType allocated = 0;
        for (std::size_t idx = 0; idx < partition_sizes.size(); ++idx)
        {
            double exact = total_logical > 0.0
                               ? (static_cast<double>(partition_sizes[idx]) / total_logical) * static_cast<double>(device_capacity)
                               : static_cast<double>(partition_sizes[idx]);
            double base = std::floor(exact);
            IdxType candidate = static_cast<IdxType>(base);
            if (candidate < circuit_sizes[idx])
            {
                candidate = circuit_sizes[idx];
            }
            partition_sizes[idx] = candidate;
            remainders[idx] = exact - base;
            allocated += candidate;
        }

        if (allocated < device_capacity)
        {
            IdxType leftover = device_capacity - allocated;
            std::vector<std::size_t> order(partition_sizes.size());
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(),
                      [&](std::size_t lhs, std::size_t rhs)
                      {
                          if (remainders[lhs] != remainders[rhs])
                          {
                              return remainders[lhs] > remainders[rhs];
                          }
                          return partition_sizes[lhs] < partition_sizes[rhs];
                      });
            std::size_t cursor = 0;
            while (leftover > 0 && !order.empty())
            {
                std::size_t idx = order[cursor % order.size()];
                partition_sizes[idx] += 1;
                ++cursor;
                --leftover;
            }
        }
        else if (allocated > device_capacity)
        {
            IdxType overshoot = allocated - device_capacity;
            std::vector<std::size_t> order(partition_sizes.size());
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(),
                      [&](std::size_t lhs, std::size_t rhs)
                      {
                          IdxType slack_lhs = partition_sizes[lhs] - circuit_sizes[lhs];
                          IdxType slack_rhs = partition_sizes[rhs] - circuit_sizes[rhs];
                          if (slack_lhs != slack_rhs)
                          {
                              return slack_lhs > slack_rhs;
                          }
                          return remainders[lhs] < remainders[rhs];
                      });
            for (std::size_t pos = 0; overshoot > 0 && pos < order.size(); ++pos)
            {
                std::size_t idx = order[pos];
                while (overshoot > 0 && partition_sizes[idx] > circuit_sizes[idx])
                {
                    partition_sizes[idx] -= 1;
                    --overshoot;
                }
            }
            if (overshoot > 0)
            {
                throw std::runtime_error("Unable to fit requested circuit sizes within device capacity.");
            }
        }
        return partition_sizes;
    }

    std::vector<std::vector<IdxType>> allocate_partitions(const std::shared_ptr<Chip> &chip,
                                                          const std::vector<IdxType> &partition_sizes,
                                                          IdxType debug_level)
    {
        std::vector<std::vector<IdxType>> partitions;
        try
        {
            partitions = partition_chip(chip, partition_sizes);
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Warning: advanced partitioning failed (" << ex.what()
                      << "), falling back to contiguous allocation." << std::endl;
            partitions.clear();
            partitions.reserve(partition_sizes.size());
            IdxType next_qubit = 0;
            bool fallback_ok = true;
            for (IdxType requested : partition_sizes)
            {
                std::vector<IdxType> part;
                part.reserve(static_cast<std::size_t>(requested));
                for (IdxType offset = 0; offset < requested; ++offset)
                {
                    if (next_qubit >= chip->chip_qubit_num)
                    {
                        fallback_ok = false;
                        break;
                    }
                    part.push_back(next_qubit++);
                }
                if (static_cast<IdxType>(part.size()) != requested)
                {
                    fallback_ok = false;
                    break;
                }
                partitions.push_back(std::move(part));
            }
            if (!fallback_ok || partitions.size() != partition_sizes.size())
            {
                throw std::runtime_error("Error: contiguous fallback partitioning failed.");
            }
        }
        if (debug_level > 0 && !partitions.empty())
        {
            std::cout << "Partitioned device into " << partitions.size() << " subchip(s)." << std::endl;
        }
        return partitions;
    }

    CombinedArtifacts transpile_and_merge(const std::vector<std::shared_ptr<Circuit>> &circuits,
                                          const std::vector<std::map<std::string, creg>> &circuit_cregs,
                                          const std::vector<std::vector<IdxType>> &partitions,
                                          const std::vector<std::string> &input_files,
                                          const std::shared_ptr<Chip> &chip,
                                          const CliConfig &config,
                                          IdxType total_requested_qubits)
    {
        CombinedArtifacts artifacts;
        artifacts.combined_gates.reserve(1024);
        artifacts.combined_mapping.reserve(static_cast<std::size_t>(total_requested_qubits));
        artifacts.subchip_artifacts.reserve(circuits.size());

        const std::size_t prefix_width = circuits.empty() ? 1 : std::to_string(circuits.size() - 1).size();

        for (std::size_t ci = 0; ci < circuits.size(); ++ci)
        {
            auto circuit = circuits[ci];
            const auto &cregs = circuit_cregs[ci];
            if (config.force_identity_layout)
            {
                std::vector<IdxType> identity_layout(static_cast<std::size_t>(circuit->num_qubits()));
                std::iota(identity_layout.begin(), identity_layout.end(), 0);
                circuit->set_mapping(identity_layout);
            }

            std::vector<IdxType> local_to_global;
            auto subchip = make_subchip(chip, partitions[ci], local_to_global);

            if (config.debug_level > 0)
            {
                std::cout << "Circuit " << ci << ": allocated " << partitions[ci].size()
                          << " physical qubits." << std::endl;
            }
            if (config.debug_level > 1)
            {
                std::cout << "  Classical registers:" << std::endl;
                for (const auto &entry : cregs)
                {
                    std::cout << "    " << entry.first << " (width=" << entry.second.width << ") indices:";
                    for (IdxType idx_val : entry.second.qubit_indices)
                    {
                        std::cout << " " << idx_val;
                    }
                    std::cout << std::endl;
                }
            }

            transpiler(circuit,
                       subchip,
                       cregs,
                       config.debug_level,
                       config.mode,
                       config.use_full_fidelity,
                       to_transpiler_mode(config.cp_mode),
                       config.disable_mapomatic,
                       config.mapomatic_limit,
                       config.enable_1q_opt,
                       to_routing_mode(config.routing_mode),
                       TranspilerProfile::Enhanced,
                       g_device_basis_gates);

            artifacts.routing_swap_count += circuit->get_routing_swap_count();

            std::vector<IdxType> local_mapping = circuit->get_mapping();
            std::vector<IdxType> local_measurement = build_measurement_mapping(cregs, local_mapping);
            std::vector<Gate> local_gates = circuit->get_gates();

            SubchipArtifact artifact;
            artifact.index = ci;
            artifact.source_filename = input_files[ci];
            artifact.subchip = subchip;
            artifact.local_to_global = local_to_global;
            artifact.local_gates = local_gates;
            artifact.logical_mapping = local_mapping;
            artifact.measurement_mapping = local_measurement;
            artifact.cregs = cregs;
            artifacts.subchip_artifacts.push_back(std::move(artifact));

            std::size_t classical_bits = 0;
            for (const auto &entry : cregs)
            {
                classical_bits += entry.second.width;
            }
            std::vector<IdxType> global_mapping(local_mapping.size(), -1);
            if (config.debug_level > 0)
            {
                std::cout << "  Logical qubits: " << local_mapping.size()
                          << ", classical bits: " << classical_bits << std::endl;
            }
            for (std::size_t idx = 0; idx < local_mapping.size(); ++idx)
            {
                IdxType local_phys = local_mapping[idx];
                if (local_phys < 0)
                {
                    continue;
                }
                if (local_phys >= static_cast<IdxType>(local_to_global.size()))
                {
                    global_mapping[idx] = local_phys;
                }
                else
                {
                    global_mapping[idx] = local_to_global[static_cast<std::size_t>(local_phys)];
                }
            }

            circuit->set_mapping(global_mapping);
            std::vector<IdxType> global_measurement = build_measurement_mapping(cregs, global_mapping);

            if (config.debug_level > 1)
            {
                std::cout << "  Global mapping:";
                for (IdxType value : global_mapping)
                {
                    std::cout << " " << value;
                }
                std::cout << std::endl;
            }

            std::vector<Gate> gates = circuit->get_gates();
            for (auto &gate : gates)
            {
                if (gate.qubit >= 0)
                {
                    gate.qubit = local_to_global[static_cast<std::size_t>(gate.qubit)];
                }
                if (gate.ctrl >= 0)
                {
                    gate.ctrl = local_to_global[static_cast<std::size_t>(gate.ctrl)];
                }
                if (gate.extra >= 0)
                {
                    gate.extra = local_to_global[static_cast<std::size_t>(gate.extra)];
                }
            }
            circuit->set_gates(gates);

            artifacts.combined_gates.insert(artifacts.combined_gates.end(), gates.begin(), gates.end());

            if (global_measurement.empty())
            {
                global_measurement = global_mapping;
            }
            artifacts.combined_mapping.insert(artifacts.combined_mapping.end(), global_measurement.begin(), global_measurement.end());

            std::ostringstream prefix_builder;
            prefix_builder << "circuit" << std::setw(static_cast<int>(std::max<std::size_t>(2, prefix_width))) << std::setfill('0') << ci << "_";
            const std::string prefix = prefix_builder.str();
            if (!artifacts.subchip_artifacts.empty())
            {
                artifacts.subchip_artifacts.back().circuit_prefix = prefix;
            }

            for (const auto &entry : cregs)
            {
                creg renamed = entry.second;
                renamed.name = prefix + entry.first;
                artifacts.combined_cregs.emplace(renamed.name, renamed);
            }
        }
        return artifacts;
    }

    OutputArtifacts emit_outputs(const CliConfig &config,
                                 const std::shared_ptr<Circuit> &combined_circuit,
                                 const std::vector<std::string> &input_files,
                                 IdxType total_requested_qubits,
                                 const std::shared_ptr<Chip> &chip,
                                 EmitTiming *timing = nullptr)
    {
        OutputArtifacts outputs;
        EmitTiming local_timing;
        auto format_ms = [](double ms)
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << ms;
            return oss.str();
        };
        cpu_timer emit_total_timer;
        emit_total_timer.start_timer();

        if (config.debug_level > 0)
        {
            std::cout << "======== QASMTrans ========" << std::endl;
            std::cout << "Processed " << input_files.size() << " circuit(s) on backend: " << config.backendpath
                      << " (" << chip->chip_qubit_num << " physical qubits)" << std::endl;
            std::cout << "Combined logical qubits: " << total_requested_qubits << std::endl;
            std::cout << "Basis gate mode: " << config.mode_name << std::endl;
            std::cout << "Limit mode: " << (config.run_with_limit ? "True" : "False") << std::endl;
            if (!config.pulse_template_path.empty())
            {
                std::cout << "Pulse template: " << config.pulse_template_path << std::endl;
            }
            else
            {
                std::cout << "Pulse template: (not provided; skipping pulse dump)" << std::endl;
            }
        }

        std::vector<Gate> expanded_gates;
        bool rigetti_mode = false;
        if (!config.pulse_template_path.empty())
        {
            cpu_timer expand_timer;
            expand_timer.start_timer();
            expanded_gates = pulses::expandGatesForPulseDump(combined_circuit,
                                                             config.backendpath,
                                                             config.pulse_template_path,
                                                             config.allow_parameterized_merge_candidates,
                                                             &rigetti_mode);
            expand_timer.stop_timer();
            local_timing.gate_expand_ms = expand_timer.measure();
        }
        if (!expanded_gates.empty() && rigetti_mode)
        {
            cpu_timer qasm_dump_timer;
            qasm_dump_timer.start_timer();
            outputs.qasm_path = dumpQASMFromGates(combined_circuit,
                                                  expanded_gates,
                                                  input_files.front().c_str(),
                                                  config.output_path,
                                                  config.debug_level,
                                                  config.mode);
            qasm_dump_timer.stop_timer();
            local_timing.qasm_dump_ms = qasm_dump_timer.measure();
        }
        else
        {
            cpu_timer qasm_dump_timer;
            qasm_dump_timer.start_timer();
            outputs.qasm_path = dumpQASM(combined_circuit,
                                         input_files.front().c_str(),
                                         config.output_path,
                                         config.debug_level,
                                         config.mode);
            qasm_dump_timer.stop_timer();
            local_timing.qasm_dump_ms = qasm_dump_timer.measure();
        }
        std::cout << "Saving output qasm to: " << outputs.qasm_path << std::endl;

        if (!config.pulse_template_path.empty())
        {
            outputs.pulses_path = derive_pulse_output_path(outputs.qasm_path);
            cpu_timer pulse_dump_timer;
            pulse_dump_timer.start_timer();
            dumpPulses(combined_circuit,
                       input_files.front().c_str(),
                       config.backendpath,
                       config.pulse_template_path,
                       outputs.pulses_path,
                       config.debug_level,
                       config.allow_parameterized_merge_candidates);
            pulse_dump_timer.stop_timer();
            local_timing.pulse_dump_ms = pulse_dump_timer.measure();
            std::cout << "Saving output pulses to: " << outputs.pulses_path << std::endl;
        }
        else if (config.debug_level > 0)
        {
            std::cout << "Pulse template not provided; skipping pulse dump." << std::endl;
        }

        emit_total_timer.stop_timer();
        local_timing.total_ms = emit_total_timer.measure();
        if (timing)
        {
            *timing = local_timing;
        }
        if (config.debug_level > 0)
        {
            std::cout << "CLI emit timing (ms): qasm_dump=" << format_ms(local_timing.qasm_dump_ms)
                      << " pulse_dump=" << format_ms(local_timing.pulse_dump_ms);
            if (local_timing.gate_expand_ms > 0.0)
            {
                std::cout << " gate_expand=" << format_ms(local_timing.gate_expand_ms);
            }
            std::cout << " total=" << format_ms(local_timing.total_ms) << std::endl;
        }

        return outputs;
    }

    void export_subchips(const std::vector<SubchipArtifact> &subchip_artifacts,
                         const OutputArtifacts &outputs,
                         const CliConfig &config)
    {
        fs::path final_output_path(outputs.qasm_path);
        fs::path base_output_dir = final_output_path.has_parent_path() ? final_output_path.parent_path() : fs::current_path();
        fs::path subchip_dir = base_output_dir / (final_output_path.stem().string() + "_subchips");
        std::error_code subchip_ec;
        fs::create_directories(subchip_dir, subchip_ec);
        if (subchip_ec)
        {
            std::cerr << "Warning: failed to create subchip directory '" << subchip_dir << "' (" << subchip_ec.message() << ")" << std::endl;
        }

        for (const auto &artifact : subchip_artifacts)
        {
            std::string cleaned_prefix = artifact.circuit_prefix;
            if (!cleaned_prefix.empty() && cleaned_prefix.back() == '_')
            {
                cleaned_prefix.pop_back();
            }
            std::string subchip_name = cleaned_prefix.empty() ? "subchip" : cleaned_prefix + "_subchip";
            fs::path json_path = subchip_dir / (subchip_name + ".json");
            fs::path qasm_path = subchip_dir / (subchip_name + ".qasm");

            auto pruned = prune_subchip_artifact(artifact.subchip,
                                                 artifact.local_to_global,
                                                 artifact.local_gates,
                                                 artifact.logical_mapping,
                                                 artifact.measurement_mapping,
                                                 subchip_name,
                                                 config.mode);
            write_json_file(pruned.device_json, json_path);

            auto subchip_circuit = std::make_shared<Circuit>(static_cast<IdxType>(pruned.new_to_old.size()));
            subchip_circuit->set_gates(pruned.gates);
            subchip_circuit->set_mapping(!pruned.measurement_mapping.empty()
                                             ? pruned.measurement_mapping
                                             : pruned.logical_mapping);
            subchip_circuit->set_creg(artifact.cregs);
            dumpQASM(subchip_circuit,
                     artifact.source_filename.c_str(),
                     qasm_path.string(),
                     config.debug_level > 1 ? config.debug_level : 0,
                     config.mode);
        }
    }
} // namespace

ExecutionPipeline select_execution_pipeline(const CliConfig &config)
{
    if (config.input_files.size() > 1)
    {
        return ExecutionPipeline::BatchEnhanced;
    }
    return needs_enhanced_single_pipeline(config)
               ? ExecutionPipeline::SingleEnhanced
               : ExecutionPipeline::SingleBaseline;
}

int run_single_circuit_pipeline(const CliConfig &config,
                                ExecutionPipeline pipeline,
                                double backend_metadata_ms)
{
    auto format_ms = [](double ms)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << ms;
        return oss.str();
    };
    cpu_timer total_cli_timer;
    total_cli_timer.start_timer();

    const std::string &input_file = config.input_files.front();
    cpu_timer parse_timer;
    parse_timer.start_timer();
    qasm_parser parser(input_file.c_str());
    IdxType logical_qubits = parser.num_qubits();
    auto circuit = std::make_shared<Circuit>(logical_qubits);
    parser.loadin_circuit(circuit);
    auto cregs = parser.get_list_cregs();
    if (config.force_identity_layout)
    {
        std::vector<IdxType> identity_layout(static_cast<std::size_t>(logical_qubits));
        std::iota(identity_layout.begin(), identity_layout.end(), 0);
        circuit->set_mapping(identity_layout);
    }
    parse_timer.stop_timer();
    const double parse_ms = parse_timer.measure();

    if (circuit->is_empty())
    {
        std::cerr << "Error: Circuit from " << input_file << " is empty" << std::endl;
        return 1;
    }

    ChipMetadataMode metadata_mode =
        pipeline == ExecutionPipeline::SingleBaseline
            ? ChipMetadataMode::TopologyOnly
            : ChipMetadataMode::Full;
    cpu_timer chip_timer;
    chip_timer.start_timer();
    auto chip = constructChip(logical_qubits,
                              config.backendpath,
                              config.run_with_limit,
                              config.debug_level,
                              metadata_mode);
    chip_timer.stop_timer();
    const double chip_ms = chip_timer.measure();
    if (!chip)
    {
        std::cerr << "Error: failed to construct chip from backend." << std::endl;
        return 1;
    }

    cpu_timer transpile_timer;
    transpile_timer.start_timer();
    transpiler(circuit,
               chip,
               cregs,
               config.debug_level,
               config.mode,
               config.use_full_fidelity,
               to_transpiler_mode(config.cp_mode),
               config.disable_mapomatic,
               config.mapomatic_limit,
               config.enable_1q_opt,
               to_routing_mode(config.routing_mode),
               pipeline == ExecutionPipeline::SingleBaseline
                   ? TranspilerProfile::Baseline
                   : TranspilerProfile::Enhanced,
               g_device_basis_gates);
    transpile_timer.stop_timer();
    const double transpile_ms = transpile_timer.measure();

    EmitTiming emit_timing;
    emit_outputs(config,
                 circuit,
                 config.input_files,
                 logical_qubits,
                 chip,
                 &emit_timing);
    total_cli_timer.stop_timer();
    const double total_cli_ms = total_cli_timer.measure();
    if (config.debug_level > 0)
    {
        std::cout << "CLI timing breakdown (ms): backend_metadata=" << format_ms(backend_metadata_ms)
                  << " parse=" << format_ms(parse_ms)
                  << " chip=" << format_ms(chip_ms)
                  << " transpiler_call=" << format_ms(transpile_ms)
                  << " emit=" << format_ms(emit_timing.total_ms)
                  << " total_cli=" << format_ms(total_cli_ms) << std::endl;
        std::cout << "CLI non-transpile overhead (inside process): "
                  << format_ms(total_cli_ms - transpile_ms) << "ms" << std::endl;
    }
    return 0;
}

int run_batch_pipeline(const CliConfig &config)
{
    auto batch = load_circuits(config.input_files);

    IdxType total_requested_qubits = 0;
    for (auto size : batch.circuit_sizes)
    {
        total_requested_qubits += size;
    }
    if (total_requested_qubits == 0)
    {
        std::cerr << "Error: no qubits found in provided circuit(s)." << std::endl;
        return 1;
    }

    auto chip = constructChip(total_requested_qubits,
                              config.backendpath,
                              config.run_with_limit,
                              config.debug_level,
                              ChipMetadataMode::Full);
    if (!chip)
    {
        std::cerr << "Error: failed to construct chip from backend." << std::endl;
        return 1;
    }

    if (total_requested_qubits > chip->chip_qubit_num)
    {
        std::cerr << "Error: total logical qubits (" << total_requested_qubits
                  << ") exceed device capacity (" << chip->chip_qubit_num << ")." << std::endl;
        return 1;
    }

    auto partition_sizes = compute_partition_sizes(batch.circuit_sizes,
                                                   chip->chip_qubit_num,
                                                   total_requested_qubits);
    auto partitions = allocate_partitions(chip, partition_sizes, config.debug_level);
    auto artifacts = transpile_and_merge(batch.circuits,
                                         batch.circuit_cregs,
                                         partitions,
                                         config.input_files,
                                         chip,
                                         config,
                                         total_requested_qubits);

    auto combined_circuit = std::make_shared<Circuit>(chip->chip_qubit_num);
    combined_circuit->set_gates(artifacts.combined_gates);
    combined_circuit->set_creg(artifacts.combined_cregs);
    combined_circuit->set_mapping(artifacts.combined_mapping);
    combined_circuit->set_routing_swap_count(artifacts.routing_swap_count);

    auto outputs = emit_outputs(config,
                                combined_circuit,
                                config.input_files,
                                total_requested_qubits,
                                chip);

    if (artifacts.subchip_artifacts.size() > 1)
    {
        export_subchips(artifacts.subchip_artifacts, outputs, config);
    }
    return 0;
}
} // namespace QASMTrans::cli
