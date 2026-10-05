#pragma once

#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <vector>
#include <map>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>

#include "QASMTransPrimitives.hpp"
#include "IR/chip.hpp"
#include "IR/gate.hpp"
#include "nlomann/json.hpp"

namespace QASMTrans
{
    extern std::unordered_set<std::string> g_device_basis_gates;
    extern std::unordered_map<std::string, std::string> g_merged_gate_aliases;

    namespace cli
    {
        // CLI helper utilities
        std::string derive_pulse_output_path(const std::string &qasm_output_path);
        IdxType mode_from_string(const std::string &mode_name);

        struct PrunedSubchipData
        {
            std::vector<IdxType> new_to_old;
            std::vector<IdxType> old_to_new;
            std::vector<QASMTrans::Gate> gates;
            std::vector<IdxType> logical_mapping;
            std::vector<IdxType> measurement_mapping;
            std::vector<IdxType> local_to_global;
            nlohmann::json device_json;
        };

        void ingest_backend_metadata(const std::string &backendpath,
                                     std::unordered_set<std::string> &device_basis_gates,
                                     std::unordered_map<std::string, std::string> &merged_gate_aliases,
                                     bool strict = false);

        template <typename CregMap>
        std::vector<IdxType> build_measurement_mapping(const CregMap &cregs,
                                                       const std::vector<IdxType> &logical_to_physical)
        {
            constexpr IdxType undefined_index = static_cast<IdxType>(-1);
            std::vector<IdxType> mapping;
            for (const auto &entry : cregs)
            {
                const auto &qubit_indices = entry.second.qubit_indices;
                for (std::size_t pos = 0; pos < qubit_indices.size(); ++pos)
                {
                    IdxType logical_index = qubit_indices[pos];
                    if (logical_index < 0)
                    {
                        mapping.push_back(undefined_index);
                        continue;
                    }
                    if (logical_index >= static_cast<IdxType>(logical_to_physical.size()))
                    {
                        throw std::runtime_error("Invalid measurement mapping for creg '" +
                                                 entry.first + "' (logical index " +
                                                 std::to_string(logical_index) + ")");
                    }
                    mapping.push_back(logical_to_physical[static_cast<std::size_t>(logical_index)]);
                }
            }
            if (mapping.empty())
            {
                return logical_to_physical;
            }
            return mapping;
        }

        PrunedSubchipData prune_subchip_artifact(const std::shared_ptr<Chip> &subchip,
                                                 const std::vector<IdxType> &local_to_global,
                                                 const std::vector<QASMTrans::Gate> &local_gates,
                                                 const std::vector<IdxType> &logical_mapping,
                                                 const std::vector<IdxType> &measurement_mapping,
                                                 const std::string &subchip_name,
                                                 IdxType mode);

        void write_json_file(const nlohmann::json &data, const std::filesystem::path &output_path);
    } // namespace cli
} // namespace QASMTrans
