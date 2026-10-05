#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <queue>
#include <limits>
#include <unordered_set>
#include <vector>

#include "../QASMTransPrimitives.hpp"

#include "../IR/chip.hpp"
#include "../IR/circuit.hpp"
#include "../IR/gate.hpp"
#include "../IR/graph.hpp"

using namespace QASMTrans;
using namespace std;
using RoutedGatePair = std::array<IdxType, 2>;
constexpr IdxType kRoutingFutureWindow = 20;
constexpr IdxType kRoutingFutureCompactThreshold = kRoutingFutureWindow * 4;

enum class RoutingMode
{
    Sabre,
    ExecWindow
};

struct RoutingCandidateScore
{
    IdxType executable_now = 0;
    IdxType max_current_distance = std::numeric_limits<IdxType>::max();
    double anchor_avg = std::numeric_limits<double>::infinity();
    double current_avg = std::numeric_limits<double>::infinity();
    double future_avg = std::numeric_limits<double>::infinity();
    double scalar_cost = std::numeric_limits<double>::infinity();
};

inline RoutedGatePair canonical_swap_pair(RoutedGatePair pair)
{
    if (pair[0] > pair[1])
    {
        std::swap(pair[0], pair[1]);
    }
    return pair;
}

inline bool same_swap_edge(const RoutedGatePair &lhs, const RoutedGatePair &rhs)
{
    if (lhs[0] < 0 || lhs[1] < 0 || rhs[0] < 0 || rhs[1] < 0)
    {
        return false;
    }
    RoutedGatePair lhs_pair = canonical_swap_pair(lhs);
    RoutedGatePair rhs_pair = canonical_swap_pair(rhs);
    return lhs_pair[0] == rhs_pair[0] && lhs_pair[1] == rhs_pair[1];
}

IdxType pick_contiguous_layout_seed(const shared_ptr<Chip> &chip, IdxType physical_qubit_num)
{
    IdxType best_seed = 0;
    long long best_distance_sum = std::numeric_limits<long long>::max();
    IdxType best_degree = -1;
    for (IdxType qubit = 0; qubit < physical_qubit_num; ++qubit)
    {
        long long distance_sum = 0;
        for (IdxType other = 0; other < physical_qubit_num; ++other)
        {
            IdxType distance = chip->distance_mat[qubit][other];
            if (distance >= std::numeric_limits<IdxType>::max() / 4)
            {
                continue;
            }
            distance_sum += static_cast<long long>(distance);
        }
        IdxType degree = qubit < static_cast<IdxType>(chip->edge_list.size())
                             ? static_cast<IdxType>(chip->edge_list[qubit].size())
                             : 0;
        if (distance_sum < best_distance_sum ||
            (distance_sum == best_distance_sum && degree > best_degree) ||
            (distance_sum == best_distance_sum && degree == best_degree && qubit < best_seed))
        {
            best_seed = qubit;
            best_distance_sum = distance_sum;
            best_degree = degree;
        }
    }
    return best_seed;
}

vector<IdxType> build_contiguous_initial_mapping(const shared_ptr<Chip> &chip, IdxType logical_qubit_num)
{
    IdxType physical_qubit_num = static_cast<IdxType>(chip->distance_mat.size());
    vector<IdxType> mapping;
    mapping.reserve(static_cast<size_t>(logical_qubit_num));
    if (logical_qubit_num <= 0 || physical_qubit_num <= 0)
    {
        return mapping;
    }

    IdxType seed_qubit = pick_contiguous_layout_seed(chip, physical_qubit_num);
    vector<uint8_t> visited(static_cast<size_t>(physical_qubit_num), 0);
    queue<IdxType> frontier;
    frontier.push(seed_qubit);
    visited[static_cast<size_t>(seed_qubit)] = 1;

    while (!frontier.empty() && static_cast<IdxType>(mapping.size()) < logical_qubit_num)
    {
        IdxType qubit = frontier.front();
        frontier.pop();
        mapping.push_back(qubit);

        vector<IdxType> neighbors = chip->edge_list[qubit];
        sort(neighbors.begin(), neighbors.end(),
             [&](IdxType lhs, IdxType rhs)
             {
                 IdxType lhs_distance = chip->distance_mat[seed_qubit][lhs];
                 IdxType rhs_distance = chip->distance_mat[seed_qubit][rhs];
                 if (lhs_distance != rhs_distance)
                 {
                     return lhs_distance < rhs_distance;
                 }
                 IdxType lhs_degree = static_cast<IdxType>(chip->edge_list[lhs].size());
                 IdxType rhs_degree = static_cast<IdxType>(chip->edge_list[rhs].size());
                 if (lhs_degree != rhs_degree)
                 {
                     return lhs_degree > rhs_degree;
                 }
                 return lhs < rhs;
             });
        for (IdxType neighbor : neighbors)
        {
            if (neighbor < 0 || neighbor >= physical_qubit_num)
            {
                continue;
            }
            if (visited[static_cast<size_t>(neighbor)] != 0)
            {
                continue;
            }
            visited[static_cast<size_t>(neighbor)] = 1;
            frontier.push(neighbor);
        }
    }

    if (static_cast<IdxType>(mapping.size()) < logical_qubit_num)
    {
        vector<IdxType> remaining;
        remaining.reserve(static_cast<size_t>(physical_qubit_num));
        for (IdxType qubit = 0; qubit < physical_qubit_num; ++qubit)
        {
            if (visited[static_cast<size_t>(qubit)] == 0)
            {
                remaining.push_back(qubit);
            }
        }
        sort(remaining.begin(), remaining.end(),
             [&](IdxType lhs, IdxType rhs)
             {
                 IdxType lhs_distance = chip->distance_mat[seed_qubit][lhs];
                 IdxType rhs_distance = chip->distance_mat[seed_qubit][rhs];
                 if (lhs_distance != rhs_distance)
                 {
                     return lhs_distance < rhs_distance;
                 }
                 return lhs < rhs;
             });
        for (IdxType qubit : remaining)
        {
            mapping.push_back(qubit);
            if (static_cast<IdxType>(mapping.size()) >= logical_qubit_num)
            {
                break;
            }
        }
    }

    return mapping;
}

