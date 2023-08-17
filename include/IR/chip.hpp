#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include "../nlomann/json.hpp"
#include "graph.hpp"
#include <limits.h>

using json = nlohmann::json;

using namespace std;
using namespace QASMTrans;

namespace QASMTrans
{

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
    };

    vector<vector<IdxType>> floyd(IdxType node_num, vector<vector<IdxType>> &adj_mat)
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

    shared_ptr<Chip> constructChip(IdxType qubit_num, string backendpath, bool run_with_limit, IdxType debug_level)
    {
        // string path = "../data/device/" +backend_name+ ".json";
        // string path = "/Users/lian599/local/QASMTrans/data/devices/" +backend_name+ ".json";
        // string path = backend_name;

        ifstream f(backendpath);
        bool limited_arc = run_with_limit;
        if (f.fail())
            throw logic_error("Device config file not found at " + backendpath);
        json backend_config = json::parse(f);
        vector<pair<IdxType, IdxType>> pairs;
        auto cx_coupling = backend_config["cx_coupling"];
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
            }
            else
            {
                if (first < qubit_num && second < qubit_num)
                {
                    pairs.push_back(make_pair(first, second));
                }
            }
        }
        vector<pair<IdxType, IdxType>> edges;
        for (auto &p : pairs)
        {
            edges.push_back(make_pair(p.first, p.second));
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
        auto chip_qubit_num = backend_config["num_qubits"];
        chip->chip_qubit_num = chip_qubit_num;
        return chip;
    }

}