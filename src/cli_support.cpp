#include "../include/cli_support.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

#include "../include/IR/gate.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace
{
    std::vector<std::string> basis_gates_for_mode(QASMTrans::IdxType mode)
    {
        using QASMTrans::IdxType;
        switch (mode)
        {
        case 0:
            return {"rz", "sx", "x", "cx"};
        case 1:
            return {"rx", "ry", "rz", "rxx"};
        case 2:
            return {"rx", "rz", "zz"};
        case 3:
            return {"rx", "ry", "rz", "cz"};
        case 4:
            return {"prx", "cz", "rx", "ry", "rz", "h"};
        case 5:
            return {"prx", "cx"};
        default:
            return {};
        }
    }

    json build_subchip_json(const std::shared_ptr<QASMTrans::Chip> &subchip,
                            const std::vector<QASMTrans::IdxType> &new_to_old,
                            const std::vector<QASMTrans::IdxType> &old_to_new,
                            const std::vector<QASMTrans::IdxType> &local_to_global,
                            const std::vector<QASMTrans::IdxType> &logical_mapping,
                            const std::string &name,
                            QASMTrans::IdxType mode)
    {
        using QASMTrans::IdxType;
        json result;
        result["name"] = name;
        result["version"] = "generated";
        result["num_qubits"] = static_cast<IdxType>(new_to_old.size());
        result["basis_gates"] = basis_gates_for_mode(mode);

        std::vector<std::string> coupling;
        if (subchip)
        {
            for (std::size_t new_src = 0; new_src < new_to_old.size(); ++new_src)
            {
                IdxType old_src = new_to_old[new_src];
                if (old_src < 0 || old_src >= static_cast<IdxType>(subchip->edge_list.size()))
                {
                    continue;
                }
                for (IdxType old_dst : subchip->edge_list[old_src])
                {
                    if (old_dst < 0 || old_dst >= static_cast<IdxType>(old_to_new.size()))
                    {
                        continue;
                    }
                    IdxType new_dst = old_to_new[old_dst];
                    if (new_dst >= 0)
                    {
                        coupling.emplace_back(std::to_string(new_src) + "_" + std::to_string(new_dst));
                    }
                }
            }
        }
        std::sort(coupling.begin(), coupling.end());
        coupling.erase(std::unique(coupling.begin(), coupling.end()), coupling.end());
        result["cx_coupling"] = coupling;

        json gate_errs = json::object();
        json gate_lens = json::object();
        if (subchip)
        {
            for (std::size_t new_idx = 0; new_idx < new_to_old.size(); ++new_idx)
            {
                IdxType old_idx = new_to_old[new_idx];
                if (old_idx >= 0 && old_idx < static_cast<IdxType>(subchip->single_qubit_errors.size()))
                {
                    for (const auto &entry : subchip->single_qubit_errors[old_idx])
                    {
                        gate_errs[entry.first + std::to_string(new_idx)] = entry.second;
                    }
                }
                if (old_idx >= 0 && old_idx < static_cast<IdxType>(subchip->single_qubit_gate_lengths.size()))
                {
                    for (const auto &entry : subchip->single_qubit_gate_lengths[old_idx])
                    {
                        gate_lens[entry.first + std::to_string(new_idx)] = entry.second;
                    }
                }
            }
            auto append_two_qubit = [&](const auto &source_map, json &dest) {
                for (const auto &entry : source_map)
                {
                    IdxType new_ctrl = (entry.first.first >= 0 && entry.first.first < static_cast<IdxType>(old_to_new.size()))
                                           ? old_to_new[entry.first.first]
                                           : -1;
                    IdxType new_tgt = (entry.first.second >= 0 && entry.first.second < static_cast<IdxType>(old_to_new.size()))
                                          ? old_to_new[entry.first.second]
                                          : -1;
                    if (new_ctrl < 0 || new_tgt < 0)
                    {
                        continue;
                    }
                    for (const auto &gate_entry : entry.second)
                    {
                        const double value = gate_entry.second;
                        const std::string forward_key = gate_entry.first + std::to_string(new_ctrl) + "_" + std::to_string(new_tgt);
                        dest[forward_key] = value;
                        if (new_ctrl != new_tgt)
                        {
                            const std::string reverse_key = gate_entry.first + std::to_string(new_tgt) + "_" + std::to_string(new_ctrl);
                            if (!dest.contains(reverse_key))
                            {
                                dest[reverse_key] = value;
                            }
                        }
                    }
                }
            };
            append_two_qubit(subchip->two_qubit_errors, gate_errs);
            append_two_qubit(subchip->two_qubit_gate_lengths, gate_lens);
        }
        result["gate_errs"] = gate_errs;
        result["gate_lens"] = gate_lens;

        auto append_optional_property = [&](const std::vector<std::optional<double>> &values,
                                            const char *key) {
            if (values.empty())
            {
                return;
            }
            json prop = json::object();
            bool any = false;
            for (std::size_t new_idx = 0; new_idx < new_to_old.size(); ++new_idx)
            {
                QASMTrans::IdxType old_idx = new_to_old[new_idx];
                if (old_idx < 0 || old_idx >= static_cast<QASMTrans::IdxType>(values.size()))
                {
                    continue;
                }
                const auto &val = values[static_cast<std::size_t>(old_idx)];
                if (val.has_value())
                {
                    prop[std::to_string(new_idx)] = *val;
                    any = true;
                }
            }
            if (any)
            {
                result[key] = std::move(prop);
            }
        };
        if (subchip)
        {
            append_optional_property(subchip->t1, "T1");
            append_optional_property(subchip->t2, "T2");
            append_optional_property(subchip->freq, "freq");
            append_optional_property(subchip->readout_length, "readout_length");
            append_optional_property(subchip->prob_meas0_prep1, "prob_meas0_prep1");
            append_optional_property(subchip->prob_meas1_prep0, "prob_meas1_prep0");
        }

        json local_global = json::array();
        for (QASMTrans::IdxType value : local_to_global)
        {
            local_global.push_back(value);
        }
        result["local_to_global"] = local_global;

        json logical_local = json::array();
        for (QASMTrans::IdxType value : logical_mapping)
        {
            logical_local.push_back(value);
        }
        result["logical_to_local"] = logical_local;
        result["generated_by"] = "qasmtrans";

        return result;
    }

    std::vector<QASMTrans::IdxType> collect_used_nodes(const std::shared_ptr<QASMTrans::Chip> &subchip,
                                                       const std::vector<QASMTrans::Gate> &local_gates,
                                                       const std::vector<QASMTrans::IdxType> &logical_mapping,
                                                       const std::vector<QASMTrans::IdxType> &measurement_mapping)
    {
        std::vector<QASMTrans::IdxType> used;
        if (!subchip)
        {
            return used;
        }

        QASMTrans::IdxType local_size = subchip->chip_qubit_num;
        if (local_size <= 0)
        {
            return used;
        }

        std::vector<char> mark(static_cast<std::size_t>(local_size), 0);
        auto mark_idx = [&](QASMTrans::IdxType idx)
        {
            if (idx >= 0 && idx < local_size)
            {
                mark[static_cast<std::size_t>(idx)] = 1;
            }
        };

        for (const auto &gate : local_gates)
        {
            mark_idx(gate.qubit);
            mark_idx(gate.ctrl);
            mark_idx(gate.extra);
        }
        for (QASMTrans::IdxType value : logical_mapping)
        {
            mark_idx(value);
        }
        for (QASMTrans::IdxType value : measurement_mapping)
        {
            mark_idx(value);
        }

        for (QASMTrans::IdxType idx = 0; idx < local_size; ++idx)
        {
            if (mark[static_cast<std::size_t>(idx)])
            {
                used.push_back(idx);
            }
        }
        return used;
    }
} // namespace