void DAG_generator(IdxType qubit_num,
                   const vector<RoutedGatePair> &circuit,
                   vector<IdxType> &gate_state,
                   vector<IdxType> &gate_dependency,
                   vector<RoutedGatePair> &following_gate_idx,
                   vector<IdxType> &first_layer_gates_idx)
{
    IdxType gate_num = circuit.size();
    vector<IdxType> current_gate_idx(qubit_num, -1);
    following_gate_idx.assign(gate_num, RoutedGatePair{-1, -1});
    gate_dependency.resize(gate_num, 0);
    for (IdxType i = 0; i < gate_num; i++)
    {
        const RoutedGatePair &gate = circuit[i];
        const IdxType prior_a = current_gate_idx[gate[0]];
        const IdxType prior_b = current_gate_idx[gate[1]];

        // Count distinct predecessor 2Q gates on the gate's two logical wires.
        // If both wires were last touched by the same predecessor, this gate only
        // depends on one earlier gate; otherwise it depends on two.
        if (prior_a == -1 && prior_b == -1)
        {
            first_layer_gates_idx.push_back(i);
            gate_state[i] = 2;
            gate_dependency[i] = 0;
        }
        else if (prior_a == -1 || prior_b == -1 || prior_a == prior_b)
        {
            gate_dependency[i] = 1;
        }
        else
        {
            gate_dependency[i] = 2;
        }
        for (IdxType j = 0; j < 2; j++)
        {
            IdxType qubit = gate[j];
            if (current_gate_idx[qubit] != -1)
            {
                const RoutedGatePair &prior_gate = circuit[current_gate_idx[qubit]];
                IdxType qubit_idx;
                if (prior_gate[j] != qubit)
                {
                    qubit_idx = 1 - j;
                }
                else
                {
                    qubit_idx = j;
                }
                following_gate_idx[current_gate_idx[qubit]][qubit_idx] = i;
            }
            current_gate_idx[qubit] = i;
        }
    }
}
// #gate_state
// # 0 - not considered
// # 1 - in future gate queue
// # 2 - in current gate layer
// # 3 - executed

