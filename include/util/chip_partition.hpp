#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../QASMTransPrimitives.hpp"
#include "../IR/chip.hpp"

namespace QASMTrans
{

inline std::vector<std::vector<IdxType>> partition_chip(const std::shared_ptr<Chip> &chip,
                                                        const std::vector<IdxType> &partition_sizes)
{
    if (!chip)
    {
        throw std::invalid_argument("partition_chip: null chip pointer");
    }

    if (partition_sizes.empty())
    {
        return {};
    }

    const std::size_t node_count = static_cast<std::size_t>(chip->chip_qubit_num);
    if (node_count == 0)
    {
        throw std::runtime_error("partition_chip: device graph has no qubits");
    }

    std::vector<IdxType> available_nodes(node_count);
    std::iota(available_nodes.begin(), available_nodes.end(), 0);

    IdxType total_requested = 0;
    for (auto size : partition_sizes)
    {
        if (size <= 0)
        {
            throw std::invalid_argument("partition_chip: partition sizes must be positive");
        }
        total_requested += size;
    }
    if (total_requested > static_cast<IdxType>(available_nodes.size()))
    {
        throw std::invalid_argument("partition_chip: requested qubits exceed device capacity");
    }

    std::vector<IdxType> degrees(available_nodes.size(), 0);
    for (IdxType node = 0; node < static_cast<IdxType>(chip->edge_list.size()); ++node)
    {
        degrees[node] = static_cast<IdxType>(chip->edge_list[node].size());
    }

    const double kMinError = 1e-6;
    const double kDefaultSingleError = 5e-3;
    const double kDefaultTwoQubitError = 5e-2;

    auto lookup_two_qubit_error = [&](IdxType ctrl, IdxType tgt) -> double
    {
        if (ctrl < 0 || tgt < 0)
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
        auto it = chip->two_qubit_errors.find({ctrl, tgt});
        if (it == chip->two_qubit_errors.end() || it->second.empty())
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
        auto cx_it = it->second.find("cx");
        if (cx_it != it->second.end())
        {
            return cx_it->second;
        }
        double sum = 0.0;
        for (const auto &entry : it->second)
        {
            sum += entry.second;
        }
        return sum / static_cast<double>(it->second.size());
    };

    auto edge_error = [&](IdxType u, IdxType v) -> double
    {
        double direct = lookup_two_qubit_error(u, v);
        double reverse = lookup_two_qubit_error(v, u);

        double best = std::numeric_limits<double>::quiet_NaN();
        if (!std::isnan(direct))
        {
            best = direct;
        }
        if (!std::isnan(reverse))
        {
            best = std::isnan(best) ? reverse : std::min(best, reverse);
        }

        if (std::isnan(best))
        {
            return kDefaultTwoQubitError;
        }
        return std::max(best, kMinError);
    };

    auto single_qubit_error = [&](IdxType q) -> double
    {
        if (q < 0 || q >= static_cast<IdxType>(chip->single_qubit_errors.size()))
        {
            return kDefaultSingleError;
        }
        const auto &err_map = chip->single_qubit_errors[q];
        if (err_map.empty())
        {
            return kDefaultSingleError;
        }
        double sum = 0.0;
        for (const auto &entry : err_map)
        {
            sum += entry.second;
        }
        return std::max(sum / static_cast<double>(err_map.size()), kMinError);
    };

    std::vector<double> node_penalty(available_nodes.size(), kDefaultSingleError + kDefaultTwoQubitError);
    for (IdxType node = 0; node < static_cast<IdxType>(available_nodes.size()); ++node)
    {
        double sq_error = single_qubit_error(node);
        double link_sum = 0.0;
        if (node < static_cast<IdxType>(chip->edge_list.size()))
        {
            for (IdxType neighbour : chip->edge_list[node])
            {
                link_sum += edge_error(node, neighbour);
            }
        }
        double degree = static_cast<double>(degrees[node]);
        double link_avg = degree > 0.0 ? link_sum / degree : kDefaultTwoQubitError;
        double penalty = sq_error + link_avg;
        node_penalty[node] = std::max(penalty, kMinError);
    }

    struct PartitionRequest
    {
        IdxType size;
        std::size_t original_index;
    };

    std::cout << "[partition] Requested sizes:";
    for (IdxType size : partition_sizes)
    {
        std::cout << " " << size;
    }
    std::cout << std::endl;

    std::vector<PartitionRequest> requests;
    requests.reserve(partition_sizes.size());
    for (std::size_t i = 0; i < partition_sizes.size(); ++i)
    {
        requests.push_back(PartitionRequest{partition_sizes[i], i});
    }

    std::sort(requests.begin(), requests.end(),
              [](const PartitionRequest &a, const PartitionRequest &b)
              {
                  if (a.size != b.size)
                  {
                      return a.size > b.size;
                  }
                  return a.original_index < b.original_index;
              });

    std::vector<std::vector<IdxType>> result(partition_sizes.size());
    std::vector<char> used(available_nodes.size(), 0);
    std::vector<int> owner(available_nodes.size(), -1);

    const auto &distance_mat = chip->distance_mat;
    bool has_distance_matrix = distance_mat.size() == available_nodes.size();

    struct FrontierCandidate
    {
        IdxType node;
        IdxType degree;
        double penalty;
    };
    struct FrontierCompare
    {
        bool operator()(const FrontierCandidate &lhs, const FrontierCandidate &rhs) const
        {
            if (lhs.degree != rhs.degree)
            {
                return lhs.degree > rhs.degree;
            }
            if (lhs.penalty != rhs.penalty)
            {
                return lhs.penalty > rhs.penalty;
            }
            return lhs.node > rhs.node;
        }
    };

    using FrontierQueue = std::priority_queue<FrontierCandidate, std::vector<FrontierCandidate>, FrontierCompare>;

    struct RegionState
    {
        std::vector<IdxType> nodes;
        FrontierQueue frontier;
    };

    std::vector<RegionState> regions(requests.size());
    std::vector<IdxType> target_sizes(requests.size(), 0);

    auto enqueue_neighbors = [&](std::size_t region_id, IdxType node)
    {
        if (node < 0 || node >= static_cast<IdxType>(chip->edge_list.size()))
        {
            return;
        }
        for (IdxType neighbour : chip->edge_list[node])
        {
            if (neighbour < 0 || neighbour >= static_cast<IdxType>(owner.size()))
            {
                continue;
            }
            if (owner[neighbour] == static_cast<int>(region_id))
            {
                continue;
            }
            regions[region_id].frontier.push(FrontierCandidate{neighbour, degrees[neighbour], node_penalty[neighbour]});
        }
    };

    auto add_node_to_region = [&](std::size_t region_id, IdxType node)
    {
        if (node < 0 || node >= static_cast<IdxType>(owner.size()))
        {
            return;
        }
        auto &region = regions[region_id];
        region.nodes.push_back(node);
        owner[node] = static_cast<int>(region_id);
        used[node] = 1;
        enqueue_neighbors(region_id, node);
    };

    auto remove_node_from_region = [&](std::size_t region_id, IdxType node)
    {
        if (node < 0 || node >= static_cast<IdxType>(owner.size()))
        {
            return;
        }
        auto &region = regions[region_id];
        auto it = std::find(region.nodes.begin(), region.nodes.end(), node);
        if (it != region.nodes.end())
        {
            region.nodes.erase(it);
        }
        owner[node] = -1;
        used[node] = 0;
    };

    auto is_leaf_in_region = [&](std::size_t region_id, IdxType node) -> bool
    {
        if (node < 0 || node >= static_cast<IdxType>(owner.size()))
        {
            return false;
        }
        if (owner[node] != static_cast<int>(region_id))
        {
            return false;
        }
        if (node >= static_cast<IdxType>(chip->edge_list.size()))
        {
            return true;
        }
        int neighbours_in_region = 0;
        for (IdxType neighbour : chip->edge_list[node])
        {
            if (neighbour < 0 || neighbour >= static_cast<IdxType>(owner.size()))
            {
                continue;
            }
            if (owner[neighbour] == static_cast<int>(region_id))
            {
                ++neighbours_in_region;
                if (neighbours_in_region > 1)
                {
                    return false;
                }
            }
        }
        return true;
    };

    auto transfer_node = [&](std::size_t donor_id, std::size_t receiver_id, IdxType node)
    {
        remove_node_from_region(donor_id, node);
        enqueue_neighbors(donor_id, node);
        add_node_to_region(receiver_id, node);
    };

    auto choose_far_seed = [&](const std::vector<std::size_t> &assigned_regions) -> IdxType
    {
        if (!has_distance_matrix)
        {
            IdxType fallback = -1;
            for (IdxType node = 0; node < static_cast<IdxType>(owner.size()); ++node)
            {
                if (owner[node] == -1)
                {
                    fallback = node;
                    break;
                }
            }
            return fallback;
        }

        IdxType best_node = -1;
        IdxType best_distance = -1;
        for (IdxType node = 0; node < static_cast<IdxType>(owner.size()); ++node)
        {
            if (owner[node] != -1)
            {
                continue;
            }

            IdxType min_dist = std::numeric_limits<IdxType>::max();
            if (!assigned_regions.empty())
            {
                for (std::size_t region_id : assigned_regions)
                {
                    for (IdxType seed_node : regions[region_id].nodes)
                    {
                        IdxType dist = distance_mat[node][seed_node];
                        if (dist == std::numeric_limits<IdxType>::max())
                        {
                            continue;
                        }
                        if (dist < min_dist)
                        {
                            min_dist = dist;
                        }
                    }
                }
            }
            else
            {
                min_dist = 0;
            }

            if (min_dist == std::numeric_limits<IdxType>::max())
            {
                continue;
            }
            if (best_node == -1 || min_dist > best_distance)
            {
                best_distance = min_dist;
                best_node = node;
            }
        }
        return best_node;
    };

    std::vector<std::size_t> seeded_regions;
    for (std::size_t idx = 0; idx < requests.size(); ++idx)
    {
        target_sizes[idx] = requests[idx].size;
        std::cout << "[partition] Planning request " << requests[idx].original_index
                  << " (size " << requests[idx].size << ")" << std::endl;

        IdxType seed = choose_far_seed(seeded_regions);
        if (seed < 0)
        {
            throw std::runtime_error("partition_chip: insufficient free qubits for seeding");
        }
        std::cout << "[partition] Seed for request " << requests[idx].original_index << ": " << seed << std::endl;
        add_node_to_region(idx, seed);
        seeded_regions.push_back(idx);
    }

    auto try_grow_once = [&](std::size_t region_id) -> bool
    {
        auto &region = regions[region_id];
        while (!region.frontier.empty())
        {
            FrontierCandidate candidate = region.frontier.top();
            region.frontier.pop();

            if (candidate.node < 0 || candidate.node >= static_cast<IdxType>(owner.size()))
            {
                continue;
            }

            if (owner[candidate.node] == static_cast<int>(region_id))
            {
                continue;
            }

            if (owner[candidate.node] == -1)
            {
                add_node_to_region(region_id, candidate.node);
                return true;
            }
        }

        auto find_unused_random = [&]() -> IdxType
        {
            for (IdxType node = 0; node < static_cast<IdxType>(owner.size()); ++node)
            {
                if (owner[node] == -1)
                {
                    return node;
                }
            }
            return -1;
        };

        IdxType unused = find_unused_random();
        if (unused >= 0)
        {
            add_node_to_region(region_id, unused);
            return true;
        }

        IdxType best_node = -1;
        std::size_t donor_id = std::numeric_limits<std::size_t>::max();
        std::size_t donor_size = 0;

        for (IdxType node : region.nodes)
        {
            if (node < 0 || node >= static_cast<IdxType>(chip->edge_list.size()))
            {
                continue;
            }
            for (IdxType neighbour : chip->edge_list[node])
            {
                if (neighbour < 0 || neighbour >= static_cast<IdxType>(owner.size()))
                {
                    continue;
                }
                int neighbour_owner = owner[neighbour];
                if (neighbour_owner < 0 || neighbour_owner == static_cast<int>(region_id))
                {
                    continue;
                }

                std::size_t neighbour_region = static_cast<std::size_t>(neighbour_owner);
                std::size_t neighbour_size = regions[neighbour_region].nodes.size();
                if (neighbour_size > donor_size && neighbour_size > 1)
                {
                    if (!is_leaf_in_region(neighbour_region, neighbour))
                    {
                        continue;
                    }
                    donor_size = neighbour_size;
                    donor_id = neighbour_region;
                    best_node = neighbour;
                }
            }
        }

        if (best_node >= 0 && donor_id != std::numeric_limits<std::size_t>::max())
        {
            transfer_node(donor_id, region_id, best_node);
            return true;
        }

        return false;
    };

    const std::size_t max_iterations = available_nodes.size() * 1000 + 1;
    std::size_t iteration = 0;

    while (true)
    {
        bool needs_more = false;
        bool unassigned_exists = false;
        for (std::size_t idx = 0; idx < regions.size(); ++idx)
        {
            if (regions[idx].nodes.size() < static_cast<std::size_t>(target_sizes[idx]))
            {
                needs_more = true;
                break;
            }
        }
        for (IdxType node = 0; node < static_cast<IdxType>(owner.size()); ++node)
        {
            if (owner[node] == -1)
            {
                unassigned_exists = true;
                break;
            }
        }

        if (!needs_more && !unassigned_exists)
        {
            break;
        }

        bool progress = false;

        for (std::size_t idx = 0; idx < regions.size(); ++idx)
        {
            bool region_needs = regions[idx].nodes.size() < static_cast<std::size_t>(target_sizes[idx]);
            bool should_grow = region_needs || unassigned_exists;
            if (!should_grow)
            {
                continue;
            }
            if (try_grow_once(idx))
            {
                progress = true;
            }
        }

        if (!progress)
        {
            break;
        }

        if (++iteration > max_iterations)
        {
            throw std::runtime_error("partition_chip: exceeded iteration limit during allocation");
        }
    }

    for (IdxType node = 0; node < static_cast<IdxType>(owner.size()); ++node)
    {
        if (owner[node] != -1)
        {
            continue;
        }

        std::optional<std::size_t> attach_region;
        if (node < static_cast<IdxType>(chip->edge_list.size()))
        {
            for (IdxType neighbour : chip->edge_list[node])
            {
                if (neighbour < 0 || neighbour >= static_cast<IdxType>(owner.size()))
                {
                    continue;
                }
                if (owner[neighbour] >= 0)
                {
                    attach_region = static_cast<std::size_t>(owner[neighbour]);
                    break;
                }
            }
        }
        if (!attach_region.has_value())
        {
            attach_region = 0;
        }
        add_node_to_region(*attach_region, node);
    }

    auto rebalance = [&]()
    {
        bool moved = true;
        while (moved)
        {
            moved = false;
            for (std::size_t receiver_id = 0; receiver_id < regions.size(); ++receiver_id)
            {
                if (regions[receiver_id].nodes.size() >= static_cast<std::size_t>(target_sizes[receiver_id]))
                {
                    continue;
                }

                IdxType best_node = -1;
                std::size_t donor_id = std::numeric_limits<std::size_t>::max();
                std::size_t best_surplus = 0;

                for (IdxType node : regions[receiver_id].nodes)
                {
                    if (node < 0 || node >= static_cast<IdxType>(chip->edge_list.size()))
                    {
                        continue;
                    }
                    for (IdxType neighbour : chip->edge_list[node])
                    {
                        if (neighbour < 0 || neighbour >= static_cast<IdxType>(owner.size()))
                        {
                            continue;
                        }
                        int neighbour_owner = owner[neighbour];
                        if (neighbour_owner < 0 || neighbour_owner == static_cast<int>(receiver_id))
                        {
                            continue;
                        }

                        std::size_t donor_region = static_cast<std::size_t>(neighbour_owner);
                        std::size_t donor_size = regions[donor_region].nodes.size();
                        if (donor_size <= static_cast<std::size_t>(target_sizes[donor_region]))
                        {
                            continue;
                        }
                        if (donor_size <= 1)
                        {
                            continue;
                        }
                        if (!is_leaf_in_region(donor_region, neighbour))
                        {
                            continue;
                        }

                        if (donor_size > best_surplus)
                        {
                            best_surplus = donor_size;
                            donor_id = donor_region;
                            best_node = neighbour;
                        }
                    }
                }

                if (best_node >= 0 && donor_id != std::numeric_limits<std::size_t>::max())
                {
                    transfer_node(donor_id, receiver_id, best_node);
                    moved = true;
                    break;
                }
            }
        }
    };

    rebalance();

    for (std::size_t idx = 0; idx < regions.size(); ++idx)
    {
        auto &region = regions[idx];
        std::sort(region.nodes.begin(), region.nodes.end());
        result[requests[idx].original_index] = region.nodes;

        if (region.nodes.size() < static_cast<std::size_t>(target_sizes[idx]))
        {
            throw std::runtime_error("partition_chip: unable to satisfy requested partition sizes");
        }
    }

    std::cout << "[partition] Final partitions:" << std::endl;
    for (std::size_t idx = 0; idx < result.size(); ++idx)
    {
        std::cout << "  [" << idx << "] ";
        for (IdxType node : result[idx])
        {
            std::cout << node << " ";
        }
        std::cout << std::endl;
    }

    return result;
}

inline std::shared_ptr<Chip> make_subchip(const std::shared_ptr<Chip> &chip,
                                          const std::vector<IdxType> &nodes,
                                          std::vector<IdxType> &local_to_global)
{
    if (!chip)
    {
        throw std::invalid_argument("make_subchip: null chip pointer");
    }
    if (nodes.empty())
    {
        throw std::invalid_argument("make_subchip: empty node set");
    }

    const std::size_t local_n = nodes.size();
    local_to_global = nodes;

    std::unordered_map<IdxType, IdxType> global_to_local;
    global_to_local.reserve(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        global_to_local[nodes[i]] = static_cast<IdxType>(i);
    }

    std::vector<std::vector<IdxType>> adj(local_n, std::vector<IdxType>(local_n, 0));
    std::vector<std::vector<IdxType>> edge_list(local_n);
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        IdxType global_src = nodes[i];
        if (global_src < 0 || global_src >= static_cast<IdxType>(chip->edge_list.size()))
        {
            continue;
        }
        for (IdxType neighbour : chip->edge_list[global_src])
        {
            auto it = global_to_local.find(neighbour);
            if (it == global_to_local.end())
            {
                continue;
            }
            IdxType local_dst = it->second;
            adj[i][local_dst] = 1;
        }
    }
    for (std::size_t i = 0; i < local_n; ++i)
    {
        for (std::size_t j = 0; j < local_n; ++j)
        {
            if (adj[i][j] == 1)
            {
                edge_list[i].push_back(static_cast<IdxType>(j));
            }
        }
    }