namespace QASMTrans
{
    namespace cli
    {
        std::string derive_pulse_output_path(const std::string &qasm_output_path)
        {
            fs::path qasm_path(qasm_output_path);
            fs::path directory = qasm_path.parent_path();
            std::string stem = qasm_path.stem().string();
            if (stem.empty())
            {
                stem = qasm_path.filename().string();
            }
            fs::path candidate = directory / (stem + "_pulses.json");
            return candidate.string();
        }

        IdxType mode_from_string(const std::string &mode_name)
        {
            std::string lowered = mode_name;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                           [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            if (lowered == "ibmq")
            {
                return 0;
            }
            if (lowered == "ionq")
            {
                return 1;
            }
            if (lowered == "quantinuum")
            {
                return 2;
            }
            if (lowered == "rigetti")
            {
                return 3;
            }
            if (lowered == "quafu")
            {
                return 4;
            }
            if (lowered == "iqm")
            {
                return 5;
            }
            throw std::invalid_argument("Unknown mode '" + lowered + "'");
        }

        void ingest_backend_metadata(const std::string &backendpath,
                                     std::unordered_set<std::string> &device_basis_gates,
                                     std::unordered_map<std::string, std::string> &merged_gate_aliases,
                                     bool strict)
        {
            device_basis_gates.clear();
            merged_gate_aliases.clear();
            try
            {
                const json &backend_config = QASMTrans::load_backend_config_cached(backendpath);
                auto to_lower = [](std::string value)
                {
                    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                                   { return static_cast<char>(std::tolower(c)); });
                    return value;
                };
                auto ingest_aliases = [&](const json &aliases)
                {
                    if (!aliases.is_object())
                    {
                        return;
                    }
                    for (const auto &item : aliases.items())
                    {
                        std::string alias_name = to_lower(item.key());
                        if (alias_name.empty())
                        {
                            continue;
                        }
                        device_basis_gates.insert(alias_name);
                        const json &info = item.value();
                        if (!info.is_object())
                        {
                            continue;
                        }
                        std::string logical = to_lower(info.value("logical_gate", std::string{}));
                        if (logical.empty())
                        {
                            continue;
                        }
                        if (merged_gate_aliases.find(logical) == merged_gate_aliases.end())
                        {
                            merged_gate_aliases[logical] = alias_name;
                        }
                    }
                };
                auto ingest_basis = [&](const json &arr)
                {
                    if (!arr.is_array())
                    {
                        return;
                    }
                    for (const auto &entry : arr)
                    {
                        if (entry.is_string())
                        {
                            std::string gate = to_lower(entry.get<std::string>());
                            if (!gate.empty())
                            {
                                device_basis_gates.insert(gate);
                            }
                        }
                    }
                };
                auto ingest_gate_lens = [&](const json &obj)
                {
                    if (!obj.is_object())
                    {
                        return;
                    }
                    for (auto it = obj.begin(); it != obj.end(); ++it)
                    {
                        const std::string key = it.key();
                        size_t first_digit = key.find_first_of("0123456789");
                        if (first_digit == std::string::npos)
                        {
                            continue;
                        }
                        std::string gate = to_lower(key.substr(0, first_digit));
                        if (!gate.empty())
                        {
                            device_basis_gates.insert(gate);
                        }
                    }
                };
                ingest_basis(backend_config.value("basis_gates", json::array()));
                if (backend_config.contains("metadata") && backend_config["metadata"].is_object())
                {
                    ingest_basis(backend_config["metadata"].value("basis_gates", json::array()));
                    ingest_aliases(backend_config["metadata"].value("merged_gate_aliases", json::object()));
                }
                ingest_aliases(backend_config.value("merged_gate_aliases", json::object()));
                if (device_basis_gates.empty())
                {
                    ingest_gate_lens(backend_config.value("gate_lens", json::object()));
                }
            }
            catch (const std::exception &ex)
            {
                if (strict)
                {
                    throw std::runtime_error(std::string("Failed to parse backend config: ") + ex.what());
                }
                // Permissive callers can fall back to defaults.
            }
        }