void maintain_layer(vector<IdxType> &current_layer_gates_idx,
                    const vector<uint8_t> &execute_mask,
                    const vector<RoutedGatePair> &circuit,
                    vector<IdxType> &gate_state,
                    const vector<RoutedGatePair> &following_gate_idx,
                    vector<IdxType> &gate_dependency,
                    vector<IdxType> &updated_layer_gates_idx,
                    vector<uint8_t> &updated_layer_seen,
                    vector<IdxType> &future_layer_gates_idx,
                    vector<uint8_t> &future_layer_active,
                    IdxType flag)
{
    updated_layer_gates_idx.clear();
    IdxType start_gate = circuit.size();
    for (IdxType gate_idx : current_layer_gates_idx)
    {
        if (execute_mask[gate_idx] != 0)
        {
            const RoutedGatePair &gate = circuit[gate_idx];
            gate_state[gate_idx] = 3;
            future_layer_active[gate_idx] = 0;

            const RoutedGatePair &following_gates = following_gate_idx[gate_idx];
            for (IdxType next_gate_idx : following_gates)
            {
                if (next_gate_idx < 0)
                {
                    continue;
                }
                gate_dependency[next_gate_idx]--;
                if (gate_dependency[next_gate_idx] == 0)
                {
                    if (updated_layer_seen[next_gate_idx] == 0)
                    {
                        updated_layer_seen[next_gate_idx] = 1;
                        updated_layer_gates_idx.push_back(next_gate_idx);
                        start_gate = min(start_gate, next_gate_idx);
                    }
                    gate_state[next_gate_idx] = 2;
                    future_layer_active[next_gate_idx] = 0;
                }
            }
        }
        else
        {
            if (updated_layer_seen[gate_idx] == 0)
            {
                updated_layer_seen[gate_idx] = 1;
                updated_layer_gates_idx.push_back(gate_idx);
                start_gate = min(start_gate, gate_idx);
            }
        }
    }
    if (!updated_layer_gates_idx.empty())
    {
        for (IdxType gate_idx = start_gate; gate_idx < start_gate + kRoutingFutureWindow && gate_idx < circuit.size(); gate_idx++)
        {
            if (gate_state[gate_idx] == 0)
            {
                gate_state[gate_idx] = 1;
                if (flag != 0 && future_layer_active[gate_idx] == 0)
                {
                    future_layer_gates_idx.push_back(gate_idx);
                    future_layer_active[gate_idx] = 1;
                }
            }
        }
    }
    if (flag == 0)
    {
        for (IdxType gate_idx = 0; gate_idx < circuit.size(); gate_idx++)
        {
            if (gate_state[gate_idx] == 1)
            {
                future_layer_gates_idx.push_back(gate_idx);
                future_layer_active[gate_idx] = 1;
            }
        }
    }
    if (future_layer_gates_idx.size() > static_cast<size_t>(kRoutingFutureCompactThreshold))
    {
        auto out_it = future_layer_gates_idx.begin();
        for (IdxType gate_idx : future_layer_gates_idx)
        {
            if (future_layer_active[static_cast<size_t>(gate_idx)] != 0)
            {
                *out_it++ = gate_idx;
            }
        }
        future_layer_gates_idx.erase(out_it, future_layer_gates_idx.end());
    }
    sort(updated_layer_gates_idx.begin(), updated_layer_gates_idx.end());
    for (IdxType gate_idx : updated_layer_gates_idx)
    {
        updated_layer_seen[gate_idx] = 0;
    }
}

void find_executable_gates(const vector<IdxType> &mapping,
                           const vector<IdxType> &current_layer,
                           const vector<RoutedGatePair> &circuit,
                           const vector<vector<IdxType>> &distance_mat,
                           vector<IdxType> &executable_gates,
                           vector<uint8_t> &execute_mask);

inline bool swap_on_shortest_path(const RoutedGatePair &swap_pair,
                                  const vector<IdxType> &mapping,
                                  const vector<IdxType> &current_layer_gates_idx,
                                  const vector<vector<IdxType>> &distance_mat,
                                  const vector<RoutedGatePair> &circuit)
{
    for (IdxType gate_idx : current_layer_gates_idx)
    {
        const RoutedGatePair &gate = circuit[static_cast<size_t>(gate_idx)];
        IdxType mapped_a = mapping[static_cast<size_t>(gate[0])];
        IdxType mapped_b = mapping[static_cast<size_t>(gate[1])];
        IdxType gate_distance = distance_mat[static_cast<size_t>(mapped_a)][static_cast<size_t>(mapped_b)];
        if (gate_distance <= 1)
        {
            continue;
        }
        IdxType path_via_forward = distance_mat[static_cast<size_t>(mapped_a)][static_cast<size_t>(swap_pair[0])] +
                                   1 +
                                   distance_mat[static_cast<size_t>(swap_pair[1])][static_cast<size_t>(mapped_b)];
        if (path_via_forward == gate_distance)
        {
            return true;
        }
        IdxType path_via_reverse = distance_mat[static_cast<size_t>(mapped_a)][static_cast<size_t>(swap_pair[1])] +
                                   1 +
                                   distance_mat[static_cast<size_t>(swap_pair[0])][static_cast<size_t>(mapped_b)];
        if (path_via_reverse == gate_distance)
        {
            return true;
        }
    }
    return false;
}

