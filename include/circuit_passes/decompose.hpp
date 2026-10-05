#pragma once

#include "../QASMTransPrimitives.hpp"
#include "../IR/gate.hpp"

#include <algorithm>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <sstream>

using namespace QASMTrans;
using namespace std;

namespace QASMTrans
{
    extern std::unordered_set<std::string> g_device_basis_gates;
    extern std::unordered_map<std::string, std::string> g_merged_gate_aliases;
}

inline std::string toLowerCase(const char *name)
{
    std::string lower = name ? std::string(name) : std::string{};
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });
    return lower;
}

inline std::string lookupMergedAlias(const Gate &gate)
{
    if (g_merged_gate_aliases.empty())
    {
        return {};
    }
    std::vector<std::string> logical_candidates;
    logical_candidates.push_back(gate.lower_base_name());
    logical_candidates.push_back(gate.lower_name());
    if (!gate.logical_label.empty())
    {
        std::string label = gate.logical_label;
        std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        logical_candidates.push_back(std::move(label));
    }
    for (const auto &candidate : logical_candidates)
    {
        if (candidate.empty())
        {
            continue;
        }
        auto it = g_merged_gate_aliases.find(candidate);
        if (it != g_merged_gate_aliases.end())
        {
            return it->second;
        }
    }
    return {};
}

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
Gate BasicISWAP(IdxType ctrl, IdxType qubit)
{
    Gate G(OP::ISWAP, qubit, ctrl, -1, 2);
    return G;
}

vector<Gate> decomposeHadamard(IdxType qubit)
{
    vector<Gate> decomposedGates;
    // Gate ctor: (op, target, control, extra, ...) with -1 for unused qubits.
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
vector<Gate> decomposeHadamardToRZSX(IdxType qubit)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
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
vector<Gate> decomposeYToRZX(IdxType qubit)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicX(qubit));
    decomposedGates.push_back(BasicRZ(-PI / 2, qubit));
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
vector<Gate> decomposeRxToRZSX(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    // Match Qiskit's IBM-basis lowering, up to global phase:
    //   RX(theta) = RZ(pi/2) SX RZ(theta + pi) SX RZ(pi/2)
    //
    // The previous sign pattern implemented a different unitary and broke
    // circuits containing RX gates even when no routing was required.
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(theta + PI, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    return decomposedGates;
}
vector<Gate> decomposeRxToPrx(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(Gate(OP::PRX, qubit, -1, -1, 1, theta, 0));
    return decomposedGates;
}
vector<Gate> decomposeRxToFixedRx(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    // RX(theta) = RZ(-pi/2) RX(pi/2) RZ(theta) RX(-pi/2) RZ(pi/2)
    decomposedGates.push_back(BasicRZ(-PI / 2, qubit));
    decomposedGates.push_back(Gate(OP::RX, qubit, -1, -1, 1, PI / 2));
    decomposedGates.push_back(BasicRZ(theta, qubit));
    // RX(-pi/2) = RZ(pi) RX(pi/2) RZ(pi)
    decomposedGates.push_back(BasicRZ(PI, qubit));
    decomposedGates.push_back(Gate(OP::RX, qubit, -1, -1, 1, PI / 2));
    decomposedGates.push_back(BasicRZ(PI, qubit));
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    return decomposedGates;
}
vector<Gate> decomposeRyToFixedRx(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    // RY(theta) = RZ(-pi/2) RX(theta) RZ(pi/2)
    decomposedGates.push_back(BasicRZ(-PI / 2, qubit));
    vector<Gate> rx = decomposeRxToFixedRx(theta, qubit);
    decomposedGates.insert(decomposedGates.end(), rx.begin(), rx.end());
    decomposedGates.push_back(BasicRZ(PI / 2, qubit));
    return decomposedGates;
}
vector<Gate> decomposePrxToFixedRx(ValType theta, ValType phi, IdxType qubit)
{
    vector<Gate> decomposedGates;
    // PRX(theta, phi) = RZ(phi) RX(theta) RZ(-phi)
    decomposedGates.push_back(BasicRZ(phi, qubit));
    vector<Gate> rx = decomposeRxToFixedRx(theta, qubit);
    decomposedGates.insert(decomposedGates.end(), rx.begin(), rx.end());
    decomposedGates.push_back(BasicRZ(-phi, qubit));
    return decomposedGates;
}
vector<Gate> decomposeRzToPrxOnly(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    // RZ(theta) = RX(pi/2) RY(theta) RX(-pi/2) using PRX pulses.
    decomposedGates.push_back(Gate(OP::PRX, qubit, -1, -1, 1, PI / 2, 0));
    decomposedGates.push_back(Gate(OP::PRX, qubit, -1, -1, 1, theta, PI / 2));
    decomposedGates.push_back(Gate(OP::PRX, qubit, -1, -1, 1, -PI / 2, 0));
    return decomposedGates;
}
vector<Gate> decomposeRyToPrx(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    // RY(theta) = PRX(theta, pi/2)
    decomposedGates.push_back(Gate(OP::PRX, qubit, -1, -1, 1, theta, PI / 2));
    return decomposedGates;
}
vector<Gate> decomposeSxToPrx(IdxType qubit)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(Gate(OP::PRX, qubit, -1, -1, 1, PI / 2, 0));
    return decomposedGates;
}
vector<Gate> decomposeXToPrx(IdxType qubit)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(Gate(OP::PRX, qubit, -1, -1, 1, PI, 0));
    return decomposedGates;
}
vector<Gate> decomposeHToPrx(IdxType qubit)
{
    vector<Gate> decomposedGates;
    // H = RY(pi/2) RZ(pi) up to global phase; map to PRX
    vector<Gate> ry = decomposeRyToPrx(PI / 2, qubit);
    decomposedGates.insert(decomposedGates.end(), ry.begin(), ry.end());
    decomposedGates.push_back(BasicRZ(PI, qubit));
    return decomposedGates;
}
vector<Gate> decomposePRX(ValType theta, ValType phi, IdxType qubit)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(phi, qubit));
    vector<Gate> rx = decomposeRx(theta, qubit);
    decomposedGates.insert(decomposedGates.end(), rx.begin(), rx.end());
    decomposedGates.push_back(BasicRZ(-phi, qubit));
    return decomposedGates;
}

