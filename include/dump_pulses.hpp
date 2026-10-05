#pragma once

#include <algorithm>
#include <fstream>
#include <map>
#include <stdexcept>
#include <set>
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <filesystem>
#include <cmath>
#include <limits>
#include <cstddef>

#include "QASMTransPrimitives.hpp"
#include "IR/gate.hpp"
#include "IR/circuit.hpp"
#include "nlomann/json.hpp"

namespace QASMTrans
{
    extern std::unordered_set<std::string> g_device_basis_gates;
    extern std::unordered_map<std::string, std::string> g_merged_gate_aliases;
}

namespace QASMTrans
{
    namespace pulses
    {
        using json = nlohmann::json;

        struct PulseDefinition
        {
            std::string id;
            std::string gate;
            std::vector<IdxType> qubits;
            std::string shape;
            std::string waveform_type;
            ValType width = 0.0;
            ValType amplitude = 0.0;
            std::string note;
            std::vector<ValType> samples_i;
            std::vector<ValType> samples_q;
            std::vector<ValType> samples_i1;
            std::vector<ValType> samples_q1;
            std::vector<ValType> samples_z;
            std::vector<ValType> samples_g01;
            bool is_virtual = false;
            std::map<std::string, ValType> parameters;
            json parameters_json = json::object();
        };

        struct PulseTemplateLibrary
        {
            std::string name;
            std::string version;
            std::map<std::string, std::vector<PulseDefinition>> definitions;
        };

        struct BackendTimingData
        {
            std::string name;
            std::string version;
            std::unordered_map<std::string, ValType> gate_lengths;
        };

        struct GateTimingInfo
        {
            std::string gate;
            std::vector<IdxType> qubits;
            ValType start = 0.0;
            ValType duration = 0.0;
            ValType finish = 0.0;
            std::ptrdiff_t predecessor = -1;
            IdxType logical_gate_id = -1;
            std::string logical_label;
            ValType theta = 0.0;
            ValType phi = 0.0;
            ValType lam = 0.0;
            ValType gamma = 0.0;
            bool is_basis_gate = false;
        };

        struct GateContribution
        {
            ValType total_duration = 0.0;
            size_t count = 0;
        };

        struct CriticalPathResult
        {
            ValType total_duration = 0.0;
            std::vector<size_t> gate_indices;
            std::unordered_map<std::string, GateContribution> contributions;
        };

        inline std::string toLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        inline bool containsRigettiTag(const std::string &value)
        {
            std::string lower = toLower(value);
            return lower.find("rigetti") != std::string::npos || lower.find("ankaa") != std::string::npos;
        }

        inline std::string canonicalGateForRigetti(const std::string &gate, bool rigetti_mode)
        {
            if (!rigetti_mode)
            {
                return gate;
            }
            if (gate == "x" || gate == "sx")
            {
                return "rx";
            }
            return gate;
        }

        inline std::string joinQubits(const std::vector<IdxType> &qubits, const std::string &delimiter)
        {
            std::ostringstream oss;
            for (size_t i = 0; i < qubits.size(); ++i)
            {
                if (i > 0)
                {
                    oss << delimiter;
                }
                oss << qubits[i];
            }
            return oss.str();
        }

        inline std::string makePulseKey(const std::string &gate, const std::vector<IdxType> &qubits)
        {
            std::ostringstream oss;
            oss << gate << ":";
            oss << joinQubits(qubits, "_");
            return oss.str();
        }

        inline std::string makePulseIdentifier(const std::string &gate, const std::vector<IdxType> &qubits)
        {
            std::ostringstream oss;
            oss << gate;
            for (auto q : qubits)
            {
                oss << "_q" << q;
            }
            return oss.str();
        }

        inline std::vector<IdxType> extractGateQubits(const Gate &gate)
        {
            std::vector<IdxType> qubits;
            if (gate.ctrl >= 0)
            {
                qubits.push_back(gate.ctrl);
            }
            if (gate.qubit >= 0)
            {
                qubits.push_back(gate.qubit);
            }
            if (gate.extra >= 0)
            {
                qubits.push_back(gate.extra);
            }
            return qubits;
        }


        inline ValType rigettiThetaForGate(const Gate &gate, bool rigetti_mode)
        {
            if (!rigetti_mode)
            {
                return gate.theta;
            }
            switch (gate.op_name)
            {
            case OP::X:
                return PI;
            case OP::SX:
                return PI / 2.0;
            default:
                return gate.theta;
            }
        }

