#pragma once

#include "../QASMTransPrimitives.hpp"
#include "../IR/gate.hpp"
#include "../IR/circuit.hpp"
#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include <iostream>

// ===========================
//  NAMESPACE & UTILITY
// ===========================
namespace QASMTrans {

/**
 * @brief Namespace holding inline decomposition functions
 * 
 * Each function takes the necessary parameters (e.g., angles, qubit indices)
 * and returns a vector of newly decomposed gates.
 */
namespace DecompositionUtils {

    // ------------------------------------
    // Basic Helpers
    // ------------------------------------
    inline Gate BasicRZ(ValType theta, IdxType qubit) {
        return Gate(OP::RZ, qubit, -1, -1, 1, theta);
    }
    inline Gate BasicSX(IdxType qubit) {
        return Gate(OP::SX, qubit);
    }
    inline Gate BasicX(IdxType qubit) {
        return Gate(OP::X, qubit);
    }
    inline Gate BasicCX(IdxType ctrl, IdxType qubit) {
        return Gate(OP::CX, qubit, ctrl, -1, 2);
    }

    // ------------------------------------
    // Single-Qubit Decompositions
    // ------------------------------------
    inline std::vector<Gate> decomposeHadamard(IdxType qubit) {
        // H = X -> SX -> RZ(-pi/2) -> SX -> X
        return {
            BasicX(qubit),
            BasicSX(qubit),
            BasicRZ(-PI / 2, qubit),
            BasicSX(qubit),
            BasicX(qubit)
        };
    }
    inline std::vector<Gate> decomposeT(IdxType qubit) {
        // T = RZ(pi/4)
        return { BasicRZ(PI / 4, qubit) };
    }
    inline std::vector<Gate> decomposeTdg(IdxType qubit) {
        // T^\dagger = RZ(-pi/4)
        return { BasicRZ(-PI / 4, qubit) };
    }
    inline std::vector<Gate> decomposeS(IdxType qubit) {
        // S = RZ(pi/2)
        return { BasicRZ(PI / 2, qubit) };
    }
    inline std::vector<Gate> decomposeSdg(IdxType qubit) {
        // S^\dagger = RZ(-pi/2)
        return { BasicRZ(-PI / 2, qubit) };
    }
    inline std::vector<Gate> decomposeZ(IdxType qubit) {
        // Z = RZ(pi)
        return { BasicRZ(PI, qubit) };
    }
    inline std::vector<Gate> decomposeY(IdxType qubit) {
        // Y = SX -> Z -> SX -> SX -> SX
        std::vector<Gate> gates { BasicSX(qubit) };
        auto z_decomp = decomposeZ(qubit); // RZ(pi)
        gates.insert(gates.end(), z_decomp.begin(), z_decomp.end());
        // Now 3 x SX
        gates.push_back(BasicSX(qubit));
        gates.push_back(BasicSX(qubit));
        gates.push_back(BasicSX(qubit));
        return gates;
    }
    inline std::vector<Gate> decomposeRx(ValType theta, IdxType qubit) {
        // RX(theta) = H -> RZ(theta) -> H
        auto had = decomposeHadamard(qubit);
        std::vector<Gate> gates = had;
        gates.push_back(BasicRZ(theta, qubit));
        gates.insert(gates.end(), had.begin(), had.end());
        return gates;
    }
    inline std::vector<Gate> decomposeRy(ValType theta, IdxType qubit) {
        // RY(theta) = SX -> RZ(theta) -> SX -> SX -> SX
        return {
            BasicSX(qubit),
            BasicRZ(theta, qubit),
            BasicSX(qubit),
            BasicSX(qubit),
            BasicSX(qubit)
        };
    }
    inline std::vector<Gate> decomposeRI(ValType theta, IdxType qubit) {
        // RI(theta) = RZ(2*theta) -> Z
        // Actually your snippet calls decomposeZ, but let's keep it as a direct usage
        // Implementation from the snippet:
        //   RZ(2*theta) -> decomposeZ -> appended
        std::vector<Gate> gates { BasicRZ(2 * theta, qubit) };
        auto z_decomp = decomposeZ(qubit); // RZ(pi)
        gates.insert(gates.end(), z_decomp.begin(), z_decomp.end());
        return gates;
    }
    inline std::vector<Gate> decomposeP(ValType theta, IdxType qubit) {
        // P(theta) = RZ(theta)
        return { BasicRZ(theta, qubit) };
    }

