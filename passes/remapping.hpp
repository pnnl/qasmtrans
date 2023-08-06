#ifndef REMAPPING_H
#define REMAPPING_H

#include "qasmtrans.hpp"
#include "gate.hpp"
#include "circuit.hpp"

using namespace QASMTrans;
using namespace std;

void Remap(shared_ptr<Circuit> circuit)
{
    vector<IdxType> initial_mapping = circuit->get_mapping();
    vector<Gate> gate_info = circuit->get_gates();
    IdxType n_qubits = circuit->num_qubits();
    vector<IdxType> new_mapping(n_qubits);
    vector<pair<IdxType, IdxType>> gateCountVec(n_qubits);
    for (int i = 0; i < n_qubits; i++) 
    {
        gateCountVec[i].first = i;
        gateCountVec[i].second = 0;
    }
    for (auto g : gate_info)
    {
        if (g.ctrl != -1)
        {
            int qubit = g.ctrl;
            auto it = find_if(gateCountVec.begin(), gateCountVec.end(),
                    [qubit](const pair<IdxType, IdxType>& element) {
                    return element.first == qubit;
                    });
            if (it != gateCountVec.end())
            {
                it->second++;
            }
            else 
            {
                gateCountVec.push_back({qubit, 1});
            }
        }
        if (g.qubit != -1)
        {
            int qubit = g.qubit;
            auto it = find_if(gateCountVec.begin(), gateCountVec.end(),
                               [qubit](const pair<IdxType, IdxType>& element) {
                                   return element.first == qubit;
                               });
            if (it != gateCountVec.end()) 
            {
                it->second++;
            }
            else 
            {
                gateCountVec.push_back({qubit, 1});
            }
        }
    }
    sort(gateCountVec.begin(), gateCountVec.end(),
              [](const pair<IdxType, IdxType>& a, const pair<IdxType, int>& b) {
                  return a.second < b.second;
              });
    
    IdxType assiged = 0;
    while (assiged < n_qubits)
    {
        if (assiged < 5) 
        {
            new_mapping[assiged] = gateCountVec[assiged].first;
            assiged++;
        } 
        else 
        {
            new_mapping[n_qubits-1-(assiged-5)] = gateCountVec[assiged].first;
            assiged++;
        }
    }
    for (auto& g : gate_info)
    {
        if(g.ctrl != -1) 
        {
            g.ctrl = new_mapping[g.ctrl];
        } 
        if(g.qubit != -1) 
        {
            g.qubit = new_mapping[g.qubit];
        }
    }
    //! update the newmapping to logical if initial mapping size is not 0
    vector<IdxType> new_mapping_logical(n_qubits);
    if (initial_mapping.size() != 0) 
    {
        for (IdxType i = 0; i < n_qubits; i++)
        {
            new_mapping_logical[i] = new_mapping[initial_mapping[i]];
            // cout<<new_mapping_logical[i];
        }
        new_mapping = new_mapping_logical;
    }
    circuit->set_mapping(new_mapping);
    circuit->set_gates(gate_info);
}
#endif /* REMAPPING_H */