        inline ValType gateParameterValue(const Gate &gate, const std::string &name, bool rigetti_mode = false)
        {
            if (name == "theta")
            {
                return rigettiThetaForGate(gate, rigetti_mode);
            }
            if (name == "phi")
            {
                return gate.phi;
            }
            if (name == "lambda" || name == "lam")
            {
                return gate.lam;
            }
            if (name == "gamma")
            {
                return gate.gamma;
            }
            return std::numeric_limits<ValType>::quiet_NaN();
        }

        inline bool isNativeRigettiRxAngle(ValType theta)
        {
            if (std::abs(theta) < 1e-9)
            {
                return true;
            }
            ValType half_pi = PI / 2;
            ValType ratio = theta / half_pi;
            return std::abs(ratio - std::round(ratio)) < 1e-6;
        }

        inline bool logicalLabelHasParameters(const std::string &label)
        {
            auto open = label.find('[');
            if (open == std::string::npos)
            {
                return false;
            }
            auto close = label.find(']', open);
            if (close == std::string::npos)
            {
                return false;
            }
            auto eq = label.find('=', open);
            return eq != std::string::npos && eq < close;
        }

        inline const PulseDefinition *selectPulseDefinition(const std::vector<PulseDefinition> &candidates,
                                                           const Gate &gate,
                                                           const std::string &gate_name,
                                                           bool rigetti_mode = false)
        {
            if (candidates.empty())
            {
                return nullptr;
            }
            if (candidates.size() == 1)
            {
                return &candidates.front();
            }
            constexpr ValType PARAM_TOL = 1e-6;
            const PulseDefinition *fallback = nullptr;
            for (const auto &candidate : candidates)
            {
                if (candidate.parameters.empty())
                {
                    if (fallback == nullptr)
                    {
                        fallback = &candidate;
                    }
                    continue;
                }
                bool matched = true;
                for (const auto &param : candidate.parameters)
                {
                    ValType gate_value = gateParameterValue(gate, param.first, rigetti_mode);
                    if (std::isnan(gate_value) || std::abs(gate_value - param.second) > PARAM_TOL)
                    {
                        matched = false;
                        break;
                    }
                }
                if (matched)
                {
                    return &candidate;
                }
            }
            if (fallback != nullptr)
            {
                return fallback;
            }
            return nullptr;
        }