    // ------------------------------------
    // U-gate Decomposition
    // ------------------------------------
    inline std::vector<Gate> decomposeU(ValType theta, ValType phi, ValType lam, IdxType qubit) {
        // Implementation from snippet:
        //   if (lam != 0) -> RZ(lam)
        //   SX
        //   RZ(theta + pi)
        //   SX
        //   RZ(3*pi + phi)
        // This is a simplified form; your snippet had these lines:
        std::vector<Gate> gates;
        if (lam != 0) {
            gates.push_back(BasicRZ(lam, qubit));
        }
        gates.push_back(BasicSX(qubit));
        gates.push_back(BasicRZ(theta + PI, qubit));
        gates.push_back(BasicSX(qubit));
        gates.push_back(BasicRZ(3 * PI + phi, qubit));
        return gates;
    }

    // ------------------------------------
    // Two-Qubit Decompositions
    // ------------------------------------
    inline std::vector<Gate> decomposeCZ(IdxType tgt, IdxType ctrl) {
        // from snippet: hadamard on target -> CX -> hadamard
        // Actually snippet used decomposeHadamard(tgt) around a CX
        // We'll replicate that:
        auto had = decomposeHadamard(tgt);
        std::vector<Gate> gates;
        gates.insert(gates.end(), had.begin(), had.end());
        gates.push_back(BasicCX(ctrl, tgt));
        gates.insert(gates.end(), had.begin(), had.end());
        return gates;
    }
    inline std::vector<Gate> decomposeCY(IdxType tgt, IdxType ctrl) {
        // from snippet: RZ(-pi/2, tgt) -> CX(ctrl, tgt) -> RZ(pi/2, tgt)
        return {
            BasicRZ(-PI / 2, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(PI / 2, tgt)
        };
    }
    inline std::vector<Gate> decomposeCH(IdxType tgt, IdxType ctrl) {
        // from snippet
        // RZ(-pi, tgt), SX(tgt), RZ(3*pi/4, tgt), CX(ctrl, tgt),
        // RZ(pi/4, tgt), SX(tgt)
        return {
            BasicRZ(-PI, tgt),
            BasicSX(tgt),
            BasicRZ(3 * PI / 4, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(PI / 4, tgt),
            BasicSX(tgt)
        };
    }
    inline std::vector<Gate> decomposeCS(IdxType tgt, IdxType ctrl) {
        // from snippet: RZ(pi/4, ctrl), CX(ctrl, tgt),
        // RZ(-pi/4, tgt), CX(ctrl, tgt), RZ(pi/4, tgt)
        return {
            BasicRZ(PI / 4, ctrl),
            BasicCX(ctrl, tgt),
            BasicRZ(-PI / 4, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(PI / 4, tgt)
        };
    }
    inline std::vector<Gate> decomposeCSDG(IdxType tgt, IdxType ctrl) {
        // from snippet
        // Many steps. We'll copy exactly:
        // RZ(pi/2, tgt), SX(tgt), RZ(pi/2, tgt), CX(ctrl, tgt), ...
        // ...
        return {
            BasicRZ(PI / 2, tgt),
            BasicSX(tgt),
            BasicRZ(PI / 2, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(PI / 2, tgt),
            BasicRZ(PI / 4, ctrl),
            BasicSX(tgt),
            BasicRZ(PI / 2, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(-PI / 4, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(PI / 4, tgt)
        };
    }
    inline std::vector<Gate> decomposeCT(IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicRZ(PI / 8, ctrl),
            BasicCX(ctrl, tgt),
            BasicRZ(-PI / 8, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(PI / 8, tgt)
        };
    }
    inline std::vector<Gate> decomposeCTdg(IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicRZ(-PI / 8, ctrl),
            BasicCX(ctrl, tgt),
            BasicRZ(PI / 8, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(-PI / 8, tgt)
        };
    }
    inline std::vector<Gate> decomposeCRX(ValType theta, IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicRZ(PI / 2, tgt),
            BasicSX(tgt),
            BasicRZ(PI / 2, tgt),
            BasicRZ(theta / 2, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(-theta / 2, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(PI / 2, tgt),
            BasicSX(tgt),
            BasicRZ(PI / 2, tgt)
        };
    }
    inline std::vector<Gate> decomposeCRY(ValType theta, IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicSX(tgt),
            BasicRZ(PI + theta / 2, tgt),
            BasicSX(tgt),
            BasicRZ(3 * PI, tgt),
            BasicCX(ctrl, tgt),
            BasicSX(tgt),
            BasicRZ(PI - theta / 2, tgt),
            BasicSX(tgt),
            BasicRZ(3 * PI, tgt),
            BasicCX(ctrl, tgt)
        };
    }
    inline std::vector<Gate> decomposeCRZ(ValType theta, IdxType tgt, IdxType ctrl) {
        // from snippet:
        // RZ(theta/2, tgt), CX(ctrl, tgt), RZ(-theta/2, tgt), CX(ctrl, tgt)
        return {
            BasicRZ(theta / 2, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(-theta / 2, tgt),
            BasicCX(ctrl, tgt)
        };
    }
    inline std::vector<Gate> decomposeCSX(IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicRZ(PI / 2, tgt),
            BasicRZ(PI / 4, ctrl),
            BasicSX(tgt),
            BasicRZ(PI / 2, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(-PI / 4, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(3 * PI / 4, tgt),
            BasicSX(tgt),
            BasicRZ(PI / 2, tgt)
        };
    }
    inline std::vector<Gate> decomposeCP(ValType theta, IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicRZ(theta / 2, ctrl),
            BasicCX(ctrl, tgt),
            BasicRZ(-theta / 2, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(theta / 2, tgt)
        };
    }
    inline std::vector<Gate> decomposeCU(ValType theta, ValType phi, ValType lam, ValType gamma,
                                        IdxType tgt, IdxType ctrl)
    {
        // from snippet
        return {
            BasicRZ(gamma, ctrl),
            BasicRZ(lam / 2 + phi / 2, ctrl),
            BasicRZ(lam / 2 - phi / 2, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(-lam / 2 - phi / 2, tgt),
            BasicSX(tgt),
            BasicRZ(PI - theta / 2, tgt),
            BasicSX(tgt),
            BasicRZ(3 * PI, tgt),
            BasicCX(ctrl, tgt),
            BasicSX(tgt),
            BasicRZ(PI + theta / 2, tgt),
            BasicSX(tgt),
            BasicRZ(3 * PI + phi, tgt)
        };
    }

    // ------------------------------------
    // Multi-Qubit Decompositions
    // ------------------------------------
    inline std::vector<Gate> decomposeSWAP(IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicCX(ctrl, tgt),
            BasicCX(tgt, ctrl),
            BasicCX(ctrl, tgt)
        };
    }
    inline std::vector<Gate> decomposeRXX(ValType theta, IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicRZ(PI / 2, tgt),
            BasicSX(tgt),
            BasicRZ(PI / 2, tgt),
            BasicRZ(PI / 2, ctrl),
            BasicSX(ctrl),
            BasicRZ(PI / 2, ctrl),
            BasicCX(ctrl, tgt),
            BasicRZ(theta, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(PI / 2, tgt),
            BasicSX(tgt),
            BasicRZ(PI / 2, tgt),
            BasicRZ(PI / 2, ctrl),
            BasicSX(ctrl),
            BasicRZ(PI / 2, ctrl)
        };
    }
    inline std::vector<Gate> decomposeRYY(ValType theta, IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicSX(tgt),
            BasicSX(ctrl),
            BasicCX(ctrl, tgt),
            BasicRZ(theta, tgt),
            BasicCX(ctrl, tgt),
            BasicRZ(-PI, tgt),
            BasicSX(tgt),
            BasicRZ(-PI, tgt),
            BasicRZ(-PI, ctrl),
            BasicSX(ctrl),
            BasicRZ(-PI, ctrl)
        };
    }
    inline std::vector<Gate> decomposeRZZ(ValType theta, IdxType tgt, IdxType ctrl) {
        // from snippet
        return {
            BasicCX(ctrl, tgt),
            BasicRZ(theta, tgt),
            BasicCX(ctrl, tgt)
        };
    }
    inline std::vector<Gate> decomposeCCX(IdxType a, IdxType b, IdxType c) {
        // from snippet
        // gate ccx a,b,c
        // {
        //   h c; cx b,c; tdg c; cx a,c; t c; cx b,c; tdg c; cx a,c; t b; t c; h c;
        //   cx a,b; t a; tdg b; cx a,b;
        // }
        std::vector<Gate> gates;
        gates.emplace_back(Gate(OP::H, c));
        gates.push_back(BasicCX(b, c));
        gates.emplace_back(Gate(OP::TDG, c));
        gates.push_back(BasicCX(a, c));
        gates.emplace_back(Gate(OP::T, c));
        gates.push_back(BasicCX(b, c));
        gates.emplace_back(Gate(OP::TDG, c));
        gates.push_back(BasicCX(a, c));
        gates.emplace_back(Gate(OP::T, b));
        gates.emplace_back(Gate(OP::T, c));
        gates.emplace_back(Gate(OP::H, c));
        gates.push_back(BasicCX(a, b));
        gates.emplace_back(Gate(OP::T, a));
        gates.emplace_back(Gate(OP::TDG, b));
        gates.push_back(BasicCX(a, b));
        return gates;
    }
    inline std::vector<Gate> decomposeRCCX(IdxType a, IdxType b, IdxType c) {
        // from snippet
        return {
            Gate(OP::U, c, -1, -1, 1, PI / 2, 0, PI),
            Gate(OP::U, c, -1, -1, 1, 0, 0, PI / 4),
            BasicCX(b, c),
            Gate(OP::U, c, -1, -1, 1, 0, 0, -PI / 4),
            BasicCX(a, c),
            Gate(OP::U, c, -1, -1, 1, 0, 0, PI / 4),
            BasicCX(b, c),
            Gate(OP::U, c, -1, -1, 1, 0, 0, -PI / 4),
            Gate(OP::U, c, -1, -1, 1, PI / 2, 0, PI)
        };
    }
    inline std::vector<Gate> decomposeCSWAP(IdxType a, IdxType b, IdxType c) {
        // from snippet
        // push_back(CX(c,b)); then CCX(a,b,c); then CX(c,b)
        std::vector<Gate> gates;
        gates.push_back(BasicCX(c, b));
        auto ccx = decomposeCCX(a, b, c);
        gates.insert(gates.end(), ccx.begin(), ccx.end());
        gates.push_back(BasicCX(c, b));
        return gates;
    }

} // namespace DecompositionUtils

// =============================
//   DECOMPOSER CLASS
// =============================
/**
 * @brief A class that maps gate names (string) to a function that performs
 *        the relevant decomposition.
 *
 * Each entry in the map returns a vector of Gates that replace the original Gate.
 */
class Decomposer {
private:
    std::unordered_map<std::string, std::function<std::vector<Gate>(const Gate&)>> gateHandlers;

public:
    Decomposer() {
        using namespace DecompositionUtils;

        // Register single-qubit gate decompositions
        gateHandlers["H"]   = [](const Gate &g){ return decomposeHadamard(g.qubit); };
        gateHandlers["T"]   = [](const Gate &g){ return decomposeT(g.qubit); };
        gateHandlers["TDG"] = [](const Gate &g){ return decomposeTdg(g.qubit); };
        gateHandlers["S"]   = [](const Gate &g){ return decomposeS(g.qubit); };
        gateHandlers["SDG"] = [](const Gate &g){ return decomposeSdg(g.qubit); };
        gateHandlers["Z"]   = [](const Gate &g){ return decomposeZ(g.qubit); };
        gateHandlers["Y"]   = [](const Gate &g){ return decomposeY(g.qubit); };
        gateHandlers["RX"]  = [](const Gate &g){ return decomposeRx(g.theta, g.qubit); };
        gateHandlers["RY"]  = [](const Gate &g){ return decomposeRy(g.theta, g.qubit); };
        gateHandlers["RI"]  = [](const Gate &g){ return decomposeRI(g.theta, g.qubit); };
        gateHandlers["P"]   = [](const Gate &g){ return decomposeP(g.theta, g.qubit); };
        gateHandlers["U"]   = [](const Gate &g){ return decomposeU(g.theta, g.phi, g.lam, g.qubit); };

        // Register two-qubit gate decompositions
        gateHandlers["CZ"]  = [](const Gate &g){ return decomposeCZ(g.qubit, g.ctrl); };
        gateHandlers["CY"]  = [](const Gate &g){ return decomposeCY(g.qubit, g.ctrl); };
        gateHandlers["CH"]  = [](const Gate &g){ return decomposeCH(g.qubit, g.ctrl); };
        gateHandlers["CS"]  = [](const Gate &g){ return decomposeCS(g.qubit, g.ctrl); };
        gateHandlers["CSDG"] = [](const Gate &g){ return decomposeCSDG(g.qubit, g.ctrl); };
        gateHandlers["CT"]  = [](const Gate &g){ return decomposeCT(g.qubit, g.ctrl); };
        gateHandlers["CTDG"] = [](const Gate &g){ return decomposeCTdg(g.qubit, g.ctrl); };
        gateHandlers["CRX"] = [](const Gate &g){ return decomposeCRX(g.theta, g.qubit, g.ctrl); };
        gateHandlers["CRY"] = [](const Gate &g){ return decomposeCRY(g.theta, g.qubit, g.ctrl); };
        gateHandlers["CRZ"] = [](const Gate &g){ return decomposeCRZ(g.theta, g.qubit, g.ctrl); };
        gateHandlers["CSX"] = [](const Gate &g){ return decomposeCSX(g.qubit, g.ctrl); };
        gateHandlers["CP"]  = [](const Gate &g){ return decomposeCP(g.theta, g.qubit, g.ctrl); };
        gateHandlers["CU"]  = [](const Gate &g){ return decomposeCU(g.theta, g.phi, g.lam, g.gamma, g.qubit, g.ctrl); };

        // Parametric 2-qubit gates
        gateHandlers["RXX"] = [](const Gate &g){ return decomposeRXX(g.theta, g.qubit, g.ctrl); };
        gateHandlers["RYY"] = [](const Gate &g){ return decomposeRYY(g.theta, g.qubit, g.ctrl); };
        gateHandlers["RZZ"] = [](const Gate &g){ return decomposeRZZ(g.theta, g.qubit, g.ctrl); };

        // SWAP
        gateHandlers["SWAP"] = [](const Gate &g){ return decomposeSWAP(g.qubit, g.ctrl); };

        // Multi-qubit gates
        gateHandlers["CCX"]   = [](const Gate &g){ return DecompositionUtils::decomposeCCX(g.qubit, g.ctrl, g.extra); };
        gateHandlers["RCCX"]  = [](const Gate &g){ return DecompositionUtils::decomposeRCCX(g.qubit, g.ctrl, g.extra); };
        gateHandlers["CSWAP"] = [](const Gate &g){ return DecompositionUtils::decomposeCSWAP(g.qubit, g.ctrl, g.extra); };
    }

    /**
     * @brief Decompose a single Gate by looking up its handler.
     *
     * If the gate is not found in the registry, return it as-is (no decomposition).
     */
    std::vector<Gate> decomposeGate(const Gate &gate) const {
        const std::string gName = OP_NAMES[gate.op_name];
        auto it = gateHandlers.find(gName);
        if (it != gateHandlers.end()) {
            return it->second(gate);
        }
        // If no handler, just return the original gate
        return { gate };
    }
};

// =========================================
//   PASS TO DECOMPOSE THREE-QUBIT GATES
// =========================================
/**
 * @brief Optionally, you can run this pass if you want to break down
 *        any found three-qubit gates (like CCX, CSWAP, etc.) into
 *        multiple two-qubit gates. If your platform can’t handle
 *        three-qubit gates natively, run this before the main Decompose().
 */
void Decompose_three_to_two(std::shared_ptr<Circuit> circuit) {
    std::vector<Gate> origGates = circuit->get_gates();
    std::vector<Gate> finalGates;
    finalGates.reserve(origGates.size()); // small optimization

    for (auto &g : origGates) {
        if (g.n_qubits > 2) {
            // This block can handle or further break down three-qubit gates
            const std::string gName = OP_NAMES[g.op_name];
            if (gName == "CSWAP") {
                auto sub = DecompositionUtils::decomposeCSWAP(g.qubit, g.ctrl, g.extra);
                finalGates.insert(finalGates.end(), sub.begin(), sub.end());
            }
            else if (gName == "CCX") {
                auto sub = DecompositionUtils::decomposeCCX(g.qubit, g.ctrl, g.extra);
                finalGates.insert(finalGates.end(), sub.begin(), sub.end());
            }
            else if (gName == "RCCX") {
                auto sub = DecompositionUtils::decomposeRCCX(g.qubit, g.ctrl, g.extra);
                finalGates.insert(finalGates.end(), sub.begin(), sub.end());
            }
            else {
                // If there's any other 3-qubit gate unaccounted for
                finalGates.push_back(g);
            }
        } else {
            // Pass through as-is
            finalGates.push_back(g);
        }
    }
    circuit->set_gates(finalGates);
}

// ======================================
//   MAIN DECOMPOSE FUNCTION
// ======================================
/**
 * @brief Decompose known gates in a circuit to a simpler gate set.
 *
 * @param circuit The circuit to be decomposed
 * @param mode    A hardware-specific mode (0=IBM, 1=IonQ, 2=Quantinuum, etc.)
 *                For demonstration, you can adapt to run additional passes.
 */
void Decompose(std::shared_ptr<Circuit> circuit, IdxType mode) {
    // 1) Optionally break down 3-qubit gates into multiple 2-qubit gates
    //    if your hardware can’t handle them natively:
    // Decompose_three_to_two(circuit);

    // 2) Decompose all gates that have known expansions
    Decomposer decomposer;
    std::vector<Gate> oldGates = circuit->get_gates();
    std::vector<Gate> newGates;
    for (auto &g : oldGates) {
        auto expanded = decomposer.decomposeGate(g);
        newGates.insert(newGates.end(), expanded.begin(), expanded.end());
    }
    circuit->set_gates(newGates);

    // 3) (Optional) For different modes, do a second pass
    //    that transforms certain gates into platform-specific primitives.
    if (mode == 0) {
        // IBM default mode, do nothing special
        return;
    } else if (mode == 1) {
        // IonQ example: transform SX -> RX(pi/2), etc.
        // This is up to you; replicate your original snippet’s “mode” logic as needed.
    } else if (mode == 2) {
        // Quantinuum example, etc.
    }
    // ... Additional modes...
}

} // end namespace QASMTrans