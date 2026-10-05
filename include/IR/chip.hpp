#pragma once

#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <exception>
#include <optional>

#include "../nlomann/json.hpp"
#include "graph.hpp"
#include <limits.h>

using json = nlohmann::json;

using namespace std;
using namespace QASMTrans;

namespace QASMTrans
{
    enum class ChipMetadataMode
    {
        TopologyOnly,
        Full
    };

    class Chip
    {
    public:
        // Constructor
        Chip(IdxType num_qubits, const vector<vector<IdxType>> &adjacency_matrix, const vector<vector<IdxType>> &edges, const vector<vector<IdxType>> &dism)
            : qubit_num(num_qubits), adj_mat(adjacency_matrix), edge_list(edges), distance_mat(dism) {}

    public:
        IdxType qubit_num;
        IdxType chip_qubit_num;
        vector<vector<IdxType>> adj_mat;
        vector<vector<IdxType>> edge_list;
        vector<vector<IdxType>> distance_mat;
        std::vector<std::unordered_set<IdxType>> directed_edge_list;
        std::vector<std::unordered_map<std::string, double>> single_qubit_errors;
        std::map<std::pair<IdxType, IdxType>, std::unordered_map<std::string, double>> two_qubit_errors;
        std::vector<std::unordered_map<std::string, double>> single_qubit_gate_lengths;
        std::map<std::pair<IdxType, IdxType>, std::unordered_map<std::string, double>> two_qubit_gate_lengths;
        std::vector<std::optional<double>> t1;
        std::vector<std::optional<double>> t2;
        std::vector<std::optional<double>> freq;
        std::vector<std::optional<double>> readout_length;
        std::vector<std::optional<double>> prob_meas0_prep1;
        std::vector<std::optional<double>> prob_meas1_prep0;
    };

    inline vector<vector<IdxType>> floyd(IdxType node_num, vector<vector<IdxType>> &adj_mat)
    {
        vector<vector<IdxType>> distance_mat(node_num, vector<IdxType>(node_num));
        for (IdxType i = 0; i < node_num; ++i)
        {
            for (IdxType j = 0; j < node_num; ++j)
            {
                if (adj_mat[i][j] != 0)
                {
                    distance_mat[i][j] = adj_mat[i][j];
                }
                else
                {
                    distance_mat[i][j] = INT_MAX;
                }
            }
            distance_mat[i][i] = 0;
        }
        for (IdxType k = 0; k < node_num; ++k)
        {
            for (IdxType i = 0; i < node_num; ++i)
            {
                for (IdxType j = 0; j < node_num; ++j)
                {
                    if (distance_mat[i][k] != INT_MAX && distance_mat[k][j] != INT_MAX && distance_mat[i][j] > distance_mat[i][k] + distance_mat[k][j])
                    {
                        distance_mat[i][j] = distance_mat[i][k] + distance_mat[k][j];
                    }
                }
            }
        }
        return distance_mat;
    }

    inline const json &load_backend_config_cached(const std::string &backendpath)
    {
        static std::unordered_map<std::string, json> cache;
        auto it = cache.find(backendpath);
        if (it != cache.end())
        {
            return it->second;
        }

        ifstream f(backendpath);
        if (f.fail())
        {
            throw logic_error("Device config file not found at " + backendpath);
        }

        json backend_config = json::parse(f, nullptr, true, true);
        return cache.emplace(backendpath, std::move(backend_config)).first->second;
    }

