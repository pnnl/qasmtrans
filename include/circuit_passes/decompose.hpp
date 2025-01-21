#pragma once

#include "../QASMTransPrimitives.hpp"
#include "../IR/gate.hpp"

using namespace QASMTrans;
using namespace std;

Gate BasicRZ(ValType theta, IdxType qubit)
{
    Gate G(OP::RZ, qubit, -1, -1, 1, theta);
    return G;
}
Gate BasicSX(IdxType qubit)
{
    Gate G(OP::SX, qubit);
    return G;
}
Gate BasicX(IdxType qubit)
{
    Gate G(OP::X, qubit);
    return G;
}
Gate BasicCX(IdxType ctrl, IdxType qubit)
{
    Gate G(OP::CX, qubit, ctrl, -1, 2);
    return G;
}
vector<Gate> decomposeHadamard(IdxType qubit)
{
    vector<Gate> decomposedGates;
    // Assuming the Gate constructor takes name, control qubit, target qubit, and angle (in that order)
    // Also assuming single qubit gates use -1 or similar for non-applicable target/control qubits
    // sx x rz(-pi/2) sx x
    Gate rzgate = BasicRZ(-PI / 2, qubit);
    Gate sxgate = BasicSX(qubit);
    Gate xgate = BasicX(qubit);
    decomposedGates.push_back(xgate);
    decomposedGates.push_back(sxgate);
    decomposedGates.push_back(rzgate);
    decomposedGates.push_back(sxgate);
    decomposedGates.push_back(xgate);
    return decomposedGates;
}
vector<Gate> decomposeT(IdxType qubit)
{
    vector<Gate> decomposedGates;
    // Applies the π/8 gate to a single qubit.
    Gate rzgate = BasicRZ(PI / 4, qubit);
    decomposedGates.push_back(rzgate);
    return decomposedGates;
}
vector<Gate> decomposeTdg(IdxType qubit)
{
    vector<Gate> decomposedGates;
    // Applies the π/8 gate to a single qubit.
    Gate rzgate = BasicRZ(-PI / 4, qubit);
    decomposedGates.push_back(rzgate);
    return decomposedGates;
}
vector<Gate> decomposeZ(IdxType qubit)
{
    vector<Gate> decomposedGates;
    Gate rzgate = BasicRZ(PI, qubit);
    decomposedGates.push_back(rzgate);
    return decomposedGates;
}
vector<Gate> decomposeY(IdxType qubit)
{
    vector<Gate> decomposedGates;
    Gate sxgate = BasicSX(qubit);
    decomposedGates.push_back(sxgate);
    vector<Gate> decomposez = decomposeZ(qubit);
    decomposedGates.insert(decomposedGates.end(), decomposez.begin(), decomposez.end());
    decomposedGates.push_back(sxgate);
    decomposedGates.push_back(sxgate);
    decomposedGates.push_back(sxgate);
    return decomposedGates;
}
vector<Gate> decomposeRx(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    vector<Gate> decomposeh = decomposeHadamard(qubit);
    decomposedGates.insert(decomposedGates.end(), decomposeh.begin(), decomposeh.end());
    Gate rzgate = BasicRZ(theta, qubit);
    decomposedGates.push_back(rzgate);
    decomposedGates.insert(decomposedGates.end(), decomposeh.begin(), decomposeh.end());
    return decomposedGates;
}
vector<Gate> decomposeP(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    Gate rzgate = BasicRZ(theta, qubit);
    decomposedGates.push_back(rzgate);
    return decomposedGates;
}