vector<Gate> decomposeCXToISWAP(IdxType ctrl, IdxType target, bool use_prx)
{
    vector<Gate> decomposedGates;
    const auto push_rz = [&](IdxType qubit, ValType theta)
    {
        if (use_prx)
        {
            auto rz = decomposeRzToPrxOnly(theta, qubit);
            decomposedGates.insert(decomposedGates.end(), rz.begin(), rz.end());
        }
        else
        {
            decomposedGates.push_back(BasicRZ(theta, qubit));
        }
    };
    const auto push_rx = [&](IdxType qubit, ValType theta)
    {
        if (use_prx)
        {
            decomposedGates.push_back(Gate(OP::PRX, qubit, -1, -1, 1, theta, 0));
        }
        else
        {
            vector<Gate> rx = decomposeRxToFixedRx(theta, qubit);
            decomposedGates.insert(decomposedGates.end(), rx.begin(), rx.end());
        }
    };
    // CX(ctrl -> target) via two iSWAPs (time order: rightmost first in algebraic expression).
    push_rz(target, -PI / 2);
    decomposedGates.push_back(BasicISWAP(ctrl, target));
    push_rx(ctrl, PI / 2);
    decomposedGates.push_back(BasicISWAP(ctrl, target));
    push_rz(target, -PI / 2);
    push_rx(target, -PI / 2);
    push_rz(ctrl, PI / 2);
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
vector<Gate> decomposeRyToRZSX(ValType theta, IdxType qubit)
{
    vector<Gate> decomposedGates;
    auto wrap_angle = [](ValType angle) -> ValType
    {
        ValType wrapped = std::fmod(angle + PI, 2 * PI);
        if (wrapped < 0)
        {
            wrapped += 2 * PI;
        }
        wrapped -= PI;
        return wrapped;
    };
    const ValType local_theta = wrap_angle(theta);
    const ValType atol = 1e-12;

    // Match Qiskit's lower-count ZSX/ZSXX representative for RY up to global phase.
    if (std::abs(local_theta) < atol)
    {
        return decomposedGates;
    }
    if (std::abs(local_theta - PI / 2) < atol)
    {
        decomposedGates.push_back(BasicRZ(-PI / 2, qubit));
        decomposedGates.push_back(BasicSX(qubit));
        decomposedGates.push_back(BasicRZ(PI / 2, qubit));
        return decomposedGates;
    }
    if (std::abs(local_theta - PI) < atol || std::abs(local_theta + PI) < atol)
    {
        decomposedGates.push_back(BasicRZ(-PI, qubit));
        decomposedGates.push_back(BasicX(qubit));
        return decomposedGates;
    }

    decomposedGates.push_back(BasicRZ(-PI, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI - local_theta, qubit));
    decomposedGates.push_back(BasicSX(qubit));
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
vector<Gate> decomposeSxdg(IdxType qubit)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(BasicRZ(PI, qubit));
    decomposedGates.push_back(BasicSX(qubit));
    decomposedGates.push_back(BasicRZ(PI, qubit));
    return decomposedGates;
}
vector<Gate> decomposeCXToECR(IdxType ctrl, IdxType qubit)
{
    vector<Gate> decomposedGates;
    vector<Gate> sdg = decomposeSdg(ctrl);
    decomposedGates.insert(decomposedGates.end(), sdg.begin(), sdg.end());
    vector<Gate> sxdg = decomposeSxdg(qubit);
    decomposedGates.insert(decomposedGates.end(), sxdg.begin(), sxdg.end());
    decomposedGates.push_back(Gate(OP::ECR, qubit, ctrl, -1, 2));
    decomposedGates.push_back(BasicX(ctrl));
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
vector<Gate> decomposeRZX(ValType theta, IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    vector<Gate> had = decomposeHadamard(qubit);
    decomposedGates.insert(decomposedGates.end(), had.begin(), had.end());
    vector<Gate> rzz = decomposeRZZ(theta, qubit, ctrl);
    decomposedGates.insert(decomposedGates.end(), rzz.begin(), rzz.end());
    decomposedGates.insert(decomposedGates.end(), had.begin(), had.end());
    return decomposedGates;
}
vector<Gate> decomposeECR(IdxType qubit, IdxType ctrl)
{
    vector<Gate> decomposedGates;
    decomposedGates.push_back(Gate(OP::S, ctrl));
    decomposedGates.push_back(Gate(OP::SX, qubit));
    decomposedGates.push_back(BasicCX(ctrl, qubit));
    decomposedGates.push_back(Gate(OP::X, ctrl));
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
    const vector<Gate> &circuit_gates = circuit->gate_list();
    vector<Gate> decomposedGates;
    decomposedGates.reserve(circuit_gates.size());
    for (const Gate &g : circuit_gates)
    {
        std::string gate_name_lower = g.lower_name();
        if (g_device_basis_gates.find(gate_name_lower) != g_device_basis_gates.end())
        {
            decomposedGates.push_back(g);
            continue;
        }
        if (g.n_qubits > 2)
        {
            // std::cout<<"find three-qubit gates"<<std::endl;
            // print gate and control, target , extra qubit
            // std::cout<<"gate name is"<<OP_NAMES[g.op_name];
            // std::cout<<"gate control is"<<g.ctrl;
            // std::cout<<"gate target is"<<g.qubit;
            // std::cout<<"gate extra is"<<g.extra<<std::endl;
            if (g.name_equals("CSWAP"))
            {
                vector<Gate> Decomposed_gates = decomposeCSWAP(g.qubit, g.ctrl, g.extra);
                decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
            }
            else if (g.name_equals("CCX"))
            {
                vector<Gate> Decomposed_gates = decomposeCCX(g.qubit, g.ctrl, g.extra);
                decomposedGates.insert(decomposedGates.end(), Decomposed_gates.begin(), Decomposed_gates.end());
            }
            else if (g.name_equals("RCCX"))
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

    circuit->set_gates(std::move(decomposedGates));
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
void Decompose(shared_ptr<Circuit> circuit, IdxType mode, bool preserve_logical_metadata = true)
{
    const vector<Gate> &circuit_gates = circuit->gate_list();
    const bool basis_has_prx = g_device_basis_gates.find("prx") != g_device_basis_gates.end();
    const bool basis_has_rz = g_device_basis_gates.find("rz") != g_device_basis_gates.end();
    const bool basis_has_sx = g_device_basis_gates.find("sx") != g_device_basis_gates.end();
    const bool basis_has_x = g_device_basis_gates.find("x") != g_device_basis_gates.end();
    const bool basis_has_ecr = g_device_basis_gates.find("ecr") != g_device_basis_gates.end();

    auto has_native_basis = [](const std::string &gate_name)
    {
        return g_device_basis_gates.find(gate_name) != g_device_basis_gates.end();
    };

    auto append_gate = [](vector<Gate> &out, Gate gate, const Gate &source, bool inherit_metadata)
    {
        if (inherit_metadata)
        {
            gate.inherit_logical_metadata(source);
        }
        out.push_back(std::move(gate));
    };

    auto append_gates = [&](vector<Gate> &out, vector<Gate> gates, const Gate &source, bool inherit_metadata)
    {
        for (auto &gate : gates)
        {
            append_gate(out, std::move(gate), source, inherit_metadata);
        }
    };

    auto append_alias_or_native =
        [&](vector<Gate> &out,
            Gate gate,
            const Gate &source,
            bool inherit_metadata,
            bool allow_alias,
            const std::function<bool(const std::string &)> &native_allowed)
    {
        if (gate.has_custom_name())
        {
            append_gate(out, std::move(gate), source, inherit_metadata);
            return true;
        }
        if (allow_alias)
        {
            std::string alias = lookupMergedAlias(gate);
            if (!alias.empty())
            {
                gate.set_custom_name(alias);
                append_gate(out, std::move(gate), source, inherit_metadata);
                return true;
            }
        }
        if (native_allowed(gate.lower_name()))
        {
            append_gate(out, std::move(gate), source, inherit_metadata);
            return true;
        }
        return false;
    };

    auto decompose_u_for_basis = [&](const Gate &g)
    {
        vector<Gate> decomposed = decomposeU(g.theta, g.phi, g.lam, g.qubit);
        if (!basis_has_prx)
        {
            return decomposed;
        }
        vector<Gate> prx_decomposed;
        for (const Gate &gate : decomposed)
        {
            if (gate.name_equals("RZ"))
            {
                auto expanded = decomposeRzToPrxOnly(gate.theta, gate.qubit);
                prx_decomposed.insert(prx_decomposed.end(), expanded.begin(), expanded.end());
            }
            else if (gate.name_equals("SX"))
            {
                auto expanded = decomposeSxToPrx(gate.qubit);
                prx_decomposed.insert(prx_decomposed.end(), expanded.begin(), expanded.end());
            }
            else
            {
                prx_decomposed.push_back(gate);
            }
        }
        return prx_decomposed;
    };

    auto append_common_decomposition = [&](vector<Gate> &out, const Gate &g, bool inherit_metadata)
    {
        if (append_alias_or_native(out, g, g, inherit_metadata, preserve_logical_metadata, has_native_basis))
        {
            return;
        }

        if (g.name_equals("H"))
        {
            if (basis_has_prx)
            {
                append_gates(out, decomposeHToPrx(g.qubit), g, inherit_metadata);
            }
            else if (basis_has_rz && basis_has_sx)
            {
                append_gates(out, decomposeHadamardToRZSX(g.qubit), g, inherit_metadata);
            }
            else
            {
                append_gates(out, decomposeHadamard(g.qubit), g, inherit_metadata);
            }
        }
        else if (g.name_equals("T"))
        {
            append_gates(out, decomposeT(g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("Z"))
        {
            append_gates(out, decomposeZ(g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("TDG"))
        {
            append_gates(out, decomposeTdg(g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("Y"))
        {
            append_gates(out, basis_has_rz && basis_has_x ? decomposeYToRZX(g.qubit) : decomposeY(g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("S"))
        {
            append_gates(out, decomposeS(g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("SDG"))
        {
            append_gates(out, decomposeSdg(g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("RX"))
        {
            if (basis_has_prx)
            {
                append_gates(out, decomposeRxToPrx(g.theta, g.qubit), g, inherit_metadata);
            }
            else if (basis_has_rz && basis_has_sx)
            {
                append_gates(out, decomposeRxToRZSX(g.theta, g.qubit), g, inherit_metadata);
            }
            else
            {
                append_gates(out, decomposeRx(g.theta, g.qubit), g, inherit_metadata);
            }
        }
        else if (g.name_equals("PRX"))
        {
            append_gates(out, decomposePRX(g.theta, g.phi, g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("RY"))
        {
            if (basis_has_prx)
            {
                append_gates(out, decomposeRyToPrx(g.theta, g.qubit), g, inherit_metadata);
            }
            else if (basis_has_rz && basis_has_sx)
            {
                append_gates(out, decomposeRyToRZSX(g.theta, g.qubit), g, inherit_metadata);
            }
            else
            {
                append_gates(out, decomposeRy(g.theta, g.qubit), g, inherit_metadata);
            }
        }
        else if (g.name_equals("RI"))
        {
            append_gates(out, decomposeRI(g.theta, g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("SX"))
        {
            if (basis_has_prx)
            {
                append_gates(out, decomposeSxToPrx(g.qubit), g, inherit_metadata);
            }
            else
            {
                append_gate(out, g, g, inherit_metadata);
            }
        }
        else if (g.name_equals("X"))
        {
            if (basis_has_prx)
            {
                append_gates(out, decomposeXToPrx(g.qubit), g, inherit_metadata);
            }
            else
            {
                append_gate(out, g, g, inherit_metadata);
            }
        }
        else if (g.name_equals("P"))
        {
            append_gates(out, decomposeP(g.theta, g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("U"))
        {
            append_gates(out, decompose_u_for_basis(g), g, inherit_metadata);
        }
        else if (g.name_equals("CZ"))
        {
            append_gates(out, decomposeCZ(g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CY"))
        {
            append_gates(out, decomposeCY(g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CH"))
        {
            append_gates(out, decomposeCH(g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CS"))
        {
            append_gates(out, decomposeCS(g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CSDG"))
        {
            append_gates(out, decomposeCSDG(g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CT"))
        {
            append_gates(out, decomposeCT(g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CTDG"))
        {
            append_gates(out, decomposeCTDG(g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CRX"))
        {
            append_gates(out, decomposeCRX(g.theta, g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CRY"))
        {
            append_gates(out, decomposeCRY(g.theta, g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CRZ"))
        {
            append_gates(out, decomposeCRZ(g.theta, g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CSX"))
        {
            append_gates(out, decomposeCSX(g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CP"))
        {
            append_gates(out, decomposeCP(g.theta, g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("CU"))
        {
            append_gates(out, decomposeCU(g.theta, g.phi, g.lam, g.gamma, g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("RXX"))
        {
            append_gates(out, decomposeRXX(g.theta, g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("RYY"))
        {
            append_gates(out, decomposeRYY(g.theta, g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("RZZ"))
        {
            append_gates(out, decomposeRZZ(g.theta, g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("RZX"))
        {
            append_gates(out, decomposeRZX(g.theta, g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("SWAP"))
        {
            append_gates(out, decomposeSWAP(g.qubit, g.ctrl), g, inherit_metadata);
        }
        else if (g.name_equals("ECR"))
        {
            if (basis_has_ecr)
            {
                append_gate(out, g, g, inherit_metadata);
            }
            else
            {
                append_gates(out, decomposeECR(g.qubit, g.ctrl), g, inherit_metadata);
            }
        }
        else if (g.name_equals("CX"))
        {
            if (basis_has_ecr)
            {
                append_gates(out, decomposeCXToECR(g.ctrl, g.qubit), g, inherit_metadata);
            }
            else
            {
                append_gate(out, g, g, inherit_metadata);
            }
        }
        else if (g.name_equals("RZ"))
        {
            append_gate(out, BasicRZ(g.theta, g.qubit), g, inherit_metadata);
        }
        else if (g.name_equals("MA") || g.name_equals("ID") || g.name_equals("RESET"))
        {
            append_gate(out, g, g, inherit_metadata);
        }
        else
        {
            cout << "Error: cannot find this gate: " << endl;
            cout << "Gate " << g.name() << " not supported" << endl;
            append_gate(out, g, g, inherit_metadata);
        }
    };

    vector<Gate> decomposedGates;
    decomposedGates.reserve(circuit_gates.size());
    const bool first_pass_metadata = preserve_logical_metadata || mode != 0;
    for (const Gate &g : circuit_gates)
    {
        append_common_decomposition(decomposedGates, g, first_pass_metadata);
    }

    if (mode == 0)
    {
        circuit->set_gates(std::move(decomposedGates));
        return;
    }

    auto lower_backend =
        [&](const vector<Gate> &src,
            const std::function<bool(const std::string &)> &native_allowed,
            const std::function<vector<Gate>(const Gate &)> &lower_gate)
    {
        vector<Gate> out;
        out.reserve(src.size());
        for (const Gate &g : src)
        {
            if (append_alias_or_native(out, g, g, true, true, native_allowed))
            {
                continue;
            }
            append_gates(out, lower_gate(g), g, true);
        }
        return out;
    };

    if (mode == 1)
    {
        auto lower_ionq = [](const Gate &g)
        {
            vector<Gate> out;
            if (g.name_equals("RZ"))
            {
                out.push_back(BasicRZ(g.theta, g.qubit));
            }
            else if (g.name_equals("SX"))
            {
                out.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI / 2));
            }
            else if (g.name_equals("X"))
            {
                out.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI));
            }
            else if (g.name_equals("CX"))
            {
                out.push_back(Gate(OP::RY, g.qubit, -1, -1, 1, PI / 2));
                out.push_back(Gate(OP::RXX, g.qubit, g.ctrl, -1, 2, PI / 2));
                out.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, -PI / 2));
                out.push_back(Gate(OP::RX, g.ctrl, -1, -1, 1, -PI / 2));
                out.push_back(Gate(OP::RY, g.qubit, -1, -1, 1, -PI / 2));
            }
            return out;
        };
        circuit->set_gates(lower_backend(decomposedGates, has_native_basis, lower_ionq));
        return;
    }

    if (mode == 2)
    {
        auto lower_quantinuum = [](const Gate &g)
        {
            vector<Gate> out;
            if (g.name_equals("RZ"))
            {
                out.push_back(BasicRZ(g.theta, g.qubit));
            }
            else if (g.name_equals("SX"))
            {
                out.push_back(Gate(OP::U, g.qubit, -1, -1, 1, PI / 2));
            }
            else if (g.name_equals("X"))
            {
                out.push_back(Gate(OP::U, g.qubit, -1, -1, 1, PI));
            }
            else if (g.name_equals("CX"))
            {
                out.push_back(Gate(OP::U, g.qubit, -1, -1, 1, -PI / 2, PI / 2));
                out.push_back(Gate(OP::ZZ, g.qubit, g.ctrl, -1, 2, PI / 2));
                out.push_back(Gate(OP::RZ, g.ctrl, -1, -1, 1, -PI / 2));
                out.push_back(Gate(OP::U, g.qubit, -1, -1, 1, PI / 2, PI));
                out.push_back(Gate(OP::RZ, g.ctrl, -1, -1, 1, -PI / 2));
            }
            return out;
        };
        circuit->set_gates(lower_backend(decomposedGates, has_native_basis, lower_quantinuum));
        return;
    }

    if (mode == 3)
    {
        auto rigetti_native = [&](const std::string &gate_name)
        {
            return has_native_basis(gate_name) &&
                   gate_name != "rx" && gate_name != "ry" && gate_name != "prx" &&
                   gate_name != "sx" && gate_name != "x";
        };
        auto lower_rigetti = [&](const Gate &g)
        {
            vector<Gate> out;
            if (g.name_equals("RZ"))
            {
                if (basis_has_prx)
                {
                    out = decomposeRzToPrxOnly(g.theta, g.qubit);
                }
                else
                {
                    out.push_back(BasicRZ(g.theta, g.qubit));
                }
            }
            else if (g.name_equals("RX"))
            {
                out = decomposeRxToFixedRx(g.theta, g.qubit);
            }
            else if (g.name_equals("RY"))
            {
                out = decomposeRyToFixedRx(g.theta, g.qubit);
            }
            else if (g.name_equals("PRX"))
            {
                if (basis_has_prx)
                {
                    out.push_back(g);
                }
                else
                {
                    out = decomposePrxToFixedRx(g.theta, g.phi, g.qubit);
                }
            }
            else if (g.name_equals("SX"))
            {
                out.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI / 2));
            }
            else if (g.name_equals("X"))
            {
                out.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI));
            }
            else if (g.name_equals("CX"))
            {
                if (has_native_basis("iswap"))
                {
                    out = decomposeCXToISWAP(g.ctrl, g.qubit, basis_has_prx);
                }
                else
                {
                    vector<Gate> had = basis_has_prx ? decomposeHToPrx(g.qubit) : decomposeHadamard(g.qubit);
                    out.insert(out.end(), had.begin(), had.end());
                    out.push_back(Gate(OP::CZ, g.qubit, g.ctrl, 2));
                    out.insert(out.end(), had.begin(), had.end());
                }
            }
            return out;
        };
        circuit->set_gates(lower_backend(decomposedGates, rigetti_native, lower_rigetti));
        return;
    }

    if (mode == 4)
    {
        auto lower_quafu = [](const Gate &g)
        {
            vector<Gate> out;
            if (g.name_equals("RZ"))
            {
                out.push_back(BasicRZ(g.theta, g.qubit));
            }
            else if (g.name_equals("RX"))
            {
                out = decomposeRx(g.theta, g.qubit);
            }
            else if (g.name_equals("PRX"))
            {
                out = decomposePRX(g.theta, g.phi, g.qubit);
            }
            else if (g.name_equals("SX"))
            {
                out.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI / 2));
            }
            else if (g.name_equals("X"))
            {
                out.push_back(Gate(OP::RX, g.qubit, -1, -1, 1, PI));
            }
            else if (g.name_equals("CX"))
            {
                out.push_back(Gate(OP::H, g.qubit));
                out.push_back(Gate(OP::CZ, g.qubit, g.ctrl, 2));
                out.push_back(Gate(OP::H, g.qubit));
            }
            return out;
        };
        circuit->set_gates(lower_backend(decomposedGates, has_native_basis, lower_quafu));
        return;
    }

    if (mode == 5)
    {
        vector<Gate> decomposedGates_IQM;
        std::function<void(const Gate &, const Gate &)> append_prx_only;
        auto expand_iqm_gate = [](const Gate &gate)
        {
            vector<Gate> out;
            switch (gate.op_name)
            {
            case OP::H:
                return decomposeHToPrx(gate.qubit);
            case OP::Z:
                return decomposeZ(gate.qubit);
            case OP::S:
                return decomposeS(gate.qubit);
            case OP::SDG:
                return decomposeSdg(gate.qubit);
            case OP::T:
                return decomposeT(gate.qubit);
            case OP::TDG:
                return decomposeTdg(gate.qubit);
            case OP::P:
                return decomposeP(gate.theta, gate.qubit);
            case OP::U:
                return decomposeU(gate.theta, gate.phi, gate.lam, gate.qubit);
            case OP::CZ:
                return decomposeCZ(gate.qubit, gate.ctrl);
            case OP::CY:
                return decomposeCY(gate.qubit, gate.ctrl);
            case OP::CH:
                return decomposeCH(gate.qubit, gate.ctrl);
            case OP::CS:
                return decomposeCS(gate.qubit, gate.ctrl);
            case OP::CSDG:
                return decomposeCSDG(gate.qubit, gate.ctrl);
            case OP::CT:
                return decomposeCT(gate.qubit, gate.ctrl);
            case OP::CTDG:
                return decomposeCTDG(gate.qubit, gate.ctrl);
            case OP::CRX:
                return decomposeCRX(gate.theta, gate.qubit, gate.ctrl);
            case OP::CRY:
                return decomposeCRY(gate.theta, gate.qubit, gate.ctrl);
            case OP::CRZ:
                return decomposeCRZ(gate.theta, gate.qubit, gate.ctrl);
            case OP::CSX:
                return decomposeCSX(gate.qubit, gate.ctrl);
            case OP::CP:
                return decomposeCP(gate.theta, gate.qubit, gate.ctrl);
            case OP::CU:
                return decomposeCU(gate.theta, gate.phi, gate.lam, gate.gamma, gate.qubit, gate.ctrl);
            case OP::RXX:
                return decomposeRXX(gate.theta, gate.qubit, gate.ctrl);
            case OP::RYY:
                return decomposeRYY(gate.theta, gate.qubit, gate.ctrl);
            case OP::RZZ:
                return decomposeRZZ(gate.theta, gate.qubit, gate.ctrl);
            case OP::SWAP:
                return decomposeSWAP(gate.qubit, gate.ctrl);
            default:
                return out;
            }
        };

        append_prx_only = [&](const Gate &source, const Gate &gate)
        {
            if (gate.name_equals("PRX"))
            {
                append_gate(decomposedGates_IQM, gate, source, true);
            }
            else if (gate.name_equals("CX"))
            {
                append_gate(decomposedGates_IQM, gate, source, true);
            }
            else if (gate.name_equals("RX"))
            {
                append_gate(decomposedGates_IQM, Gate(OP::PRX, gate.qubit, -1, -1, 1, gate.theta, 0), source, true);
            }
            else if (gate.name_equals("RY"))
            {
                append_gate(decomposedGates_IQM, Gate(OP::PRX, gate.qubit, -1, -1, 1, gate.theta, PI / 2), source, true);
            }
            else if (gate.name_equals("RZ"))
            {
                append_gates(decomposedGates_IQM, decomposeRzToPrxOnly(gate.theta, gate.qubit), source, true);
            }
            else if (gate.name_equals("SX"))
            {
                append_gate(decomposedGates_IQM, Gate(OP::PRX, gate.qubit, -1, -1, 1, PI / 2, 0), source, true);
            }
            else if (gate.name_equals("X"))
            {
                append_gate(decomposedGates_IQM, Gate(OP::PRX, gate.qubit, -1, -1, 1, PI, 0), source, true);
            }
            else
            {
                vector<Gate> expanded = expand_iqm_gate(gate);
                if (expanded.empty())
                {
                    append_gate(decomposedGates_IQM, gate, source, true);
                    return;
                }
                for (const Gate &expanded_gate : expanded)
                {
                    append_prx_only(source, expanded_gate);
                }
            }
        };

        for (const Gate &g : decomposedGates)
        {
            append_prx_only(g, g);
        }
        circuit->set_gates(std::move(decomposedGates_IQM));
    }
}