        inline PulseTemplateLibrary loadPulseTemplate(const std::string &template_path)
        {
            PulseTemplateLibrary library;
            std::ifstream input(template_path);
            if (!input.is_open())
            {
                throw std::logic_error("Unable to open pulse template at " + template_path);
            }
            json document = json::parse(input, nullptr, true, true);
            library.name = document.value("name", std::string{});
            library.version = document.value("version", std::string{});
            const bool rigetti_template = containsRigettiTag(library.name) || containsRigettiTag(template_path);
            if (!document.contains("pulse_definitions") || !document["pulse_definitions"].is_array())
            {
                throw std::logic_error("Pulse template " + template_path + " is missing 'pulse_definitions' array");
            }
            for (const auto &entry : document["pulse_definitions"])
            {
                PulseDefinition definition;
                std::string gate_raw = toLower(entry.value("gate", std::string{}));
                definition.gate = canonicalGateForRigetti(gate_raw, rigetti_template);
                definition.qubits = entry.value("qubits", std::vector<IdxType>{});
                definition.shape = entry.value("shape", std::string{});
                definition.waveform_type = entry.value("waveform_type", std::string{});
                definition.width = entry.value("width", ValType{0.0});
                definition.amplitude = entry.value("amplitude", ValType{0.0});
                definition.note = entry.value("note", std::string{});
                definition.id = entry.value("id", std::string{});
                definition.is_virtual = entry.value("virtual", false);
                if (entry.contains("parameters") && entry["parameters"].is_object())
                {
                    for (auto it = entry["parameters"].begin(); it != entry["parameters"].end(); ++it)
                    {
                        if (it.value().is_null())
                        {
                            continue;
                        }
                        if (it.value().is_number())
                        {
                            definition.parameters.emplace(it.key(), it.value().get<ValType>());
                        }
                        else
                        {
                            definition.parameters_json[it.key()] = it.value();
                        }
                    }
                }
                if (entry.contains("samples_i") && entry["samples_i"].is_array())
                {
                    definition.samples_i = entry["samples_i"].get<std::vector<ValType>>();
                }
                if (entry.contains("samples_q") && entry["samples_q"].is_array())
                {
                    definition.samples_q = entry["samples_q"].get<std::vector<ValType>>();
                }
                if (entry.contains("samples_i1") && entry["samples_i1"].is_array())
                {
                    definition.samples_i1 = entry["samples_i1"].get<std::vector<ValType>>();
                }
                if (entry.contains("samples_q1") && entry["samples_q1"].is_array())
                {
                    definition.samples_q1 = entry["samples_q1"].get<std::vector<ValType>>();
                }
                if (entry.contains("samples_z") && entry["samples_z"].is_array())
                {
                    definition.samples_z = entry["samples_z"].get<std::vector<ValType>>();
                }
                if (entry.contains("samples_g01") && entry["samples_g01"].is_array())
                {
                    definition.samples_g01 = entry["samples_g01"].get<std::vector<ValType>>();
                }
                if (definition.gate.empty() || definition.qubits.empty())
                {
                    continue;
                }
                if (definition.id.empty())
                {
                    definition.id = makePulseIdentifier(definition.gate, definition.qubits);
                }
                if (definition.waveform_type.empty())
                {
                    definition.waveform_type = definition.shape;
                }
                const std::string canonical_key = makePulseKey(definition.gate, definition.qubits);
                library.definitions[canonical_key].push_back(definition);
                if (rigetti_template && (gate_raw == "x" || gate_raw == "sx" || gate_raw == "sxdg"))
                {
                    const std::string alias_key = makePulseKey(gate_raw, definition.qubits);
                    library.definitions[alias_key].push_back(definition);
                }
                if (definition.qubits.size() > 1)
                {
                    std::vector<IdxType> sorted_qubits = definition.qubits;
                    std::sort(sorted_qubits.begin(), sorted_qubits.end());
                    if (sorted_qubits != definition.qubits)
                    {
                        const std::string sorted_key = makePulseKey(definition.gate, sorted_qubits);
                        library.definitions[sorted_key].push_back(definition);
                        if (rigetti_template && (gate_raw == "x" || gate_raw == "sx" || gate_raw == "sxdg"))
                        {
                            const std::string sorted_alias = makePulseKey(gate_raw, sorted_qubits);
                            library.definitions[sorted_alias].push_back(definition);
                        }
                    }
                }
            }
            return library;
        }

        inline BackendTimingData loadBackendTiming(const std::string &backend_path)
        {
            BackendTimingData data;
            std::ifstream input(backend_path);
            if (!input.is_open())
            {
                throw std::logic_error("Unable to open backend config at " + backend_path);
            }
            json document = json::parse(input, nullptr, true, true);
            data.name = document.value("name", std::string{});
            data.version = document.value("version", std::string{});
            if (document.contains("gate_lens") && document["gate_lens"].is_object())
            {
                for (auto it = document["gate_lens"].begin(); it != document["gate_lens"].end(); ++it)
                {
                    data.gate_lengths.emplace(it.key(), it.value().get<ValType>());
                }
            }
            return data;
        }