inline RoutingCandidateScore score_after_swap(const vector<IdxType> &mapping,
                                              const vector<IdxType> &anchor_mapping,
                                              const vector<IdxType> &current_layer_gates_idx,
                                              const vector<IdxType> &future_gates_idx,
                                              const vector<vector<IdxType>> &distance_mat,
                                              const vector<RoutedGatePair> &circuit,
                                              IdxType logical_a,
                                              IdxType physical_a,
                                              IdxType logical_b,
                                              IdxType physical_b)
{
    RoutingCandidateScore score;
    auto mapped_qubit = [&](IdxType logical) -> IdxType
    {
        if (logical == logical_a)
        {
            return physical_b;
        }
        if (logical == logical_b)
        {
            return physical_a;
        }
        return mapping[static_cast<size_t>(logical)];
    };

    if (current_layer_gates_idx.empty())
    {
        score.max_current_distance = 0;
        score.anchor_avg = 0.0;
        score.current_avg = 0.0;
        score.future_avg = 0.0;
        score.scalar_cost = 0.0;
        return score;
    }

    double current_total = 0.0;
    double anchor_total = 0.0;
    IdxType anchor_terms = 0;
    score.max_current_distance = 0;
    for (IdxType gate_idx : current_layer_gates_idx)
    {
        const RoutedGatePair &gate = circuit[static_cast<size_t>(gate_idx)];
        IdxType mapped_a = mapped_qubit(gate[0]);
        IdxType mapped_b = mapped_qubit(gate[1]);
        IdxType distance = distance_mat[static_cast<size_t>(mapped_a)][static_cast<size_t>(mapped_b)];
        if (distance == 1)
        {
            score.executable_now += 1;
        }
        score.max_current_distance = std::max(score.max_current_distance, distance);
        current_total += static_cast<double>(distance);
        anchor_total += std::abs(mapped_a - anchor_mapping[static_cast<size_t>(gate[0])]);
        anchor_total += std::abs(mapped_b - anchor_mapping[static_cast<size_t>(gate[1])]);
        anchor_terms += 2;
    }
    score.anchor_avg = anchor_terms > 0 ? anchor_total / static_cast<double>(anchor_terms) : 0.0;
    score.current_avg = current_total / static_cast<double>(current_layer_gates_idx.size());

    if (future_gates_idx.empty())
    {
        score.future_avg = 0.0;
    }
    else
    {
        double future_total = 0.0;
        for (IdxType gate_idx : future_gates_idx)
        {
            const RoutedGatePair &gate = circuit[static_cast<size_t>(gate_idx)];
            future_total += distance_mat[static_cast<size_t>(mapped_qubit(gate[0]))]
                                        [static_cast<size_t>(mapped_qubit(gate[1]))];
        }
        score.future_avg = future_total / static_cast<double>(future_gates_idx.size());
    }

    score.scalar_cost = score.current_avg + 0.5 * score.future_avg;
    return score;
}

inline bool better_candidate_score(const RoutingCandidateScore &lhs,
                                   const RoutingCandidateScore &rhs,
                                   RoutingMode routing_mode)
{
    constexpr double kScoreEpsilon = 1e-9;
    if (routing_mode == RoutingMode::ExecWindow)
    {
        if (lhs.executable_now != rhs.executable_now)
        {
            return lhs.executable_now > rhs.executable_now;
        }
        if (lhs.max_current_distance != rhs.max_current_distance)
        {
            return lhs.max_current_distance < rhs.max_current_distance;
        }
        if (std::fabs(lhs.anchor_avg - rhs.anchor_avg) > kScoreEpsilon)
        {
            return lhs.anchor_avg < rhs.anchor_avg;
        }
        if (std::fabs(lhs.current_avg - rhs.current_avg) > kScoreEpsilon)
        {
            return lhs.current_avg < rhs.current_avg;
        }
        if (std::fabs(lhs.future_avg - rhs.future_avg) > kScoreEpsilon)
        {
            return lhs.future_avg < rhs.future_avg;
        }
    }
    return lhs.scalar_cost + kScoreEpsilon < rhs.scalar_cost;
}

struct RoutingPickWorkspace
{
    vector<RoutedGatePair> possible_pairs;
    unordered_set<uint64_t> seen_pairs;
    vector<uint32_t> seen_pair_stamp;
    uint32_t current_stamp = 0;
    vector<IdxType> active_future_layer;
};