    std::vector<std::vector<IdxType>> distance_mat = floyd(static_cast<IdxType>(local_n), adj);
    auto subchip = std::make_shared<Chip>(static_cast<IdxType>(local_n), adj, edge_list, distance_mat);
    subchip->chip_qubit_num = static_cast<IdxType>(local_n);
    subchip->directed_edge_list.assign(local_n, {});

    subchip->single_qubit_errors.assign(local_n, {});
    subchip->single_qubit_gate_lengths.assign(local_n, {});
    subchip->t1.assign(local_n, std::nullopt);
    subchip->t2.assign(local_n, std::nullopt);
    subchip->freq.assign(local_n, std::nullopt);
    subchip->readout_length.assign(local_n, std::nullopt);
    subchip->prob_meas0_prep1.assign(local_n, std::nullopt);
    subchip->prob_meas1_prep0.assign(local_n, std::nullopt);
    for (std::size_t i = 0; i < local_n; ++i)
    {
        IdxType global = nodes[i];
        if (global < 0 || global >= static_cast<IdxType>(chip->single_qubit_errors.size()))
        {
            continue;
        }
        subchip->single_qubit_errors[i] = chip->single_qubit_errors[global];
        subchip->single_qubit_gate_lengths[i] = chip->single_qubit_gate_lengths[global];
        if (global >= 0 && global < static_cast<IdxType>(chip->directed_edge_list.size()))
        {
            for (IdxType global_tgt : chip->directed_edge_list[static_cast<std::size_t>(global)])
            {
                auto it_tgt = global_to_local.find(global_tgt);
                if (it_tgt == global_to_local.end())
                {
                    continue;
                }
                subchip->directed_edge_list[i].insert(it_tgt->second);
            }
        }
        if (global >= 0 && global < static_cast<IdxType>(chip->t1.size()))
        {
            subchip->t1[i] = chip->t1[global];
        }
        if (global >= 0 && global < static_cast<IdxType>(chip->t2.size()))
        {
            subchip->t2[i] = chip->t2[global];
        }
        if (global >= 0 && global < static_cast<IdxType>(chip->freq.size()))
        {
            subchip->freq[i] = chip->freq[global];
        }
        if (global >= 0 && global < static_cast<IdxType>(chip->readout_length.size()))
        {
            subchip->readout_length[i] = chip->readout_length[global];
        }
        if (global >= 0 && global < static_cast<IdxType>(chip->prob_meas0_prep1.size()))
        {
            subchip->prob_meas0_prep1[i] = chip->prob_meas0_prep1[global];
        }
        if (global >= 0 && global < static_cast<IdxType>(chip->prob_meas1_prep0.size()))
        {
            subchip->prob_meas1_prep0[i] = chip->prob_meas1_prep0[global];
        }
    }

    for (const auto &entry : chip->two_qubit_errors)
    {
        auto it_ctrl = global_to_local.find(entry.first.first);
        auto it_tgt = global_to_local.find(entry.first.second);
        if (it_ctrl == global_to_local.end() || it_tgt == global_to_local.end())
        {
            continue;
        }
        subchip->two_qubit_errors[{it_ctrl->second, it_tgt->second}] = entry.second;
    }
    for (const auto &entry : chip->two_qubit_gate_lengths)
    {
        auto it_ctrl = global_to_local.find(entry.first.first);
        auto it_tgt = global_to_local.find(entry.first.second);
        if (it_ctrl == global_to_local.end() || it_tgt == global_to_local.end())
        {
            continue;
        }
        subchip->two_qubit_gate_lengths[{it_ctrl->second, it_tgt->second}] = entry.second;
    }

    return subchip;
}

} // namespace QASMTrans