        inline std::vector<Gate> expandGatesForPulseDump(std::shared_ptr<Circuit> circuit,
                                                         const std::string &backend_path,
                                                         const std::string &pulse_template_path,
                                                         bool allow_parameterized_candidates,
                                                         bool *rigetti_mode_out = nullptr)
        {
            PulseTemplateLibrary library = loadPulseTemplate(pulse_template_path);
            BackendTimingData backend = loadBackendTiming(backend_path);
            const bool rigetti_mode = containsRigettiTag(library.name) ||
                                      containsRigettiTag(backend.name) ||
                                      containsRigettiTag(pulse_template_path);
            if (rigetti_mode_out)
            {
                *rigetti_mode_out = rigetti_mode;
            }

            const std::map<std::string, std::vector<PulseDefinition>> &definitions = library.definitions;
            const std::vector<Gate> original_gates = circuit->get_gates();
            std::vector<Gate> gates;
            gates.reserve(original_gates.size() * 5);

            auto hasMatchingPulse = [&](const Gate &candidate_gate, const std::string &gate_name, const std::vector<IdxType> &qubits) -> bool
            {
                const std::string key = makePulseKey(gate_name, qubits);
                auto def_it = definitions.find(key);
                if (def_it == definitions.end())
                {
                    if (qubits.size() > 1)
                    {
                        std::vector<IdxType> sorted = qubits;
                        std::sort(sorted.begin(), sorted.end());
                        if (sorted != qubits)
                        {
                            const std::string sorted_key = makePulseKey(gate_name, sorted);
                            def_it = definitions.find(sorted_key);
                        }
                    }
                    if (def_it == definitions.end())
                    {
                        return false;
                    }
                }
                return selectPulseDefinition(def_it->second, candidate_gate, gate_name, rigetti_mode) != nullptr;
            };

            for (const auto &gate : original_gates)
            {
                std::string gate_name_raw = gate.lower_name();
                std::string gate_name_canonical = canonicalGateForRigetti(gate_name_raw, rigetti_mode);
                Gate gate_adjusted = gate;
                gate_adjusted.theta = rigettiThetaForGate(gate, rigetti_mode);
                ValType effective_theta = gate_adjusted.theta;
                if (gate_name_canonical == "rx")
                {
                    std::vector<IdxType> qubits = extractGateQubits(gate);
                    if (!hasMatchingPulse(gate_adjusted, gate_name_canonical, qubits))
                    {
                        IdxType target = qubits.empty() ? gate.qubit : qubits.front();
                        Gate rz1(OP::RZ, target, -1, -1, 1, -PI / 2.0);
                        rz1.inherit_logical_metadata(gate);
                        gates.push_back(rz1);
                        Gate rx1(OP::RX, target, -1, -1, 1, PI / 2.0);
                        rx1.inherit_logical_metadata(gate);
                        gates.push_back(rx1);
                        Gate rz2(OP::RZ, target, -1, -1, 1, effective_theta);
                        rz2.inherit_logical_metadata(gate);
                        gates.push_back(rz2);
                        Gate rx2(OP::RX, target, -1, -1, 1, -PI / 2.0);
                        rx2.inherit_logical_metadata(gate);
                        gates.push_back(rx2);
                        Gate rz3(OP::RZ, target, -1, -1, 1, PI / 2.0);
                        rz3.inherit_logical_metadata(gate);
                        gates.push_back(rz3);
                        continue;
                    }
                }
                gates.push_back(gate_adjusted);
            }

            return gates;
        }

    } // namespace pulses

    inline pulses::CriticalPathResult computeCriticalPath(const std::vector<pulses::GateTimingInfo> &timings,
                                                          bool allow_parameterized_candidates = true)
    {
        using namespace pulses;
        CriticalPathResult result;
        if (timings.empty())
        {
            return result;
        }

        ValType max_finish = 0.0;
        std::ptrdiff_t max_index = -1;
        for (size_t idx = 0; idx < timings.size(); ++idx)
        {
            if (timings[idx].finish >= max_finish)
            {
                max_finish = timings[idx].finish;
                max_index = static_cast<std::ptrdiff_t>(idx);
            }
        }

        result.total_duration = max_finish;
        std::vector<size_t> path_indices;
        while (max_index >= 0)
        {
            size_t current = static_cast<size_t>(max_index);
            path_indices.push_back(current);
            max_index = timings[current].predecessor;
        }
        std::reverse(path_indices.begin(), path_indices.end());
        result.gate_indices = path_indices;

        for (size_t idx : path_indices)
        {
            const GateTimingInfo &info = timings[idx];
            if (info.is_basis_gate)
            {
                continue;
            }
            if (!allow_parameterized_candidates && logicalLabelHasParameters(info.logical_label))
            {
                continue;
            }
            const std::string &key = !info.logical_label.empty() ? info.logical_label : info.gate;
            GateContribution &entry = result.contributions[key];
            entry.total_duration += info.duration;
            entry.count += 1;
        }

        return result;
    }