RoutedGatePair pick_one_movement(vector<IdxType> &mapping,
                                 const vector<IdxType> &anchor_mapping,
                                 vector<IdxType> &physical_to_logical,
                                 const vector<IdxType> &current_layer,
                                 const vector<IdxType> &future_layer,
                                 const vector<uint8_t> &future_layer_active,
                                 const vector<vector<IdxType>> &distance_mat,
                                 IdxType physical_qubit_num,
                                 const vector<RoutedGatePair> &circuit,
                                 shared_ptr<Chip> chip,
                                 RoutingPickWorkspace &workspace,
                                 RoutingMode routing_mode,
                                 const vector<RoutedGatePair> *tabu_pairs = nullptr)
{
    vector<RoutedGatePair> &possible_pairs = workspace.possible_pairs;
    unordered_set<uint64_t> &seen_pairs = workspace.seen_pairs;
    possible_pairs.clear();
    possible_pairs.reserve(current_layer.size() * 6);
    const bool use_dense_pair_stamps = !workspace.seen_pair_stamp.empty();
    if (use_dense_pair_stamps)
    {
        workspace.current_stamp += 1;
        if (workspace.current_stamp == 0)
        {
            std::fill(workspace.seen_pair_stamp.begin(), workspace.seen_pair_stamp.end(), 0);
            workspace.current_stamp = 1;
        }
    }
    else
    {
        seen_pairs.clear();
        seen_pairs.reserve(current_layer.size() * 8 + 8);
    }
    for (IdxType gate_idx : current_layer)
    {
        const RoutedGatePair &gate = circuit[gate_idx];
        for (IdxType logical_endpoint : gate)
        {
            IdxType p_qubit = mapping[static_cast<size_t>(logical_endpoint)];
            for (IdxType p_qubit_target : chip->edge_list[p_qubit])
            {
                IdxType lhs = min(p_qubit, p_qubit_target);
                IdxType rhs = max(p_qubit, p_qubit_target);
                if (use_dense_pair_stamps)
                {
                    const size_t pair_key = static_cast<size_t>(lhs) * static_cast<size_t>(physical_qubit_num) +
                                            static_cast<size_t>(rhs);
                    if (workspace.seen_pair_stamp[pair_key] == workspace.current_stamp)
                    {
                        continue;
                    }
                    workspace.seen_pair_stamp[pair_key] = workspace.current_stamp;
                }
                else
                {
                    uint64_t pair_key = (static_cast<uint64_t>(static_cast<uint32_t>(lhs)) << 32) |
                                        static_cast<uint32_t>(rhs);
                    if (!seen_pairs.insert(pair_key).second)
                    {
                        continue;
                    }
                }
                possible_pairs.push_back({p_qubit, p_qubit_target});
            }
        }
    }

    vector<IdxType> &active_future_layer = workspace.active_future_layer;
    active_future_layer.clear();
    active_future_layer.reserve(kRoutingFutureWindow);
    for (IdxType gate_idx : future_layer)
    {
        if (future_layer_active[static_cast<size_t>(gate_idx)] != 0)
        {
            active_future_layer.push_back(gate_idx);
        }
    }

    RoutingCandidateScore best_score;
    RoutingCandidateScore fallback_score;
    size_t best_move_idx = 0;
    size_t fallback_move_idx = 0;
    bool found_non_tabu_move = false;
    bool found_any_move = false;
    bool have_on_path_candidates = false;
    vector<uint8_t> on_path_candidate_mask(possible_pairs.size(), 0);
    if (routing_mode == RoutingMode::ExecWindow)
    {
        for (size_t pair_idx = 0; pair_idx < possible_pairs.size(); ++pair_idx)
        {
            if (swap_on_shortest_path(possible_pairs[pair_idx], mapping, current_layer, distance_mat, circuit))
            {
                on_path_candidate_mask[pair_idx] = 1;
                have_on_path_candidates = true;
            }
        }
    }
    for (size_t pair_idx = 0; pair_idx < possible_pairs.size(); ++pair_idx)
    {
        if (have_on_path_candidates && on_path_candidate_mask[pair_idx] == 0)
        {
            continue;
        }
        const RoutedGatePair &pair = possible_pairs[pair_idx];
        IdxType logical_a = physical_to_logical[static_cast<size_t>(pair[0])];
        IdxType logical_b = physical_to_logical[static_cast<size_t>(pair[1])];
        RoutingCandidateScore candidate_score = score_after_swap(mapping,
                                                                anchor_mapping,
                                                                current_layer,
                                                                active_future_layer,
                                                                distance_mat,
                                                                circuit,
                                                                logical_a,
                                                                pair[0],
                                                                logical_b,
                                                                pair[1]);
        if (!found_any_move || better_candidate_score(candidate_score, fallback_score, routing_mode))
        {
            fallback_score = candidate_score;
            fallback_move_idx = pair_idx;
            found_any_move = true;
        }
        bool is_tabu = false;
        if (tabu_pairs != nullptr)
        {
            for (const RoutedGatePair &tabu_pair : *tabu_pairs)
            {
                if (same_swap_edge(pair, tabu_pair))
                {
                    is_tabu = true;
                    break;
                }
            }
        }
        if (!is_tabu && (!found_non_tabu_move || better_candidate_score(candidate_score, best_score, routing_mode)))
        {
            best_score = candidate_score;
            best_move_idx = pair_idx;
            found_non_tabu_move = true;
        }
    }
    if (!found_non_tabu_move)
    {
        best_score = fallback_score;
        best_move_idx = fallback_move_idx;
    }

    RoutedGatePair pair = possible_pairs[best_move_idx];
    IdxType logical_a = physical_to_logical[static_cast<size_t>(pair[0])];
    IdxType logical_b = physical_to_logical[static_cast<size_t>(pair[1])];
    if (logical_a >= 0)
    {
        mapping[logical_a] = pair[1];
    }
    if (logical_b >= 0)
    {
        mapping[logical_b] = pair[0];
    }
    physical_to_logical[static_cast<size_t>(pair[0])] = logical_b;
    physical_to_logical[static_cast<size_t>(pair[1])] = logical_a;
    return pair;
}

