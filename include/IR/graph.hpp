#pragma once

#include <unordered_set>
#include <utility>
#include <vector>
#include <set>
#include <iostream>

#include "../QASMTransPrimitives.hpp"

using namespace std;
namespace QASMTrans
{

    class Graph
    {
    public:
        Graph() {}
        Graph(const vector<pair<IdxType, IdxType>> &edges)
        {
            for (const auto &edge : edges)
            {
                addEdge(edge.first, edge.second);
            }
        }
        void addEdge(IdxType u, IdxType v)
        {
            // Add the vertices to the vertex set
            vertices.insert(u);
            vertices.insert(v);

            // Add the edge to the edge list
            edges.push_back({u, v});
        }
        bool edgeExists(IdxType u, IdxType v) const
        {
            for (const auto &edge : edges)
            {
                if ((edge.first == u && edge.second == v) || (edge.first == v && edge.second == u))
                {
                    return true;
                }
            }
            return false;
        }
        unordered_set<IdxType> getVertices() const
        {
            return vertices;
        }
        vector<pair<IdxType, IdxType>> getEdges() const
        {
            return edges;
        }
        void printGraph()
        {
            for (const auto &edge : edges)
            {
                cout << edge.first << " -> " << edge.second << endl;
            }
        }

    private:
        unordered_set<IdxType> vertices;
        vector<pair<IdxType, IdxType>> edges;
    };

}