vector<Gate> decomposeRI(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    Gate rzgate = BasicRZ(2 * theta, qubit);
    decomposedGates.push_back(rzgate);
    vector<Gate> decomposeh = decomposeZ(qubit);
    decomposedGates.insert(decomposedGates.end(), decomposeh.begin(), decomposeh.end());
    return decomposedGates;
}
vector<Gate> decomposeRy(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    Gate sxgate = BasicSX(qubit);
    Gate rzgate = BasicRZ(theta, qubit);
    decomposedGates.push_back(sxgate);
    decomposedGates.push_back(rzgate);
    decomposedGates.push_back(sxgate);
    decomposedGates.push_back(sxgate);
    decomposedGates.push_back(sxgate);
    return decomposedGates;
}
vector<Gate> decomposeS(IdxType qubit)
{
    vector<Gate> decomposedGates;
    Gate rzgate = BasicRZ(PI / 2, qubit);
    decomposedGates.push_back(rzgate);
    return decomposedGates;
}
vector<Gate> decomposeSdg(IdxType qubit)
{
    vector<Gate> decomposedGates;
    Gate rzgate = BasicRZ(-PI / 2, qubit);
    decomposedGates.push_back(rzgate);
    return decomposedGates;
}
vector<Gate> decomposeU(ValType theta, ValType phi, ValType lam, IdxType qubit)
{
    vector<Gate> decomposedGates;
    if (lam != 0)
    {
        decomposedGates.push_back(BasicRZ(lam, qubit));
    }
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(theta + PI, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(3 * PI + phi, qubit));

    return decomposedGates;
}
vector<Gate> decomposeCZ(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    vector<Gate> decomposery = decomposeHadamard(qubit);
    decomposedGates.insert(decomposedGates.end(), decomposery.begin(), decomposery.end());
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.insert(decomposedGates.end(), decomposery.begin(), decomposery.end());
    return decomposedGates;
}
vector<Gate> decomposeCY(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(-PI / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCH(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(-PI, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI * 3 / 4, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(PI / 4, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    return decomposedGates;
}
vector<Gate> decomposeCS(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(PI / 4, ctrl));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-PI / 4, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(PI / 4, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCSDG(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicRZ(PI / 4, ctrl));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-PI / 4, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(PI / 4, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCT(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(PI / 8, ctrl));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-PI / 8, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(PI / 8, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCTDG(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(-PI / 8, ctrl));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(PI / 8, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-PI / 8, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCRX(ValType theta, IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicRZ(theta / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-theta / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    return decomposedGates;
}
vector<Gate> decomposeRXX(ValType theta, IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, ctrl));
    decomposedGates.push_back(BasicSX(ctrl));
    decomposedGates.push_back(BasicRZ(PI / 2, ctrl));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(theta, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, ctrl));
    decomposedGates.push_back(BasicSX(ctrl));
    decomposedGates.push_back(BasicRZ(PI / 2, ctrl));
    return decomposedGates;
}
vector<Gate> decomposeRYY(ValType theta, IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicSX(ctrl));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(theta, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-PI, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(-PI, qubit));
    decomposedGates.push_back(BasicRZ(-PI, ctrl));
    decomposedGates.push_back(BasicSX(ctrl));
    decomposedGates.push_back(BasicRZ(-PI, ctrl));
    return decomposedGates;
}
vector<Gate> decomposeRZZ(ValType theta, IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(theta, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCRY(ValType theta, IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI + theta / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(3 * PI, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI - theta / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(3 * PI, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCRZ(ValType theta, IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    // cout<<"theta is"<<theta<<endl;
    // cout<<"phi is"<<phi<<endl;
    // cout<<"lam is"<<lam<<endl;
    // cout<<"gamma is"<<gamma<<endl;
    decomposedGates.push_back(BasicRZ(theta / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-theta / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCSX(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicRZ(PI / 4, ctrl));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-PI / 4, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(3 * PI / 4, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCP(ValType theta, IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(theta / 2, ctrl));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-theta / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(theta / 2, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCU(ValType theta, ValType phi, ValType lam, ValType gamma,
                         IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    // cout<<"theta is"<<theta<<endl;
    // cout<<"phi is"<<phi<<endl;
    // cout<<"lam is"<<lam<<endl;
    // cout<<"gamma is"<<gamma<<endl;
    decomposedGates.push_back(BasicRZ(gamma, ctrl));
    decomposedGates.push_back(BasicRZ(lam / 2 + phi / 2, ctrl));
    decomposedGates.push_back(BasicRZ(lam / 2 - phi / 2, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicRZ(-lam / 2 - phi / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI - theta / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(3 * PI, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI + theta / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(3 * PI + phi, qubit));
    return decomposedGates;
}
vector<Gate> decomposeSWAP(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    Gate cxgate = BasicCX(ctrl, qubit);
    Gate cxgate2 = BasicCX(qubit, ctrl);
    decomposedGates.push_back(cxgate);
    decomposedGates.push_back(cxgate2);
    decomposedGates.push_back(cxgate);
    return decomposedGates;
}
// gate ccx a,b,c
// {
//   h c;
//   cx b,c; tdg c;
//   cx a,c; t c;
//   cx b,c; tdg c;
//   cx a,c; t b; t c; h c;
//   cx a,b; t a; tdg b;
//   cx a,b;
// }
vector<Gate> decomposeCCX(IdxType a, IdxType b, IdxType c)
{
    vector<Gate> decomposedGates;

    decomposedGates.push_back(Gate(OP::H, c));
    decomposedGates.push_back(BasicCX(b, c));
    decomposedGates.push_back(Gate(OP::TDG, c));
    decomposedGates.push_back(BasicCX(a, c));
    decomposedGates.push_back(Gate(OP::T, c));
    decomposedGates.push_back(BasicCX(b, c));
    decomposedGates.push_back(Gate(OP::TDG, c));
    decomposedGates.push_back(BasicCX(a, c));
    decomposedGates.push_back(Gate(OP::T, b));
    decomposedGates.push_back(Gate(OP::T, c));
    decomposedGates.push_back(Gate(OP::H, c));
    decomposedGates.push_back(BasicCX(a, b));
    decomposedGates.push_back(Gate(OP::T, a));
    decomposedGates.push_back(Gate(OP::TDG, b));
    decomposedGates.push_back(BasicCX(a, b));
    return decomposedGates;
}

vector<Gate> decomposeRCCX(IdxType a, IdxType b, IdxType c)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(Gate(OP::U, c, -1, -1, 1, PI / 2, 0, PI));
    decomposedGates.push_back(Gate(OP::U, c, -1, -1, 1, 0, 0, PI / 4));
    decomposedGates.push_back(BasicCX(b, c));
    decomposedGates.push_back(Gate(OP::U, c, -1, -1, 1, 0, 0, -PI / 4));
    decomposedGates.push_back(BasicCX(a, c));
    decomposedGates.push_back(Gate(OP::U, c, -1, -1, 1, 0, 0, PI / 4));
    decomposedGates.push_back(BasicCX(b, c));
    decomposedGates.push_back(Gate(OP::U, c, -1, -1, 1, 0, 0, -PI / 4));
    decomposedGates.push_back(Gate(OP::U, c, -1, -1, 1, PI / 2, 0, PI));
    return decomposedGates;
}
vector<Gate> decomposeCSWAP(IdxType a, IdxType b, IdxType c)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicCX(c, b));
    vector<Gate> decomposeccx = decomposeCCX(a, b, c);
    decomposedGates.insert(decomposedGates.end(), decomposeccx.begin(), decomposeccx.end());
    decomposedGates.push_back(BasicCX(c, b));
    return decomposedGates;
}
void Decompose_three_to_two(shared_ptr<Circuit> circuit)
{
    vector<Gate> circuit_gates = circuit->get_gates();
    vector<Gate> decomposedGates;
    for (Gate g : circuit_gates)
    {
        if (g.n_qubits > 2)
        {
            // std::cout<<"find three-qubit gates"<<std::endl;
            // print gate and control, target , extra qubit
            // std::cout<<"gate name is"<<OP_NAMES[g.op_name];
            // std::cout<<"gate control is"<<g.ctrl;
            // std::cout<<"gate target is"<<g.qubit;
            // std::cout<<"gate extra is"<<g.extra<<std::endl;
            if (strcmp(OP_NAMES[g.op_name], "CSWAP") == 0)
            {
                vector<Gate> Decomposed_gates = decomposeCSWAP(g.qubit, g.ctrl, g.extra);
                decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
            }
            else if (strcmp(OP_NAMES[g.op_name], "CCX") == 0)
            {
                vector<Gate> Decomposed_gates = decomposeCCX(g.qubit, g.ctrl, g.extra);
                decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
            }
            else if (strcmp(OP_NAMES[g.op_name], "RCCX") == 0)
            {
                vector<Gate> Decomposed_gates = decomposeRCCX(g.qubit, g.ctrl, g.extra);
                decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
            }
        }
        else
        {
            decomposedGates.push_back(g);
        }
    }

    circuit->set_gates(decomposedGates);
    // print circuit gates
    //  if (debug_level > 1) {
    //      vector<Gate> circuit_gates2 = circuit->get_gates();
    //      for (Gate g : circuit_gates2)
    //      {
    //          std::cout<<"gate name is"<<OP_NAMES[g.op_name];
    //          std::cout<<"gate control is"<<g.ctrl;
    //          std::cout<<"gate target is"<<g.qubit;
    //          std::cout<<"gate extra is"<<g.extra;
    //          //print angle
    //          std::cout<<"gate angle is"<<g.theta<<","<<g.phi<<","<<g.lam<<std::endl;

    //     }
    // }

    return;
}
void Decompose(shared_ptr<Circuit> circuit, IdxType mode)
{
    vector<Gate> circuit_gates = circuit->get_gates();
    vector<Gate> decomposedGates;
    for (Gate g : circuit_gates)
    {
        if (strcmp(OP_NAMES[g.op_name], "H") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeHadamard(g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "T") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeT(g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "Z") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeZ(g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "TDG") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeTdg(g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "Y") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeY(g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "S") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeS(g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "SDG") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeSdg(g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "RX") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeRx(g.theta, g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "RY") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeRy(g.theta, g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());

        } //^ double check RI gate later
        else if (strcmp(OP_NAMES[g.op_name], "RI") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeRI(g.theta, g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "P") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeP(g.theta, g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "U") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeU(g.theta, g.phi, g.lam, g.qubit);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CZ") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCZ(g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CY") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCY(g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CH") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCH(g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CS") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCS(g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CSDG") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCSDG(g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CT") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCT(g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CTDG") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCTDG(g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CRX") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCRX(g.theta, g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CRY") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCRY(g.theta, g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CRZ") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCRZ(g.theta, g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CSX") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCSX(g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CP") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCP(g.theta, g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CU") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeCU(g.theta, g.phi, g.lam, g.gamma, g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "RXX") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeRXX(g.theta, g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "RYY") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeRYY(g.theta, g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "RZZ") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeRZZ(g.theta, g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "SWAP") == 0)
        {
            vector<Gate> Decomposed_gates = decomposeSWAP(g.qubit, g.ctrl);
            decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
        }
        else if (strcmp(OP_NAMES[g.op_name], "CX") == 0)
        {
            decomposedGates.push_back(g);
        }
        else if (strcmp(OP_NAMES[g.op_name], "RZ") == 0)
        {
            // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
            decomposedGates.push_back(BasicRZ(g.theta, g.qubit));
        }
        else if (strcmp(OP_NAMES[g.op_name], "SX") == 0)
        {
            decomposedGates.push_back(g);
        }
        else if (strcmp(OP_NAMES[g.op_name], "X") == 0)
        {
            decomposedGates.push_back(g);
        }
        else if (strcmp(OP_NAMES[g.op_name], "MA") == 0)
        {
            decomposedGates.push_back(g);
        }
        else if (strcmp(OP_NAMES[g.op_name], "ID") == 0)
        {
            decomposedGates.push_back(g);
        }
        else if (strcmp(OP_NAMES[g.op_name], "RESET") == 0)
        {
            decomposedGates.push_back(g);
        }
        else
        {
            cout << "Error: cannot find this gate: " << endl;
            cout << "Gate " << OP_NAMES[g.op_name] << " not supported" << endl;
            decomposedGates.push_back(g);
        }
    }
    if (mode == 0)
    {
        circuit->set_gates(decomposedGates);
        return;
    }
    else if (mode == 1)
    {
        vector<Gate> decomposedGates_IonQ;
        for (Gate g : decomposedGates)
        {
            if (strcmp(OP_NAMES[g.op_name], "RZ") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_IonQ.push_back(BasicRZ(g.theta, g.qubit));
            }
            else if (strcmp(OP_NAMES[g.op_name], "SX") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_IonQ.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI / 2));
            }
            else if (strcmp(OP_NAMES[g.op_name], "X") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_IonQ.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI));
            }
            else if (strcmp(OP_NAMES[g.op_name], "CX") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_IonQ.push_back(Gate(OP::RY, g.qubit, -1, -1, 1, PI / 2));
                decomposedGates_IonQ.push_back(Gate(OP::RXX, g.qubit, g.ctrl, -1, 2, PI / 2));
                decomposedGates_IonQ.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, -PI / 2));
                decomposedGates_IonQ.push_back(Gate(OP::RX, g.ctrl, -1, -1, 1, -PI / 2));
                decomposedGates_IonQ.push_back(Gate(OP::RY, g.qubit, -1, -1, 1, -PI / 2));
            }
        }
        circuit->set_gates(decomposedGates_IonQ);
        return;
    }
    else if (mode == 2)
    {
        vector<Gate> decomposedGates_Quantinuum;
        for (Gate g : decomposedGates)
        {
            if (strcmp(OP_NAMES[g.op_name], "RZ") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_Quantinuum.push_back(BasicRZ(g.theta, g.qubit));
            }
            else if (strcmp(OP_NAMES[g.op_name], "SX") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_Quantinuum.push_back(Gate(OP::U, g.qubit, -1, -1, 1, PI / 2));
            }
            else if (strcmp(OP_NAMES[g.op_name], "X") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_Quantinuum.push_back(Gate(OP::U, g.qubit, -1, -1, 1, PI));
            }
            else if (strcmp(OP_NAMES[g.op_name], "CX") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_Quantinuum.push_back(Gate(OP::U, g.qubit, -1, -1, 1, -PI / 2, PI / 2));
                decomposedGates_Quantinuum.push_back(Gate(OP::ZZ, g.qubit, g.ctrl, -1, 2, PI / 2));
                decomposedGates_Quantinuum.push_back(Gate(OP::RZ, g.ctrl, -1, -1, 1, -PI / 2));
                decomposedGates_Quantinuum.push_back(Gate(OP::U, g.qubit, -1, -1, 1, PI / 2, PI));
                decomposedGates_Quantinuum.push_back(Gate(OP::RZ, g.ctrl, -1, -1, 1, -PI / 2));
            }
        }
        circuit->set_gates(decomposedGates_Quantinuum);
        return;
    }
    else if (mode == 3)
    {
        vector<Gate> decomposedGates_Rigetti;
        for (Gate g : decomposedGates)
        {
            if (strcmp(OP_NAMES[g.op_name], "RZ") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_Rigetti.push_back(BasicRZ(g.theta, g.qubit));
            }
            else if (strcmp(OP_NAMES[g.op_name], "SX") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_Rigetti.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI / 2));
            }
            else if (strcmp(OP_NAMES[g.op_name], "X") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_Rigetti.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI));
            }
            else if (strcmp(OP_NAMES[g.op_name], "CX") == 0)
            {
                // cout<<"gate name is"<<OP_NAMES[g.op_name]<<"angle is"<<g.theta<<endl;
                decomposedGates_Rigetti.push_back(Gate(OP::RZ, g.qubit, -1, -1, 1, -PI / 2));
                decomposedGates_Rigetti.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, -PI / 2));
                decomposedGates_Rigetti.push_back(Gate(OP::RZ, g.qubit, -1, -1, 1, -PI / 2));
                decomposedGates_Rigetti.push_back(Gate(OP::CZ, g.qubit, g.ctrl, 2));
                decomposedGates_Rigetti.push_back(Gate(OP::RZ, g.qubit, -1, -1, 1, -PI / 2));
                decomposedGates_Rigetti.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, -PI / 2));
                decomposedGates_Rigetti.push_back(Gate(OP::RZ, g.qubit, -1, -1, 1, -PI / 2));
            }
        }
        circuit->set_gates(decomposedGates_Rigetti);
        return;
    }
    else if (mode == 4)
    {
        vector<Gate> decomposedGates_Quafu;
        for (Gate g : decomposedGates)
        {
            if (strcmp(OP_NAMES[g.op_name], "RZ") == 0)
            {
                decomposedGates_Quafu.push_back(BasicRZ(g.theta, g.qubit));
            }
            else if (strcmp(OP_NAMES[g.op_name], "SX") == 0)
            {
                decomposedGates_Quafu.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI / 2));
            }
            else if (strcmp(OP_NAMES[g.op_name], "X") == 0)
            {
                decomposedGates_Quafu.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI));
            }
            else if (strcmp(OP_NAMES[g.op_name], "CX") == 0)
            {
                decomposedGates_Quafu.push_back(Gate(OP::H, g.qubit));
                decomposedGates_Quafu.push_back(Gate(OP::CZ, g.qubit, g.ctrl, 2));
                decomposedGates_Quafu.push_back(Gate(OP::H, g.qubit));
            }
        }
        circuit->set_gates(decomposedGates_Quafu);
        return;
    }
}
