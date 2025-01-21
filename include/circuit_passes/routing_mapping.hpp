#pragma once

#include <string>
#include <random>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <stdexcept>
#include <iterator>
#include <functional>

#include "../QASMTransPrimitives.hpp"  // For OP_NAMES, OP enum, etc.
#include "../IR/gate.hpp"
#include "../IR/circuit.hpp"
#include "../IR/chip.hpp"
#include "../IR/graph.hpp"

#include "../nlomann/json.hpp"         // For nlohmann::json
// #include "cpu_timer.hpp"            // For cpu_timer if you have a custom timer class

namespace QASMTrans {

using json = nlohmann::json;

/**
 * @brief Extracts "cx__" gate pairs from a JSON object for building a coupling graph.
 *
 * The keys are expected to look like "cx0_1", "cx1_0", etc.
 *
 * @param j The JSON object containing gate definitions.
 * @return A vector of (control, target) pairs found under keys that start with "cx".
 */
inline std::vector<std::pair<IdxType, IdxType>> extract_cx_pairs(const json &j)
{
    std::vector<std::pair<IdxType, IdxType>> pairs;
    pairs.reserve(j.size());

    for (auto &item : j.items())
    {
        const std::string &key = item.key();
        if (key.rfind("cx", 0) == 0) // starts with "cx"
        {
            // Expected format: "cxX_Y"
            IdxType pos = key.find('_');
            if (pos != std::string::npos)
            {
                IdxType first  = std::stoi(key.substr(2, pos - 2));
                IdxType second = std::stoi(key.substr(pos + 1));
                pairs.emplace_back(std::make_pair(first, second));
            }
        }
    }
    return pairs;
}

/**
 * @brief Builds a DAG representation (dependency graph) of the 2-qubit gates in the circuit.
 *
 * Each 2-qubit gate is stored in `circuit[g] = {ctrl, qubit}`.
 * This function sets up:
 *  - gate_dependency[g]: How many gates must complete before gate g can run
 *  - following_gate_idx[g]: Which gate(s) follow gate g on each qubit
 *  - gate_state[g]: 0 (not considered), 1 (in future queue), 2 (in current layer), 3 (executed)
 *  - qubit_state[q]: 0 (free), 1 (occupied)
 *  - first_layer_gates_idx: The gates that have no dependencies
 *
 * @param qubit_num            Number of qubits
 * @param circuit              [in]  2D list of gates, each gate is {ctrl, qubit}
 * @param gate_state           [out] Gate states for each gate
 * @param qubit_state          [out] Occupancy state of each qubit
 * @param gate_dependency      [out] Number of dependencies for each gate
 * @param following_gate_idx   [out] For each gate, index of the next gate using the same qubit
 * @param first_layer_gates_idx[out] Gates that can be placed in the first layer
 */
inline void DAG_generator(
    IdxType qubit_num,
    std::vector<std::vector<IdxType>> &circuit,
    std::vector<IdxType> &gate_state,
    std::vector<IdxType> &qubit_state,
    std::vector<IdxType> &gate_dependency,
    std::vector<std::vector<IdxType>> &following_gate_idx,
    std::vector<IdxType> &first_layer_gates_idx)
{
    IdxType gate_num = circuit.size();
    gate_dependency.resize(gate_num, 0);
    gate_state.resize(gate_num, 0);
    following_gate_idx.resize(gate_num, std::vector<IdxType>(2, 0));

    // current_gate_idx[q] holds the index of the last gate that used qubit q
    std::vector<IdxType> current_gate_idx(qubit_num, IdxType(-1));

    for (IdxType i = 0; i < gate_num; i++)
    {
        // gate = {ctrl, qubit}
        const auto &gate = circuit[i];
        // Check dependencies
        bool free_ctrl  = (current_gate_idx[gate[0]] == IdxType(-1));
        bool free_tgt   = (current_gate_idx[gate[1]] == IdxType(-1));
        if (free_ctrl && free_tgt)
        {
            first_layer_gates_idx.push_back(i);
            gate_state[i] = 2;  // in current layer
            qubit_state[gate[0]] = 1;
            qubit_state[gate[1]] = 1;
            gate_dependency[i] = 0;
        }
        else
        {
            // If either qubit was in use, we have a dependency
            gate_dependency[i] = 1;
        }

        // If only one qubit was free, it’s also a dependency
        if (free_ctrl != free_tgt)
        {
            gate_dependency[i] = 1;
        }

        // Update following_gate_idx
        for (IdxType j = 0; j < (IdxType)gate.size(); j++)
        {
            IdxType q = gate[j];
            if (current_gate_idx[q] != IdxType(-1))
            {
                // The prior gate that used q
                IdxType prior_gate_idx = current_gate_idx[q];
                // Mark the next gate on that qubit
                following_gate_idx[prior_gate_idx][j] = i;
            }
            // Now this gate is the last to use qubit q
            current_gate_idx[q] = i;
        }
    }
}

/**
 * @brief Gate states:
 *  - 0 = not considered
 *  - 1 = in future gate queue
 *  - 2 = in current layer
 *  - 3 = executed
 * 
 * Qubit states:
 *  - 0 = not occupied in current layer
 *  - 1 = occupied in current layer
 */

/**
 * @brief Maintains and updates the current layer of gates after execution.
 * 
 * @param current_layer_gates_idx  [in/out] Indices of gates in the current layer
 * @param gate_execute_idx_list    [in]     Gates that have been executed in this step
 * @param circuit                  [in]     The 2D list of gates (ctrl, qubit)
 * @param gate_state               [in/out] State of each gate
 * @param following_gate_idx       [in/out] For each gate, next gates that depend on it
 * @param qubit_state              [in/out] Whether each qubit is free (0) or in use (1)
 * @param gate_dependency          [in/out] Dependencies remaining for each gate
 * @param updated_layer_gates_idx  [out]    The newly updated list of gates in the current layer
 * @param future_layer_gates_idx   [in/out] The queue of gates for future layers
 * @param initialize_future        [in]     0 = fill future from all gates in state=1; 
 *                                          1 = partial fill (up to 20 from start_gate)
 */
inline void maintain_layer(
    std::vector<IdxType> &current_layer_gates_idx,
    const std::set<IdxType> &gate_execute_idx_list,
    const std::vector<std::vector<IdxType>> &circuit,
    std::vector<IdxType> &gate_state,
    std::vector<std::vector<IdxType>> &following_gate_idx,
    std::vector<IdxType> &qubit_state,
    std::vector<IdxType> &gate_dependency,
    std::vector<IdxType> &updated_layer_gates_idx,
    std::vector<IdxType> &future_layer_gates_idx,
    IdxType initialize_future)
{
    std::unordered_set<IdxType> new_current_set;
    new_current_set.reserve(current_layer_gates_idx.size());

    // 1) Process gates that were executed
    for (IdxType gate_idx : current_layer_gates_idx)
    {
        if (gate_execute_idx_list.count(gate_idx))
        {
            gate_state[gate_idx] = 3; // executed

            // Free up qubits
            const auto &gate = circuit[gate_idx];
            for (IdxType q : gate)
            {
                qubit_state[q] = 0; 
            }

            // Decrement dependency for any following gates
            const auto &next_gates = following_gate_idx[gate_idx];
            for (IdxType ng : next_gates)
            {
                if (ng < gate_dependency.size() && gate_dependency[ng] > 0)
                {
                    gate_dependency[ng]--;
                    if (gate_dependency[ng] == 0)
                    {
                        // This gate is now ready
                        new_current_set.insert(ng);
                        gate_state[ng] = 2;
                        // Remove it from future
                        future_layer_gates_idx.erase(
                            std::remove(future_layer_gates_idx.begin(),
                                        future_layer_gates_idx.end(), ng),
                            future_layer_gates_idx.end()
                        );
                        // Occupy qubits for this gate
                        for (IdxType q : circuit[ng])
                        {
                            qubit_state[q] = 1;
                        }
                    }
                }
            }
        }
        else
        {
            // if gate was not executed, keep it in the current layer
            new_current_set.insert(gate_idx);
        }
    }

    // 2) Update the current layer from the new_current_set
    updated_layer_gates_idx.assign(new_current_set.begin(), new_current_set.end());

    // 3) Possibly re-initialize the future layer
    if (initialize_future == 0)
    {
        // Add all gates in state=1
        for (IdxType i = 0; i < (IdxType)gate_state.size(); i++)
        {
            if (gate_state[i] == 1)
            {
                future_layer_gates_idx.push_back(i);
            }
        }
    }
    else
    {
        // Add up to 20 gates from start_gate
        if (!updated_layer_gates_idx.empty())
        {
            IdxType start_gate = *std::min_element(updated_layer_gates_idx.begin(),
                                                   updated_layer_gates_idx.end());
            for (IdxType g = start_gate; g < start_gate + 20 && g < (IdxType)gate_state.size(); g++)
            {
                if (gate_state[g] == 0)
                {
                    gate_state[g] = 1;
                    future_layer_gates_idx.push_back(g);
                }
            }
        }
    }

    // 4) Sort for consistency
    std::sort(updated_layer_gates_idx.begin(), updated_layer_gates_idx.end());
    std::sort(future_layer_gates_idx.begin(), future_layer_gates_idx.end());
}

/**
 * @brief A cost function (heuristic) to measure how "expensive" the current and future gates are
 * given a particular logical->physical qubit mapping.
 * 
 * The cost is: (avg distance of current layer) + 0.5*(avg distance of future layer)
 *
 * @param new_mapping    The logical->physical mapping
 * @param current_layer  Indices of gates in the current layer
 * @param future_layer   Indices of gates in the future layer
 * @param distance_mat   A matrix of distances [p_qubit][p_qubit]
 * @param circuit        The 2D list of gates {ctrl, qubit} by index
 * @return A cost value (lower is better)
 */
inline double heuristic(
    const std::vector<IdxType> &new_mapping,
    const std::vector<IdxType> &current_layer,
    const std::vector<IdxType> &future_layer,
    const std::vector<std::vector<IdxType>> &distance_mat,
    const std::vector<std::vector<IdxType>> &circuit)
{
    // If there are no current-layer gates, cost is zero
    if (current_layer.empty())
        return 0.0;

    // 1) Cost of current layer
    double cost_curr = 0.0;
    for (IdxType gate_idx : current_layer)
    {
        const auto &gate = circuit[gate_idx];
        cost_curr += distance_mat[new_mapping[gate[0]]][new_mapping[gate[1]]];
    }
    cost_curr /= current_layer.size();

    // 2) If no future gates, total cost is just current
    if (future_layer.empty())
        return cost_curr;

    // 3) Cost of future layer
    double cost_fut = 0.0;
    for (IdxType gate_idx : future_layer)
    {
        const auto &gate = circuit[gate_idx];
        cost_fut += distance_mat[new_mapping[gate[0]]][new_mapping[gate[1]]];
    }
    cost_fut /= future_layer.size();

    // Weighted sum
    return cost_curr + 0.5 * cost_fut;
}

/**
 * @brief Finds the reverse mapping p2l (physical->logical) given a forward mapping l2p (logical->physical).
 *
 * @param mapping    The forward mapping of size qubit_num
 * @param qubit_num  The total number of qubits
 * @return A reverse mapping (physical->logical) vector
 */
inline std::vector<IdxType> find_reverse_mapping(const std::vector<IdxType> &mapping, IdxType qubit_num)
{
    std::vector<IdxType> reverse_mapping(qubit_num, -1);
    for (IdxType l_qubit = 0; l_qubit < (IdxType)mapping.size(); ++l_qubit)
    {
        IdxType p_qubit = mapping[l_qubit];
        if (p_qubit >= 0 && p_qubit < qubit_num)
        {
            reverse_mapping[p_qubit] = l_qubit;
        }
        else
        {
            // Possibly handle error
            // std::cerr << "Warning: invalid qubit index: " << p_qubit << "\n";
        }
    }
    return reverse_mapping;
}

/**
 * @brief Attempts a local SWAP to make a gate executable if none are currently executable.
 *
 * This picks a pair of adjacent physical qubits (neighbors in chip->edge_list) that, once swapped,
 * yields the best improvement in the heuristic cost.
 *
 * @param mapping       [in/out] Current logical->physical mapping
 * @param current_layer [in]     The current layer of gates
 * @param future_layer  [in]     The next layer of gates
 * @param distance_mat  [in]     The hardware distance matrix
 * @param qubit_num     [in]     Number of qubits
 * @param circuit       [in]     The 2D list of gates (ctrl, qubit)
 * @param chip          [in]     The hardware chip (edges, etc.)
 * @return A vector of size 2 with the physical qubits that were swapped
 */
inline std::vector<IdxType> pick_one_movement(
    std::vector<IdxType> &mapping,
    const std::vector<IdxType> &current_layer,
    const std::vector<IdxType> &future_layer,
    const std::vector<std::vector<IdxType>> &distance_mat,
    IdxType qubit_num,
    const std::vector<std::vector<IdxType>> &circuit,
    std::shared_ptr<Chip> chip)
{
    // Copy our current (l->p) mapping
    std::vector<IdxType> l2p_mapping = mapping;

    // Gather the physical qubits relevant to current-layer gates
    std::vector<IdxType> key_p_qubits;
    key_p_qubits.reserve(current_layer.size() * 2);
    for (IdxType g_idx : current_layer)
    {
        const auto &gate = circuit[g_idx];
        key_p_qubits.push_back(l2p_mapping[gate[0]]);
        key_p_qubits.push_back(l2p_mapping[gate[1]]);
    }

    // Build list of possible swap pairs
    std::vector<std::vector<IdxType>> possible_pairs;
    possible_pairs.reserve(key_p_qubits.size() * 2);
    for (IdxType p_qubit : key_p_qubits)
    {
        // Check adjacency from chip->edge_list
        for (IdxType neighbor : chip->edge_list[p_qubit])
        {
            possible_pairs.push_back({p_qubit, neighbor});
        }
    }

    double best_score = std::numeric_limits<double>::infinity();
    size_t best_move_idx = 0;
    std::vector<double> scores(possible_pairs.size());

    // Evaluate each swap pair
    for (size_t i = 0; i < possible_pairs.size(); ++i)
    {
        const auto &pair = possible_pairs[i];
        // Reverse map
        std::vector<IdxType> p2l = find_reverse_mapping(l2p_mapping, qubit_num);

        // Swap in physical domain
        std::swap(p2l[pair[0]], p2l[pair[1]]);

        // Convert back to l->p
        std::vector<IdxType> temp_l2p = find_reverse_mapping(p2l, qubit_num);

        // Evaluate cost
        double cost_val = heuristic(temp_l2p, current_layer, future_layer, distance_mat, circuit);
        scores[i] = cost_val;

        if (cost_val < best_score)
        {
            best_score = cost_val;
            best_move_idx = i;
        }
    }

    // Perform the best swap
    const auto &best_pair = possible_pairs[best_move_idx];
    // Rebuild p2l
    std::vector<IdxType> p2l_final = find_reverse_mapping(l2p_mapping, qubit_num);
    std::swap(p2l_final[best_pair[0]], p2l_final[best_pair[1]]);
    mapping = find_reverse_mapping(p2l_final, qubit_num);

    return best_pair;
}

/**
 * @brief Finds which gates in the current layer can be executed immediately, i.e., 
 * which have distance 1 in the distance_mat (meaning their assigned qubits are neighbors).
 *
 * @param mapping       [in] The l->p mapping
 * @param current_layer [in] Indices of gates in the current layer
 * @param circuit       [in] The 2D list of gates {ctrl, qubit}
 * @param distance_mat  [in] The hardware distance matrix
 * @return A set of gate indices that can be executed now
 */
inline std::set<IdxType> find_executable_gates(
    const std::vector<IdxType> &mapping,
    const std::vector<IdxType> &current_layer,
    const std::vector<std::vector<IdxType>> &circuit,
    const std::vector<std::vector<IdxType>> &distance_mat)
{
    std::set<IdxType> executable;
    for (IdxType g_idx : current_layer)
    {
        IdxType l_ctrl = circuit[g_idx][0];
        IdxType l_tgt  = circuit[g_idx][1];
        IdxType p_ctrl = mapping[l_ctrl];
        IdxType p_tgt  = mapping[l_tgt];
        // If distance is 1, they are adjacent => executable
        if (distance_mat[p_ctrl][p_tgt] == 1)
        {
            executable.insert(g_idx);
        }
    }
    return executable;
}

/**
 * @brief Sorts a vector using minimal swaps, returning the (oldVal, newVal) pairs swapped.
 * 
 * @param lst The list to sort in-place (ignoring -1 entries).
 * @return A vector of (beforeSwap, afterSwap) pairs
 */
inline std::vector<std::pair<IdxType, IdxType>> sortWithSwaps(std::vector<IdxType> &lst)
{
    // Filter out -1
    std::vector<IdxType> temp;
    for (auto &val : lst)
    {
        if (val != IdxType(-1)) temp.push_back(val);
    }

    // Sort them
    std::sort(temp.begin(), temp.end());

    // Rebuild in-place
    std::vector<std::pair<IdxType, IdxType>> swaps;
    size_t idx_temp = 0;
    for (size_t i = 0; i < lst.size(); i++)
    {
        if (lst[i] != IdxType(-1))
        {
            // If mismatch, swap
            if (lst[i] != temp[idx_temp])
            {
                // Find correct position
                auto it = std::find(lst.begin() + i, lst.end(), temp[idx_temp]);
                if (it != lst.end())
                {
                    size_t j = std::distance(lst.begin(), it);
                    std::swap(lst[i], lst[j]);
                    swaps.push_back({lst[i], lst[j]});
                }
            }
            idx_temp++;
        }
    }
    return swaps;
}

/**
 * @brief Performs a single round of SABRE-like optimization.
 *
 * @param initial_mapping [in/out] The initial l->p qubit mapping
 * @param circuit_gate    [in]     The two-qubit gates in the circuit
 * @param distance_mat    [in]     The distance matrix for the chip
 * @param gate_info       [in]     Full gate list (including single-qubit gates)
 * @param chip            [in]     The hardware chip definition
 * @param gate_qubit      [in]     Extra gate->qubit info (optional)
 * @param return_circuit  [out]    The resulting circuit after applying SWAPs and expansions
 * @param debug_level     [in]     Debug verbosity
 * @return The number of SWAPs performed
 */
inline IdxType one_round_optimization(
    std::vector<IdxType> &initial_mapping,
    const std::vector<Gate> &circuit_gate,
    const std::vector<std::vector<IdxType>> &distance_mat,
    const std::vector<Gate> &gate_info,
    std::shared_ptr<Chip> chip,
    std::vector<std::vector<IdxType>> &gate_qubit,
    std::vector<Gate> &return_circuit,
    IdxType debug_level)
{
    IdxType swap_count   = 0;
    IdxType executed_cnt = 0;
    IdxType gate_num     = (IdxType)circuit_gate.size();
    IdxType qubit_num    = (IdxType)distance_mat.size();

    // Build a circuit representation for these 2-qubit gates: circuit[i] = [ctrl, qubit]
    std::vector<std::vector<IdxType>> circuit(gate_num, std::vector<IdxType>(2, 0));
    for (IdxType i = 0; i < gate_num; i++)
    {
        circuit[i][0] = circuit_gate[i].ctrl;
        circuit[i][1] = circuit_gate[i].qubit;
    }

    // Gate states / dependencies
    std::vector<IdxType> gate_state(gate_num, 0);
    std::vector<IdxType> gate_dependency(gate_num, 2);
    std::vector<IdxType> qubit_state(qubit_num, 0);
    std::vector<std::vector<IdxType>> following_gates_idx;
    std::vector<IdxType> first_layer_gates_idx;

    // Generate DAG
    DAG_generator(qubit_num, circuit, gate_state, qubit_state,
                  gate_dependency, following_gates_idx, first_layer_gates_idx);

    // Initialize current/future layers
    std::vector<IdxType> current_layer = first_layer_gates_idx;
    std::vector<IdxType> future_layer;
    std::vector<IdxType> updated_layer;
    std::set<IdxType> gate_execute_idx_list;

    // For single-qubit gates
    std::vector<Gate> single_q_gates;
    std::vector<IdxType> num_single_before; // how many single-q gates appear before each 2-q gate
    IdxType single_count = 0;

    // Collect single-qubit gates
    for (IdxType i = 0; i < (IdxType)gate_info.size(); i++)
    {
        // If gate_info[i] is a single-qubit gate (ctrl == -1) and not measurement
        if (gate_info[i].ctrl == -1 && std::strcmp(OP_NAMES[gate_info[i].op_name], "MA") != 0)
        {
            single_count++;
            single_q_gates.push_back(gate_info[i]);
        }
        else
        {
            // Record how many single gates we saw up to this point
            num_single_before.push_back(single_count);
        }
    }

    // Start with the user-provided initial_mapping
    std::vector<IdxType> mapping = initial_mapping;

    // If debug
    double total_maintain_time = 0.0;
    double total_pickone_time  = 0.0;

    while (executed_cnt < gate_num)
    {
        // 1) Find which gates are executable
        std::set<IdxType> exec_gates = find_executable_gates(mapping, current_layer, circuit, distance_mat);

        // 2) If we can execute something
        if (!exec_gates.empty())
        {
            // Optionally measure time
            // cpu_timer t; t.start_timer();
            maintain_layer(
                current_layer, exec_gates, circuit,
                gate_state, following_gates_idx, qubit_state,
                gate_dependency, updated_layer, future_layer, 1
            );
            // t.stop_timer(); total_maintain_time += t.measure();

            // Add gates to return_circuit
            for (auto g_idx : exec_gates)
            {
                // Insert single-qubit gates that come before this 2-qubit gate
                // (based on your code logic in the snippet)
                // If you have a separate dependency structure for single-qubit gates,
                // insert them here as well.

                // The 2-qubit gate
                Gate g = circuit_gate[g_idx];
                g.qubit = mapping[g.qubit];
                g.ctrl  = mapping[g.ctrl];
                return_circuit.push_back(g);
            }
            executed_cnt += (IdxType)exec_gates.size();
            current_layer = updated_layer;
        }
        else
        {
            // 3) No gates are executable => pick a SWAP
            // cpu_timer t; t.start_timer();
            std::vector<IdxType> swap_pair = pick_one_movement(
                mapping, current_layer, future_layer,
                distance_mat, qubit_num, circuit, chip
            );
            // t.stop_timer(); total_pickone_time += t.measure();

            // Record the SWAP in return_circuit
            Gate swapG(OP::SWAP, swap_pair[1], swap_pair[0]); 
            return_circuit.push_back(swapG);
            swap_count++;
        }
    }

    // 4) After all 2-qubit gates are placed, append any leftover single-qubit gates
    //    that haven’t been inserted yet, if your logic requires it.
    //    (Your snippet checks for single-qubit gates that weren't included previously.)
    // For example:
    // std::unordered_set<IdxType> used_single_indices;
    // (Use your logic here to track which single-qubit gates have already been inserted.)

    // Update mapping
    initial_mapping = mapping;

    if (debug_level > 1)
    {
        std::cout << "Total maintain_layer time: " << total_maintain_time << " ms\n"
                  << "Total pick_one_movement time: " << total_pickone_time << " ms\n";
    }
    return swap_count;
}

/**
 * @brief Main function to perform SABRE-like routing on a circuit.
 *
 * This does multiple rounds (e.g., 1st, 2nd, 3rd) of the one_round_optimization pass,
 * reversing gate order between rounds, etc.
 *
 * @param circuit     [in/out] The circuit to be routed
 * @param chip        [in]     The hardware chip (distance matrix, edges, etc.)
 * @param debug_level [in]     Debug verbosity
 */
inline void Routing(std::shared_ptr<Circuit> circuit, std::shared_ptr<Chip> chip, IdxType debug_level)
{
    IdxType n_qubits  = circuit->num_qubits();
    auto gate_info    = circuit->get_gates();
    // Filter 2-qubit gates for the sabre pass
    std::vector<Gate> cx_gates;
    cx_gates.reserve(gate_info.size());

    for (const auto &g : gate_info)
    {
        if (g.ctrl != -1 && std::strcmp(OP_NAMES[g.op_name], "MA") != 0)
        {
            cx_gates.push_back(g);
        }
    }

    // Prepare a random initial mapping
    std::vector<IdxType> initial_mapping(n_qubits);
    std::iota(initial_mapping.begin(), initial_mapping.end(), 0);

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(initial_mapping.begin(), initial_mapping.end(), g);

    if (debug_level > 1)
    {
        std::cout << "******* 1st round sabre optimization *******\n";
    }

    // We'll store the final expanded circuit after each round
    std::vector<Gate> sabre_circuit;

    // Round 1
    std::vector<std::vector<IdxType>> empty_gate_qubit; // if you store something for the gates
    one_round_optimization(initial_mapping, cx_gates, chip->distance_mat, gate_info,
                           chip, empty_gate_qubit, sabre_circuit, debug_level);

    // Round 2
    if (debug_level > 1)
    {
        std::cout << "******* 2nd round sabre optimization *******\n";
    }
    sabre_circuit.clear();
    std::reverse(cx_gates.begin(), cx_gates.end()); 
    one_round_optimization(initial_mapping, cx_gates, chip->distance_mat, gate_info,
                           chip, empty_gate_qubit, sabre_circuit, debug_level);

    // Round 3
    if (debug_level > 1)
    {
        std::cout << "******* 3rd round sabre optimization *******\n";
    }
    sabre_circuit.clear();
    std::reverse(cx_gates.begin(), cx_gates.end()); 
    one_round_optimization(initial_mapping, cx_gates, chip->distance_mat, gate_info,
                           chip, empty_gate_qubit, sabre_circuit, debug_level);

    // Store final gates into the circuit
    circuit->set_mapping(initial_mapping);
    circuit->set_gates(sabre_circuit);

    // If needed, you could further decompose gates or measure final cost
}

} // namespace QASMTrans