    inline shared_ptr<Chip> constructChip(IdxType qubit_num,
                                          string backendpath,
                                          bool run_with_limit,
                                          IdxType debug_level,
                                          ChipMetadataMode metadata_mode = ChipMetadataMode::Full)
    {
        // string path = "../data/device/" +backend_name+ ".json";
        // string path = "/Users/lian599/local/QASMTrans/data/devices/" +backend_name+ ".json";
        // string path = backend_name;

        bool limited_arc = run_with_limit;
        const json &backend_config = load_backend_config_cached(backendpath);
        vector<pair<IdxType, IdxType>> pairs;
        IdxType chip_qubit_num = backend_config["num_qubits"];
        auto cx_coupling = backend_config["cx_coupling"];
        unordered_set<IdxType> connected;
        connected.reserve(static_cast<size_t>(chip_qubit_num));
        bool has_cx_coupling = cx_coupling.is_array() && !cx_coupling.empty();
        // Iterate over the array
        for (const auto &item : cx_coupling)
        {
            // Split the string IdxTypeo two parts
            stringstream ss(item.get<string>());
            string part;
            getline(ss, part, '_');
            IdxType first = stoi(part);
            getline(ss, part, '_');
            IdxType second = stoi(part);
            if (!limited_arc)
            {
                pairs.push_back(make_pair(first, second));
                connected.insert(first);
                connected.insert(second);
            }
            else
            {
                if (first < qubit_num && second < qubit_num)
                {
                    pairs.push_back(make_pair(first, second));
                    connected.insert(first);
                    connected.insert(second);
                }
            }
        }
        IdxType expected_qubits = limited_arc ? std::min(qubit_num, chip_qubit_num) : chip_qubit_num;
        if (has_cx_coupling && expected_qubits > 1)
        {
            for (IdxType q = 0; q < expected_qubits; ++q)
            {
                if (!connected.count(q))
                {
                    std::ostringstream msg;
                    msg << "Disconnected qubit " << q << " in cx_coupling; "
                        << "cx_coupling must include each qubit at least once.";
                    throw logic_error(msg.str());
                }
            }
        }
        vector<pair<IdxType, IdxType>> edges;
        vector<std::unordered_set<IdxType>> directed_edges;
        directed_edges.resize(static_cast<size_t>(chip_qubit_num));
        for (auto &p : pairs)
        {
            edges.push_back(make_pair(p.first, p.second));
            if (p.first >= 0 && p.first < chip_qubit_num &&
                p.second >= 0 && p.second < chip_qubit_num)
            {
                directed_edges[static_cast<size_t>(p.first)].insert(p.second);
            }
        }
        Graph graph(edges);
        unordered_set<IdxType> vertices = graph.getVertices();
        vector<pair<IdxType, IdxType>> retrievedEdges = graph.getEdges();
        vector<vector<IdxType>> adj_mat(vertices.size(), vector<IdxType>(vertices.size(), 0));
        for (const auto &edge : retrievedEdges)
        {
            adj_mat[edge.first][edge.second] = 1;
            adj_mat[edge.second][edge.first] = 1;
        }
        vector<vector<IdxType>> edge_list;
        for (IdxType i = 0; i < adj_mat.size(); i++)
        {
            vector<IdxType> edges;
            for (IdxType j = 0; j < adj_mat[i].size(); j++)
            {
                if (adj_mat[i][j] == 1)
                {
                    edges.push_back(j);
                }
            }
            edge_list.push_back(edges);
        }
        vector<vector<IdxType>> distance_mat(vertices.size(), vector<IdxType>(vertices.size(), 0));
        distance_mat = floyd(vertices.size(), adj_mat);
        shared_ptr<Chip> chip = make_shared<Chip>(distance_mat.size(), adj_mat, edge_list, distance_mat);
        chip->chip_qubit_num = chip_qubit_num;
        chip->directed_edge_list = std::move(directed_edges);
        chip->single_qubit_errors.assign(chip->chip_qubit_num, {});
        chip->single_qubit_gate_lengths.assign(chip->chip_qubit_num, {});
        chip->t1.assign(chip->chip_qubit_num, std::nullopt);
        chip->t2.assign(chip->chip_qubit_num, std::nullopt);
        chip->freq.assign(chip->chip_qubit_num, std::nullopt);
        chip->readout_length.assign(chip->chip_qubit_num, std::nullopt);
        chip->prob_meas0_prep1.assign(chip->chip_qubit_num, std::nullopt);
        chip->prob_meas1_prep0.assign(chip->chip_qubit_num, std::nullopt);

        if (metadata_mode == ChipMetadataMode::TopologyOnly)
        {
            return chip;
        }

        if (backend_config.contains("gate_errs"))
        {
            const auto &gate_errs = backend_config["gate_errs"];
            for (auto it = gate_errs.begin(); it != gate_errs.end(); ++it)
            {
                if (it.value().is_null())
                {
                    continue;
                }
                const std::string gate_identifier = it.key();
                double error_value = it.value();

                size_t first_digit = gate_identifier.find_first_of("0123456789");
                if (first_digit == std::string::npos)
                {
                    continue;
                }

                std::string gate_name = gate_identifier.substr(0, first_digit);
                if (gate_name.empty())
                {
                    continue;
                }

                size_t underscore_pos = gate_identifier.find('_', first_digit);
                try
                {
                    if (underscore_pos == std::string::npos)
                    {
                        IdxType qubit = static_cast<IdxType>(std::stoll(gate_identifier.substr(first_digit)));
                        if (qubit >= 0 && qubit < static_cast<IdxType>(chip->single_qubit_errors.size()))
                        {
                            chip->single_qubit_errors[qubit][gate_name] = error_value;
                        }
                    }
                    else
                    {
                        IdxType first_qubit = static_cast<IdxType>(std::stoll(gate_identifier.substr(first_digit, underscore_pos - first_digit)));
                        IdxType second_qubit = static_cast<IdxType>(std::stoll(gate_identifier.substr(underscore_pos + 1)));
                        if (first_qubit >= 0 && first_qubit < chip->chip_qubit_num &&
                            second_qubit >= 0 && second_qubit < chip->chip_qubit_num)
                        {
                            std::pair<IdxType, IdxType> key = {first_qubit, second_qubit};
                            chip->two_qubit_errors[key][gate_name] = error_value;
                        }
                    }
                }
                catch (const std::exception &)
                {
                    continue;
                }
            }
        }
        if (backend_config.contains("gate_lens"))
        {
            const auto &gate_lens = backend_config["gate_lens"];
            for (auto it = gate_lens.begin(); it != gate_lens.end(); ++it)
            {
                if (it.value().is_null())
                {
                    continue;
                }

                const std::string gate_identifier = it.key();
                double length_value = it.value();

                size_t first_digit = gate_identifier.find_first_of("0123456789");
                if (first_digit == std::string::npos)
                {
                    continue;
                }

                std::string gate_name = gate_identifier.substr(0, first_digit);
                if (gate_name.empty())
                {
                    continue;
                }

                size_t underscore_pos = gate_identifier.find('_', first_digit);
                try
                {
                    if (underscore_pos == std::string::npos)
                    {
                        IdxType qubit = static_cast<IdxType>(std::stoll(gate_identifier.substr(first_digit)));
                        if (qubit >= 0 && qubit < static_cast<IdxType>(chip->single_qubit_gate_lengths.size()))
                        {
                            chip->single_qubit_gate_lengths[qubit][gate_name] = length_value;
                        }
                    }
                    else
                    {
                        IdxType first_qubit = static_cast<IdxType>(std::stoll(gate_identifier.substr(first_digit, underscore_pos - first_digit)));
                        IdxType second_qubit = static_cast<IdxType>(std::stoll(gate_identifier.substr(underscore_pos + 1)));
                        if (first_qubit >= 0 && first_qubit < chip->chip_qubit_num &&
                            second_qubit >= 0 && second_qubit < chip->chip_qubit_num)
                        {
                            std::pair<IdxType, IdxType> key = {first_qubit, second_qubit};
                            chip->two_qubit_gate_lengths[key][gate_name] = length_value;
                        }
                    }
                }
                catch (const std::exception &)
                {
                    continue;
                }
            }
        }

        auto parse_qubit_property = [&](const char *key, std::vector<std::optional<double>> &target) {
            if (!backend_config.contains(key))
            {
                return;
            }
            const auto &obj = backend_config[key];
            if (!obj.is_object())
            {
                return;
            }
            for (auto it = obj.begin(); it != obj.end(); ++it)
            {
                if (!it.value().is_number())
                {
                    continue;
                }
                try
                {
                    IdxType qubit = static_cast<IdxType>(std::stoll(it.key()));
                    if (qubit >= 0 && qubit < chip->chip_qubit_num)
                    {
                        target[static_cast<std::size_t>(qubit)] = it.value().get<double>();
                    }
                }
                catch (const std::exception &)
                {
                    continue;
                }
            }
        };

        parse_qubit_property("T1", chip->t1);
        parse_qubit_property("T2", chip->t2);
        parse_qubit_property("freq", chip->freq);
        parse_qubit_property("readout_length", chip->readout_length);
        parse_qubit_property("prob_meas0_prep1", chip->prob_meas0_prep1);
        parse_qubit_property("prob_meas1_prep0", chip->prob_meas1_prep0);
        return chip;
    }

}