    inline void dumpPulses(std::shared_ptr<Circuit> circuit,
                           const char *input_filename,
                           const std::string &backend_path,
                           const std::string &pulse_template_path,
                           const std::string &output_path,
                           IdxType debug_level,
                           bool allow_parameterized_candidates = true)
    {
        using namespace pulses;
        PulseTemplateLibrary library = loadPulseTemplate(pulse_template_path);
        BackendTimingData backend = loadBackendTiming(backend_path);
        const bool rigetti_mode = containsRigettiTag(library.name) ||
                                  containsRigettiTag(backend.name) ||
                                  containsRigettiTag(pulse_template_path);

        const std::map<std::string, std::vector<PulseDefinition>> &definitions = library.definitions;
        std::set<std::string> used_ids;
        std::vector<json> schedule;
        struct QubitAvailability
        {
            ValType finish = 0.0;
            std::ptrdiff_t gate_index = -1;
        };
        std::unordered_map<IdxType, QubitAvailability> availability;
        const std::vector<Gate> original_gates = circuit->get_gates();
        std::vector<Gate> gates;
        gates.reserve(original_gates.size() * 5);

        auto hasMatchingPulse = [&](const Gate &candidate_gate, const std::string &gate_name, const std::vector<IdxType> &qubits) -> bool
        {
            const std::string key = makePulseKey(gate_name, qubits);
            auto def_it = definitions.find(key);
            if (def_it == definitions.end())
            {
                if (qubits.size() > 1)
                {
                    std::vector<IdxType> sorted = qubits;
                    std::sort(sorted.begin(), sorted.end());
                    if (sorted != qubits)
                    {
                        const std::string sorted_key = makePulseKey(gate_name, sorted);
                        def_it = definitions.find(sorted_key);
                    }
                }
                if (def_it == definitions.end())
                {
                    return false;
                }
            }
            return selectPulseDefinition(def_it->second, candidate_gate, gate_name, rigetti_mode) != nullptr;
        };

        for (const auto &gate : original_gates)
        {
            std::string gate_name_raw = gate.lower_name();
            std::string gate_name_canonical = canonicalGateForRigetti(gate_name_raw, rigetti_mode);
            Gate gate_adjusted = gate;
            gate_adjusted.theta = rigettiThetaForGate(gate, rigetti_mode);
            ValType effective_theta = gate_adjusted.theta;
            if (gate_name_canonical == "rx")
            {
                std::vector<IdxType> qubits = extractGateQubits(gate);
                if (!hasMatchingPulse(gate_adjusted, gate_name_canonical, qubits))
                {
                    // Expand Rx(theta) into virtual RZ and calibrated Rx(pi/2) pulses.
                    IdxType target = qubits.empty() ? gate.qubit : qubits.front();
                    Gate rz1(OP::RZ, target, -1, -1, 1, -PI / 2.0);
                    rz1.inherit_logical_metadata(gate);
                    gates.push_back(rz1);
                    Gate rx1(OP::RX, target, -1, -1, 1, PI / 2.0);
                    rx1.inherit_logical_metadata(gate);
                    gates.push_back(rx1);
                    Gate rz2(OP::RZ, target, -1, -1, 1, effective_theta);
                    rz2.inherit_logical_metadata(gate);
                    gates.push_back(rz2);
                    Gate rx2(OP::RX, target, -1, -1, 1, -PI / 2.0);
                    rx2.inherit_logical_metadata(gate);
                    gates.push_back(rx2);
                    Gate rz3(OP::RZ, target, -1, -1, 1, PI / 2.0);
                    rz3.inherit_logical_metadata(gate);
                    gates.push_back(rz3);
                    continue;
                }
            }
            gates.push_back(gate_adjusted);
        }

        std::vector<GateTimingInfo> gate_timings;
        gate_timings.reserve(gates.size());
        size_t gate_index = 0;
        for (const auto &gate : gates)
        {
            std::string gate_name_raw = gate.lower_name();
            std::string gate_name = canonicalGateForRigetti(gate_name_raw, rigetti_mode);
            if (gate_name.empty())
            {
                continue;
            }
            if (gate_name == "ma" || gate_name == "m")
            {
                continue;
            }
            std::vector<IdxType> qubits = extractGateQubits(gate);
            if (qubits.empty())
            {
                continue;
            }
            const std::string key = makePulseKey(gate_name, qubits);
            auto def_it = definitions.find(key);
            if (def_it == definitions.end())
            {
                if (qubits.size() > 1)
                {
                    std::vector<IdxType> sorted = qubits;
                    std::sort(sorted.begin(), sorted.end());
                    if (sorted != qubits)
                    {
                        const std::string sorted_key = makePulseKey(gate_name, sorted);
                        def_it = definitions.find(sorted_key);
                    }
                }
                if (def_it == definitions.end())
                {
                    throw std::logic_error(
                        "Pulse template missing definition for gate '" + gate_name + "' on qubits [" +
                        joinQubits(qubits, ",") + "]");
                }
            }
            const PulseDefinition *definition_ptr = selectPulseDefinition(def_it->second, gate, gate_name, rigetti_mode);
            if (definition_ptr == nullptr)
            {
                std::ostringstream oss;
                oss << "Pulse template missing matching definition for gate '" << gate_name
                    << "' on qubits [" << joinQubits(qubits, ",") << "]";
                if (gate.theta != 0.0)
                {
                    oss << " (theta=" << gate.theta << ")";
                }
                if (gate.phi != 0.0)
                {
                    oss << " (phi=" << gate.phi << ")";
                }
                if (gate.lam != 0.0)
                {
                    oss << " (lambda=" << gate.lam << ")";
                }
                if (gate.gamma != 0.0)
                {
                    oss << " (gamma=" << gate.gamma << ")";
                }
                throw std::logic_error(oss.str());
            }
            const PulseDefinition &definition = *definition_ptr;
            if (!definition.is_virtual && toLower(definition.waveform_type) == "arbitrary")
            {
                if (definition.samples_i.empty() && definition.samples_q.empty())
                {
                    throw std::logic_error("Arbitrary waveform for gate '" + gate_name + "' on qubits [" +
                                           joinQubits(qubits, ",") + "] is missing samples");
                }
            }
            ValType start_time = 0.0;
            std::ptrdiff_t predecessor = -1;
            for (auto q : qubits)
            {
                auto avail_it = availability.find(q);
                if (avail_it != availability.end())
                {
                    if (avail_it->second.finish >= start_time)
                    {
                        start_time = avail_it->second.finish;
                        predecessor = avail_it->second.gate_index;
                    }
                }
            }
            ValType duration = std::max(definition.width, ValType{0.0});
            ValType finish = start_time + duration;

            size_t current_gate_index = gate_index;

            json entry;
            entry["index"] = gate_index++;
            entry["gate"] = gate_name;
            entry["qubits"] = qubits;
            entry["pulse_id"] = definition.id;
            entry["start_time"] = start_time;
            entry["duration"] = duration;
            if (definition.is_virtual)
            {
                entry["virtual"] = true;
            }
            if (gate.has_logical_metadata())
            {
                entry["logical_gate_id"] = gate.logical_gate_id;
                entry["logical_label"] = gate.logical_label;
            }
            json parameters = json::object();
            ValType theta_effective = rigettiThetaForGate(gate, rigetti_mode);
            if (theta_effective != 0.0)
            {
                parameters["theta"] = theta_effective;
            }
            if (gate.phi != 0.0)
            {
                parameters["phi"] = gate.phi;
            }
            if (gate.lam != 0.0)
            {
                parameters["lambda"] = gate.lam;
            }
            if (gate.gamma != 0.0)
            {
                parameters["gamma"] = gate.gamma;
            }
            if (!parameters.empty())
            {
                entry["parameters"] = parameters;
            }
            const bool is_basis_gate = g_device_basis_gates.find(gate.lower_name()) != g_device_basis_gates.end();
            schedule.push_back(entry);
            for (auto q : qubits)
            {
                availability[q] = QubitAvailability{finish, static_cast<std::ptrdiff_t>(current_gate_index)};
            }
            used_ids.insert(definition.id);
            GateTimingInfo timing_info;
            timing_info.gate = gate_name;
            timing_info.qubits = qubits;
            timing_info.start = start_time;
            timing_info.duration = duration;
            timing_info.finish = finish;
            timing_info.predecessor = predecessor;
            timing_info.logical_gate_id = gate.logical_gate_id;
            timing_info.logical_label = gate.logical_label;
            timing_info.theta = theta_effective;
            timing_info.phi = gate.phi;
            timing_info.lam = gate.lam;
            timing_info.gamma = gate.gamma;
            timing_info.is_basis_gate = is_basis_gate;
            gate_timings.push_back(timing_info);
        }

        json pulse_library = json::array();
        for (const auto &pair : definitions)
        {
            for (const auto &definition : pair.second)
            {
                if (used_ids.find(definition.id) == used_ids.end())
                {
                    continue;
                }
                json entry;
                entry["id"] = definition.id;
                entry["gate"] = canonicalGateForRigetti(definition.gate, rigetti_mode);
                entry["qubits"] = definition.qubits;
                if (!definition.shape.empty())
                {
                    entry["shape"] = definition.shape;
                }
                if (!definition.waveform_type.empty())
                {
                    entry["waveform_type"] = definition.waveform_type;
                }
                entry["width"] = definition.width;
                entry["amplitude"] = definition.amplitude;
                if (!definition.note.empty())
                {
                    entry["note"] = definition.note;
                }
                if (definition.is_virtual)
                {
                    entry["virtual"] = true;
                }
                if (!definition.parameters.empty() || !definition.parameters_json.empty())
                {
                    json param_obj = json::object();
                    for (const auto &param : definition.parameters)
                    {
                        param_obj[param.first] = param.second;
                    }
                    for (auto it = definition.parameters_json.begin(); it != definition.parameters_json.end(); ++it)
                    {
                        param_obj[it.key()] = it.value();
                    }
                    entry["parameters"] = param_obj;
                }
                if (!definition.samples_i.empty())
                {
                    entry["samples_i"] = definition.samples_i;
                }
                if (!definition.samples_q.empty())
                {
                    entry["samples_q"] = definition.samples_q;
                }
                if (!definition.samples_i1.empty())
                {
                    entry["samples_i1"] = definition.samples_i1;
                }
                if (!definition.samples_q1.empty())
                {
                    entry["samples_q1"] = definition.samples_q1;
                }
                if (!definition.samples_z.empty())
                {
                    entry["samples_z"] = definition.samples_z;
                }
                if (!definition.samples_g01.empty())
                {
                    entry["samples_g01"] = definition.samples_g01;
                }
                pulse_library.push_back(entry);
            }
        }

        json backend_info = json::object();
        backend_info["name"] = !backend.name.empty() ? backend.name : library.name;
        backend_info["version"] = !backend.version.empty() ? backend.version : library.version;
        backend_info["input_qasm"] = std::filesystem::path(input_filename).filename().string();
        backend_info["backend_config"] = backend_path;
        backend_info["pulse_template"] = pulse_template_path;
        backend_info["num_qubits"] = circuit->num_qubits();
        backend_info["total_pulses"] = schedule.size();
        ValType total_duration = 0.0;
        for (const auto &pair : availability)
        {
            total_duration = std::max(total_duration, pair.second.finish);
        }
        backend_info["total_duration"] = total_duration;
        json output;
        output["backend"] = backend_info;
        output["pulse_library"] = pulse_library;
        output["schedule"] = schedule;

        CriticalPathResult critical_path_result = computeCriticalPath(gate_timings, allow_parameterized_candidates);
        json critical_summary = json::object();
        critical_summary["total_duration"] = critical_path_result.total_duration;
        critical_summary["gate_count"] = critical_path_result.gate_indices.size();
        json path_indices = json::array();
        for (size_t idx : critical_path_result.gate_indices)
        {
            path_indices.push_back(idx);
        }
        critical_summary["path_indices"] = path_indices;
        critical_summary["allow_parameterized_candidates"] = allow_parameterized_candidates;

        std::vector<std::pair<std::string, GateContribution>> contributions(
            critical_path_result.contributions.begin(), critical_path_result.contributions.end());
        contributions.erase(
            std::remove_if(contributions.begin(), contributions.end(), [](const auto &entry)
                           {
                               const std::string &gate = entry.first;
                               return g_device_basis_gates.find(gate) != g_device_basis_gates.end();
                           }),
            contributions.end());
        std::sort(contributions.begin(), contributions.end(), [](const auto &lhs, const auto &rhs)
                  {
                      if (std::abs(lhs.second.total_duration - rhs.second.total_duration) > 1e-9)
                      {
                          return lhs.second.total_duration > rhs.second.total_duration;
                      }
                      if (lhs.second.count != rhs.second.count)
                      {
                          return lhs.second.count > rhs.second.count;
                      }
                      return lhs.first < rhs.first;
                  });
        json top_gates = json::array();
        size_t gate_limit = std::min<size_t>(10, contributions.size());
        for (size_t i = 0; i < gate_limit; ++i)
        {
            json entry = json::object();
            entry["gate"] = contributions[i].first;
            entry["count"] = contributions[i].second.count;
            entry["total_duration"] = contributions[i].second.total_duration;
            top_gates.push_back(entry);
        }
        critical_summary["top_gates"] = top_gates;

        std::unordered_set<size_t> critical_path_indices(
            critical_path_result.gate_indices.begin(), critical_path_result.gate_indices.end());

        std::unordered_map<IdxType, ValType> logical_instance_duration;
        std::unordered_map<IdxType, std::string> logical_instance_label;
        std::unordered_map<IdxType, std::vector<IdxType>> logical_instance_qubits;
        for (size_t idx = 0; idx < gate_timings.size(); ++idx)
        {
            if (critical_path_indices.find(idx) == critical_path_indices.end())
            {
                continue;
            }
            const auto &timing = gate_timings[idx];
            bool skip_logical = false;
            if (!timing.logical_label.empty())
            {
                std::string base_label = timing.logical_label;
                auto bracket = base_label.find('[');
                if (bracket != std::string::npos)
                {
                    base_label = base_label.substr(0, bracket);
                }
                std::string base_lower = toLower(base_label);
                if (!base_lower.empty())
                {
                    bool is_merged_gate = base_lower.rfind("merged_", 0) == 0;
                    if (!is_merged_gate && g_device_basis_gates.find(base_lower) != g_device_basis_gates.end())
                    {
                        if (base_lower == "rx")
                        {
                            skip_logical = isNativeRigettiRxAngle(timing.theta);
                        }
                        else
                        {
                            skip_logical = true;
                        }
                    }
                }
            }
            if (skip_logical || timing.logical_gate_id < 0 || timing.logical_label.empty())
            {
                continue;
            }
            if (!allow_parameterized_candidates && logicalLabelHasParameters(timing.logical_label))
            {
                continue;
            }
            logical_instance_duration[timing.logical_gate_id] += timing.duration;
            logical_instance_label.emplace(timing.logical_gate_id, timing.logical_label);
            auto &stored_qubits = logical_instance_qubits[timing.logical_gate_id];
            for (auto q : timing.qubits)
            {
                if (std::find(stored_qubits.begin(), stored_qubits.end(), q) == stored_qubits.end())
                {
                    stored_qubits.push_back(q);
                }
            }
        }

        std::unordered_map<std::string, GateContribution> logical_totals;
        std::unordered_map<std::string, std::map<std::vector<IdxType>, GateContribution>> logical_qubit_totals;
        for (const auto &pair : logical_instance_duration)
        {
            const auto &label = logical_instance_label[pair.first];
            GateContribution &entry = logical_totals[label];
            entry.total_duration += pair.second;
            entry.count += 1;
            const auto &qubits_vec = logical_instance_qubits[pair.first];
            if (!qubits_vec.empty())
            {
                GateContribution &q_entry = logical_qubit_totals[label][qubits_vec];
                q_entry.total_duration += pair.second;
                q_entry.count += 1;
            }
        }

        std::vector<std::pair<std::string, GateContribution>> logical_entries(
            logical_totals.begin(), logical_totals.end());
        std::sort(logical_entries.begin(), logical_entries.end(), [](const auto &lhs, const auto &rhs)
                  {
                      if (std::abs(lhs.second.total_duration - rhs.second.total_duration) > 1e-9)
                      {
                          return lhs.second.total_duration > rhs.second.total_duration;
                      }
                      if (lhs.second.count != rhs.second.count)
                      {
                          return lhs.second.count > rhs.second.count;
                      }
                      return lhs.first < rhs.first;
                  });
        json logical_top = json::array();
        size_t logical_limit = std::min<size_t>(10, logical_entries.size());
        for (size_t i = 0; i < logical_limit; ++i)
        {
            json entry = json::object();
            entry["gate"] = logical_entries[i].first;
            entry["count"] = logical_entries[i].second.count;
            entry["total_duration"] = logical_entries[i].second.total_duration;
            logical_top.push_back(entry);
        }
        critical_summary["top_gates"] = logical_top;
        output["critical_path"] = critical_summary;
        output["allow_parameterized_candidates"] = allow_parameterized_candidates;
        output["logical_latency"] = logical_top;

        std::filesystem::path out_path(output_path);
        if (out_path.has_parent_path())
        {
            std::filesystem::create_directories(out_path.parent_path());
        }
        std::ofstream out_file(out_path);
        if (!out_file.is_open())
        {
            throw std::logic_error("Unable to open output pulse file " + output_path);
        }
        out_file << output.dump(2) << std::endl;
        out_file.close();

        if (debug_level > 0)
        {
            std::cout << "Saved pulse schedule with " << schedule.size() << " entries to " << output_path << std::endl;
        }
    }
}