        PrunedSubchipData prune_subchip_artifact(const std::shared_ptr<Chip> &subchip,
                                                 const std::vector<IdxType> &local_to_global,
                                                 const std::vector<QASMTrans::Gate> &local_gates,
                                                 const std::vector<IdxType> &logical_mapping,
                                                 const std::vector<IdxType> &measurement_mapping,
                                                 const std::string &subchip_name,
                                                 IdxType mode)
        {
            PrunedSubchipData result;

            if (!subchip)
            {
                result.device_json = build_subchip_json(nullptr, result.new_to_old, result.old_to_new,
                                                        result.local_to_global, result.logical_mapping,
                                                        subchip_name, mode);
                return result;
            }

            auto used_nodes = collect_used_nodes(subchip, local_gates, logical_mapping, measurement_mapping);
            if (used_nodes.empty())
            {
                used_nodes.reserve(static_cast<std::size_t>(subchip->chip_qubit_num));
                for (IdxType idx = 0; idx < subchip->chip_qubit_num; ++idx)
                {
                    used_nodes.push_back(idx);
                }
            }

            std::sort(used_nodes.begin(), used_nodes.end());
            used_nodes.erase(std::unique(used_nodes.begin(), used_nodes.end()), used_nodes.end());

            result.new_to_old = used_nodes;
            result.old_to_new.assign(static_cast<std::size_t>(subchip->chip_qubit_num), -1);
            result.local_to_global.reserve(used_nodes.size());
            for (std::size_t new_idx = 0; new_idx < used_nodes.size(); ++new_idx)
            {
                IdxType old_idx = used_nodes[new_idx];
                if (old_idx >= 0 && old_idx < static_cast<IdxType>(result.old_to_new.size()))
                {
                    result.old_to_new[static_cast<std::size_t>(old_idx)] = static_cast<IdxType>(new_idx);
                }
                if (old_idx >= 0 && old_idx < static_cast<IdxType>(local_to_global.size()))
                {
                    result.local_to_global.push_back(local_to_global[static_cast<std::size_t>(old_idx)]);
                }
                else
                {
                    result.local_to_global.push_back(old_idx);
                }
            }

            result.gates.reserve(local_gates.size());
            for (const auto &gate : local_gates)
            {
                Gate adjusted = gate;
                if (adjusted.qubit >= 0 && adjusted.qubit < static_cast<IdxType>(result.old_to_new.size()))
                {
                    adjusted.qubit = result.old_to_new[static_cast<std::size_t>(adjusted.qubit)];
                }
                if (adjusted.ctrl >= 0 && adjusted.ctrl < static_cast<IdxType>(result.old_to_new.size()))
                {
                    adjusted.ctrl = result.old_to_new[static_cast<std::size_t>(adjusted.ctrl)];
                }
                if (adjusted.extra >= 0 && adjusted.extra < static_cast<IdxType>(result.old_to_new.size()))
                {
                    adjusted.extra = result.old_to_new[static_cast<std::size_t>(adjusted.extra)];
                }
                result.gates.push_back(adjusted);
            }

            result.logical_mapping = logical_mapping;
            for (auto &value : result.logical_mapping)
            {
                if (value >= 0 && value < static_cast<IdxType>(result.old_to_new.size()))
                {
                    value = result.old_to_new[static_cast<std::size_t>(value)];
                }
                else
                {
                    value = -1;
                }
            }

            result.measurement_mapping = measurement_mapping;
            for (auto &value : result.measurement_mapping)
            {
                if (value >= 0 && value < static_cast<IdxType>(result.old_to_new.size()))
                {
                    value = result.old_to_new[static_cast<std::size_t>(value)];
                }
                else
                {
                    value = -1;
                }
            }

            result.device_json = build_subchip_json(subchip,
                                                    result.new_to_old,
                                                    result.old_to_new,
                                                    result.local_to_global,
                                                    result.logical_mapping,
                                                    subchip_name,
                                                    mode);
            return result;
        }

        void write_json_file(const nlohmann::json &data, const fs::path &output_path)
        {
            if (output_path.has_parent_path())
            {
                std::error_code ec;
                fs::create_directories(output_path.parent_path(), ec);
                if (ec)
                {
                    std::cerr << "Warning: failed to create directory '" << output_path.parent_path() << "' (" << ec.message() << ")" << std::endl;
                }
            }
            std::ofstream out(output_path);
            if (!out.is_open())
            {
                std::cerr << "Error: unable to open JSON output file '" << output_path << "'" << std::endl;
                return;
            }
            out << data.dump(2);
            out.close();
        }
    } // namespace cli
} // namespace QASMTrans