void find_executable_gates(const vector<IdxType> &mapping,
                           const vector<IdxType> &current_layer,
                           const vector<RoutedGatePair> &circuit,
                           const vector<vector<IdxType>> &distance_mat,
                           vector<IdxType> &executable_gates,
                           vector<uint8_t> &execute_mask)
{
    executable_gates.clear();
    for (IdxType gate_idx : current_layer)
    {
        IdxType mapped_gate_zero = mapping[circuit[gate_idx][0]];
        IdxType mapped_gate_one = mapping[circuit[gate_idx][1]];
        if (distance_mat[mapped_gate_zero][mapped_gate_one] == 1)
        {
            executable_gates.push_back(gate_idx);
            execute_mask[gate_idx] = 1;
        }
    }
}

IdxType one_round_optimization(vector<IdxType> &initial_mapping,
                               const vector<Gate> &circuit_gate,
                               const vector<vector<IdxType>> &distance_mat,
                               const vector<Gate> &gate_info,
                               shared_ptr<Chip> chip,
                               vector<Gate> &return_circuit,
                               bool materialize_circuit,
                               RoutingMode routing_mode)
{
    IdxType swap_num = 0;
    vector<IdxType> mapping = initial_mapping;
    if (materialize_circuit)
    {
        return_circuit.clear();
        return_circuit.reserve(gate_info.size() + circuit_gate.size() / 4 + 16);
    }

    IdxType executed_gates_num = 0;
    IdxType gate_num = circuit_gate.size();
    vector<RoutedGatePair> circuit(static_cast<size_t>(gate_num));
    for (IdxType i = 0; i < gate_num; i++)
    {
        circuit[static_cast<size_t>(i)] = RoutedGatePair{circuit_gate[i].ctrl, circuit_gate[i].qubit};
    }
    IdxType logical_qubit_num = static_cast<IdxType>(mapping.size());
    IdxType physical_qubit_num = static_cast<IdxType>(distance_mat.size());
    vector<IdxType> physical_to_logical(static_cast<size_t>(physical_qubit_num), -1);
    for (IdxType logical = 0; logical < logical_qubit_num; ++logical)
    {
        IdxType physical = mapping[static_cast<size_t>(logical)];
        if (physical >= 0 && physical < physical_qubit_num)
        {
            physical_to_logical[static_cast<size_t>(physical)] = logical;
        }
    }
    vector<IdxType> gate_state(gate_num, 0);
    vector<IdxType> gate_dependency(gate_num, 2);
    vector<RoutedGatePair> following_gates_idx;
    vector<IdxType> first_layer_gates_idx;
    DAG_generator(logical_qubit_num, circuit, gate_state, gate_dependency, following_gates_idx, first_layer_gates_idx);

    vector<IdxType> current_layer = first_layer_gates_idx;
    vector<IdxType> future_layer;
    vector<uint8_t> execute_mask(gate_num, 0);
    vector<uint8_t> future_layer_active(gate_num, 0);
    vector<IdxType> executable_gates;
    executable_gates.reserve(current_layer.size() + 8);
    vector<IdxType> updated_layer_gates_idx;
    vector<uint8_t> updated_layer_seen(gate_num, 0);
    current_layer.reserve(first_layer_gates_idx.size() + 8);
    updated_layer_gates_idx.reserve(first_layer_gates_idx.size() + 8);
    future_layer.reserve(32);
    maintain_layer(current_layer, execute_mask, circuit, gate_state, following_gates_idx, gate_dependency, updated_layer_gates_idx, updated_layer_seen, future_layer, future_layer_active, 0);
    current_layer = updated_layer_gates_idx;
    vector<vector<IdxType>> dependency_vector;
    vector<uint8_t> visited_gate;
    if (materialize_circuit)
    {
        vector<vector<IdxType>> qubit_to_gate_indices(static_cast<size_t>(logical_qubit_num));
        dependency_vector.resize(static_cast<size_t>(gate_num));

        IdxType two_qubit_gate_index = 0;

        for (IdxType i = 0; i < gate_info.size(); ++i)
        {
            const auto &gate = gate_info[i];
            if (gate.ctrl != -1)
            {
                auto &ctrl_pending = qubit_to_gate_indices[static_cast<size_t>(gate.ctrl)];
                if (!ctrl_pending.empty())
                {
                    for (const auto &idx : ctrl_pending)
                    {
                        dependency_vector[static_cast<size_t>(two_qubit_gate_index)].push_back(idx);
                    }
                    ctrl_pending.clear();
                }
                auto &target_pending = qubit_to_gate_indices[static_cast<size_t>(gate.qubit)];
                if (!target_pending.empty())
                {
                    for (const auto &idx : target_pending)
                    {
                        dependency_vector[static_cast<size_t>(two_qubit_gate_index)].push_back(idx);
                    }
                    target_pending.clear();
                }
                two_qubit_gate_index++;
            }
            else if (strcmp(OP_NAMES[gate_info[i].op_name], "MA") != 0)
            {
                qubit_to_gate_indices[static_cast<size_t>(gate.qubit)].push_back(i);
            }
        }
        visited_gate.assign(gate_info.size(), 0);
    }

    RoutingPickWorkspace pick_workspace;
    pick_workspace.possible_pairs.reserve(64);
    pick_workspace.seen_pairs.reserve(128);
    constexpr IdxType kDensePairStampMaxQubits = 4096;
    if (physical_qubit_num > 0 && physical_qubit_num <= kDensePairStampMaxQubits)
    {
        pick_workspace.seen_pair_stamp.assign(static_cast<size_t>(physical_qubit_num) *
                                                  static_cast<size_t>(physical_qubit_num),
                                              0);
    }
    pick_workspace.active_future_layer.reserve(kRoutingFutureWindow);
    IdxType consecutive_swap_only_steps = 0;
    RoutedGatePair last_swap_pair{-1, -1};
    std::deque<RoutedGatePair> recent_no_progress_swaps;
    constexpr size_t kRoutingNoProgressTabuLimit = 4;
    while (executed_gates_num < gate_num)
    {
        find_executable_gates(mapping, current_layer, circuit, distance_mat, executable_gates, execute_mask);
        if (materialize_circuit)
        {
            for (IdxType ee : executable_gates)
            {
                const vector<IdxType> &cur_index_vector = dependency_vector[static_cast<size_t>(ee)];
                for (IdxType cur_index : cur_index_vector)
                {
                    Gate cur_gate = gate_info[static_cast<size_t>(cur_index)];
                    IdxType q_qubit = mapping[static_cast<size_t>(cur_gate.qubit)];
                    cur_gate.qubit = q_qubit;
                    return_circuit.push_back(cur_gate);
                    visited_gate[static_cast<size_t>(cur_index)] = 1;
                }
                Gate cur_gate = circuit_gate[static_cast<size_t>(ee)];
                IdxType q_qubit = mapping[static_cast<size_t>(cur_gate.qubit)];
                IdxType c_qubit = mapping[static_cast<size_t>(cur_gate.ctrl)];
                cur_gate.qubit = q_qubit;
                cur_gate.ctrl = c_qubit;
                return_circuit.push_back(cur_gate);
            }
        }
        if (!executable_gates.empty())
        {
            maintain_layer(current_layer, execute_mask, circuit, gate_state, following_gates_idx, gate_dependency, updated_layer_gates_idx, updated_layer_seen, future_layer, future_layer_active, 1);

            current_layer = updated_layer_gates_idx;
            executed_gates_num += executable_gates.size();
            consecutive_swap_only_steps = 0;
            last_swap_pair = RoutedGatePair{-1, -1};
            recent_no_progress_swaps.clear();
            for (IdxType gate_idx : executable_gates)
            {
                execute_mask[gate_idx] = 0;
            }
        }
        else
        {
            vector<RoutedGatePair> tabu_pairs;
            tabu_pairs.reserve(kRoutingNoProgressTabuLimit);
            if (last_swap_pair[0] >= 0)
            {
                tabu_pairs.push_back(last_swap_pair);
            }
            if (consecutive_swap_only_steps >= 4)
            {
                for (auto it = recent_no_progress_swaps.rbegin();
                     it != recent_no_progress_swaps.rend() &&
                     tabu_pairs.size() < kRoutingNoProgressTabuLimit;
                     ++it)
                {
                    bool already_present = false;
                    for (const RoutedGatePair &tabu_pair : tabu_pairs)
                    {
                        if (same_swap_edge(tabu_pair, *it))
                        {
                            already_present = true;
                            break;
                        }
                    }
                    if (!already_present)
                    {
                        tabu_pairs.push_back(*it);
                    }
                }
            }
            RoutedGatePair pair = pick_one_movement(mapping,
                                                    initial_mapping,
                                                    physical_to_logical,
                                                    current_layer,
                                                    future_layer,
                                                    future_layer_active,
                                                    distance_mat,
                                                    physical_qubit_num,
                                                    circuit,
                                                    chip,
                                                    pick_workspace,
                                                    routing_mode,
                                                    tabu_pairs.empty() ? nullptr : &tabu_pairs);
            if (materialize_circuit)
            {
                Gate SWAPG = Gate(OP::SWAP, IdxType(pair[1]), IdxType(pair[0]));
                return_circuit.push_back(SWAPG);
            }
            swap_num += 1;
            consecutive_swap_only_steps += 1;
            RoutedGatePair normalized_pair = canonical_swap_pair(pair);
            last_swap_pair = normalized_pair;
            recent_no_progress_swaps.push_back(normalized_pair);
            if (recent_no_progress_swaps.size() > kRoutingNoProgressTabuLimit)
            {
                recent_no_progress_swaps.pop_front();
            }
        }
    }
    if (materialize_circuit)
    {
        for (IdxType i = 0; i < gate_info.size(); i++)
        {
            if (gate_info[static_cast<size_t>(i)].ctrl == -1 &&
                strcmp(OP_NAMES[gate_info[static_cast<size_t>(i)].op_name], "MA") != 0 &&
                visited_gate[static_cast<size_t>(i)] == 0)
            {
                Gate cur_gate = gate_info[static_cast<size_t>(i)];
                IdxType q_qubit = mapping[static_cast<size_t>(cur_gate.qubit)];
                cur_gate.qubit = q_qubit;
                return_circuit.push_back(cur_gate);
            }
        }
    }
    initial_mapping = mapping;
    return swap_num;
}

void Routing(shared_ptr<Circuit> circuit, shared_ptr<Chip> chip,
             IdxType debug_level,
             RoutingMode routing_mode)
{
    IdxType n_qubits = IdxType(circuit->num_qubits());
    IdxType physical_qubit_num = static_cast<IdxType>(chip->distance_mat.size());
    const vector<Gate> &gate_info = circuit->gate_list();

    vector<Gate> cx_gates;
    cx_gates.reserve(gate_info.size());
    for (IdxType i = 0; i < gate_info.size(); i++)
    {
        const Gate &gate = gate_info[i];
        if (gate.ctrl != -1 && strcmp(OP_NAMES[gate.op_name], "MA") != 0)
        {
            cx_gates.push_back(gate);
        }
    }

    const vector<IdxType> &requested_initial_mapping = circuit->mapping_view();
    bool has_explicit_initial_mapping = requested_initial_mapping.size() == static_cast<size_t>(n_qubits);
    if (has_explicit_initial_mapping)
    {
        std::vector<IdxType> seen(static_cast<size_t>(physical_qubit_num), 0);
        for (IdxType physical : requested_initial_mapping)
        {
            if (physical < 0 || physical >= physical_qubit_num || seen[static_cast<size_t>(physical)] != 0)
            {
                has_explicit_initial_mapping = false;
                break;
            }
            seen[static_cast<size_t>(physical)] = 1;
        }
    }

    vector<IdxType> initial_mapping = has_explicit_initial_mapping
                                          ? requested_initial_mapping
                                          : build_contiguous_initial_mapping(chip, n_qubits);
    vector<IdxType> start_mapping = initial_mapping;
    vector<Gate> return_circuit;
    IdxType swap_num = 0;

    if (has_explicit_initial_mapping)
    {
        if (debug_level > 1)
            cout << "******* fixed-layout routing *******" << endl;
        swap_num = one_round_optimization(initial_mapping, cx_gates, chip->distance_mat, gate_info, chip, return_circuit, true, routing_mode);
    }
    else
    {
        if (debug_level > 1)
            cout << "******* 1st round sabre optimization *******" << endl;
        swap_num = one_round_optimization(initial_mapping, cx_gates, chip->distance_mat, gate_info, chip, return_circuit, false, routing_mode);

        if (debug_level > 1)
            cout << "******* 2nd round sabre optimization *******" << endl;
        reverse(cx_gates.begin(), cx_gates.end());
        return_circuit.clear();
        swap_num = one_round_optimization(initial_mapping, cx_gates, chip->distance_mat, gate_info, chip, return_circuit, false, routing_mode);

        if (debug_level > 1)
            cout << "******* 3rd round sabre optimization *******" << endl;
        return_circuit.clear();
        reverse(cx_gates.begin(), cx_gates.end());
        if (debug_level > 1)
        {
            cout << "initial mapping is:";
            for (IdxType ini : initial_mapping)
            {
                cout << ini << " ";
            }
            cout << endl;
        }
        start_mapping = initial_mapping;
        swap_num = one_round_optimization(initial_mapping, cx_gates, chip->distance_mat, gate_info, chip, return_circuit, true, routing_mode);
    }

    circuit->clear_critical_path();
    circuit->set_mapping(std::move(initial_mapping));
    circuit->set_routed_initial_mapping(std::move(start_mapping));
    circuit->set_routing_swap_count(swap_num);
    circuit->set_gates(std::move(return_circuit));
}
