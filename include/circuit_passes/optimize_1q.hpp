#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <initializer_list>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../IR/chip.hpp"
#include "../IR/circuit.hpp"
#include "../IR/gate.hpp"
#include "../QASMTransPrimitives.hpp"

namespace QASMTrans
{
    namespace optimize
    {
        namespace detail
        {
            constexpr double kAngleEps = 1e-12;

            struct Mat2
            {
                std::complex<double> m[2][2];
            };

            inline Mat2 identity()
            {
                Mat2 out{};
                out.m[0][0] = {1.0, 0.0};
                out.m[0][1] = {0.0, 0.0};
                out.m[1][0] = {0.0, 0.0};
                out.m[1][1] = {1.0, 0.0};
                return out;
            }

            inline Mat2 matmul(const Mat2 &a, const Mat2 &b)
            {
                Mat2 out{};
                out.m[0][0] = a.m[0][0] * b.m[0][0] + a.m[0][1] * b.m[1][0];
                out.m[0][1] = a.m[0][0] * b.m[0][1] + a.m[0][1] * b.m[1][1];
                out.m[1][0] = a.m[1][0] * b.m[0][0] + a.m[1][1] * b.m[1][0];
                out.m[1][1] = a.m[1][0] * b.m[0][1] + a.m[1][1] * b.m[1][1];
                return out;
            }

            inline Mat2 rz_matrix(double theta)
            {
                const std::complex<double> i(0.0, 1.0);
                const std::complex<double> phase = std::exp(-i * theta / 2.0);
                Mat2 out{};
                out.m[0][0] = phase;
                out.m[0][1] = {0.0, 0.0};
                out.m[1][0] = {0.0, 0.0};
                out.m[1][1] = std::conj(phase);
                return out;
            }

            inline Mat2 rx_matrix(double theta)
            {
                const std::complex<double> i(0.0, 1.0);
                const double c = std::cos(theta / 2.0);
                const double s = std::sin(theta / 2.0);
                Mat2 out{};
                out.m[0][0] = {c, 0.0};
                out.m[0][1] = -i * s;
                out.m[1][0] = -i * s;
                out.m[1][1] = {c, 0.0};
                return out;
            }

            inline Mat2 ry_matrix(double theta)
            {
                const double c = std::cos(theta / 2.0);
                const double s = std::sin(theta / 2.0);
                Mat2 out{};
                out.m[0][0] = {c, 0.0};
                out.m[0][1] = {-s, 0.0};
                out.m[1][0] = {s, 0.0};
                out.m[1][1] = {c, 0.0};
                return out;
            }

            inline Mat2 phase_matrix(double theta)
            {
                const std::complex<double> i(0.0, 1.0);
                Mat2 out{};
                out.m[0][0] = {1.0, 0.0};
                out.m[0][1] = {0.0, 0.0};
                out.m[1][0] = {0.0, 0.0};
                out.m[1][1] = std::exp(i * theta);
                return out;
            }

            inline Mat2 u_matrix(double theta, double phi, double lam)
            {
                const std::complex<double> i(0.0, 1.0);
                const double c = std::cos(theta / 2.0);
                const double s = std::sin(theta / 2.0);
                const std::complex<double> e_phi = std::exp(i * phi);
                const std::complex<double> e_lam = std::exp(i * lam);
                const std::complex<double> e_phi_lam = std::exp(i * (phi + lam));
                Mat2 out{};
                out.m[0][0] = {c, 0.0};
                out.m[0][1] = -e_lam * s;
                out.m[1][0] = e_phi * s;
                out.m[1][1] = e_phi_lam * c;
                return out;
            }

            inline Mat2 prx_matrix(double theta, double phi)
            {
                return matmul(rz_matrix(phi), matmul(rx_matrix(theta), rz_matrix(-phi)));
            }

            inline bool gate_matrix(const Gate &gate, Mat2 &out)
            {
                switch (gate.op_name)
                {
                case OP::ID:
                    out = identity();
                    return true;
                case OP::X:
                    out = rx_matrix(PI);
                    return true;
                case OP::Y:
                    out = matmul(rz_matrix(PI / 2.0), matmul(rx_matrix(PI), rz_matrix(-PI / 2.0)));
                    return true;
                case OP::Z:
                    out = rz_matrix(PI);
                    return true;
                case OP::H:
                {
                    const double inv_sqrt2 = 1.0 / std::sqrt(2.0);
                    Mat2 h{};
                    h.m[0][0] = {inv_sqrt2, 0.0};
                    h.m[0][1] = {inv_sqrt2, 0.0};
                    h.m[1][0] = {inv_sqrt2, 0.0};
                    h.m[1][1] = {-inv_sqrt2, 0.0};
                    out = h;
                    return true;
                }
                case OP::S:
                    out = phase_matrix(PI / 2.0);
                    return true;
                case OP::SDG:
                    out = phase_matrix(-PI / 2.0);
                    return true;
                case OP::T:
                    out = phase_matrix(PI / 4.0);
                    return true;
                case OP::TDG:
                    out = phase_matrix(-PI / 4.0);
                    return true;
                case OP::RI:
                    out = phase_matrix(gate.theta);
                    return true;
                case OP::RX:
                    out = rx_matrix(gate.theta);
                    return true;
                case OP::RY:
                    out = ry_matrix(gate.theta);
                    return true;
                case OP::RZ:
                    out = rz_matrix(gate.theta);
                    return true;
                case OP::SX:
                    out = rx_matrix(PI / 2.0);
                    return true;
                case OP::P:
                    out = phase_matrix(gate.theta);
                    return true;
                case OP::U:
                    out = u_matrix(gate.theta, gate.phi, gate.lam);
                    return true;
                case OP::PRX:
                    out = prx_matrix(gate.theta, gate.phi);
                    return true;
                default:
                    break;
                }
                return false;
            }

            inline Mat2 normalize_global_phase(const Mat2 &mat)
            {
                const std::complex<double> det = mat.m[0][0] * mat.m[1][1] - mat.m[0][1] * mat.m[1][0];
                if (std::abs(det) < kAngleEps)
                {
                    return mat;
                }
                const double phase = 0.5 * std::arg(det);
                const std::complex<double> factor = std::exp(std::complex<double>(0.0, -phase));
                Mat2 out{};
                out.m[0][0] = mat.m[0][0] * factor;
                out.m[0][1] = mat.m[0][1] * factor;
                out.m[1][0] = mat.m[1][0] * factor;
                out.m[1][1] = mat.m[1][1] * factor;
                return out;
            }

            inline double mod_2pi(double angle, double atol)
            {
                double wrapped = std::fmod(angle + PI, 2.0 * PI);
                if (wrapped < 0.0)
                {
                    wrapped += 2.0 * PI;
                }
                wrapped -= PI;
                if (std::abs(wrapped - PI) < atol)
                {
                    return -PI;
                }
                return wrapped;
            }

            inline bool near_zero(double value, double atol)
            {
                return std::abs(value) < atol;
            }

            inline void params_zyz(const Mat2 &mat, double &theta, double &phi, double &lam)
            {
                const std::complex<double> det = mat.m[0][0] * mat.m[1][1] - mat.m[0][1] * mat.m[1][0];
                const double det_arg = std::arg(det);
                theta = 2.0 * std::atan2(std::abs(mat.m[1][0]), std::abs(mat.m[0][0]));
                const double ang1 = std::arg(mat.m[1][1]);
                const double ang2 = std::arg(mat.m[1][0]);
                phi = ang1 + ang2 - det_arg;
                lam = ang1 - ang2;
            }

            inline void params_zxz(const Mat2 &mat, double &theta, double &phi, double &lam)
            {
                params_zyz(mat, theta, phi, lam);
                phi += PI / 2.0;
                lam -= PI / 2.0;
            }

            enum class AxisGate
            {
                RZ,
                RX,
                RY
            };

            inline void emit_axis_gate(std::vector<Gate> &out, AxisGate axis, double angle, IdxType qubit, double atol)
            {
                if (near_zero(angle, atol))
                {
                    return;
                }
                switch (axis)
                {
                case AxisGate::RZ:
                    out.emplace_back(OP::RZ, qubit, -1, -1, 1, angle);
                    break;
                case AxisGate::RX:
                    out.emplace_back(OP::RX, qubit, -1, -1, 1, angle);
                    break;
                case AxisGate::RY:
                    out.emplace_back(OP::RY, qubit, -1, -1, 1, angle);
                    break;
                }
            }

            inline std::vector<Gate> circuit_kak(double theta,
                                                 double phi,
                                                 double lam,
                                                 AxisGate k_gate,
                                                 AxisGate a_gate,
                                                 IdxType qubit)
            {
                std::vector<Gate> circuit;

                if (std::abs(theta) < kAngleEps)
                {
                    lam += phi;
                    lam = mod_2pi(lam, kAngleEps);
                    emit_axis_gate(circuit, k_gate, lam, qubit, kAngleEps);
                    return circuit;
                }
                if (std::abs(theta - PI) < kAngleEps)
                {
                    lam -= phi;
                    phi = 0.0;
                }
                if (std::abs(mod_2pi(lam + PI, kAngleEps)) < kAngleEps ||
                    std::abs(mod_2pi(phi + PI, kAngleEps)) < kAngleEps)
                {
                    lam += PI;
                    theta = -theta;
                    phi += PI;
                }
                lam = mod_2pi(lam, kAngleEps);
                emit_axis_gate(circuit, k_gate, lam, qubit, kAngleEps);
                emit_axis_gate(circuit, a_gate, theta, qubit, kAngleEps);
                phi = mod_2pi(phi, kAngleEps);
                emit_axis_gate(circuit, k_gate, phi, qubit, kAngleEps);
                return circuit;
            }

            inline std::vector<Gate> circuit_u321(double theta,
                                                  double phi,
                                                  double lam,
                                                  IdxType qubit)
            {
                std::vector<Gate> circuit;
                if (std::abs(theta) < kAngleEps)
                {
                    const double tot = mod_2pi(phi + lam, kAngleEps);
                    if (std::abs(tot) > kAngleEps)
                    {
                        Gate gate(OP::U, qubit, -1, -1, 1, 0.0, 0.0, tot);
                        gate.set_custom_name("u1");
                        circuit.push_back(gate);
                    }
                }
                else if (std::abs(theta - PI / 2.0) < kAngleEps)
                {
                    Gate gate(OP::U, qubit, -1, -1, 1, PI / 2.0, mod_2pi(phi, kAngleEps), mod_2pi(lam, kAngleEps));
                    gate.set_custom_name("u2");
                    circuit.push_back(gate);
                }
                else
                {
                    Gate gate(OP::U, qubit, -1, -1, 1, theta, mod_2pi(phi, kAngleEps), mod_2pi(lam, kAngleEps));
                    gate.set_custom_name("u3");
                    circuit.push_back(gate);
                }
                return circuit;
            }

            inline void emit_rz(std::vector<Gate> &out, double angle, IdxType qubit)
            {
                const double wrapped = mod_2pi(angle, kAngleEps);
                if (near_zero(wrapped, kAngleEps))
                {
                    return;
                }
                out.emplace_back(OP::RZ, qubit, -1, -1, 1, wrapped);
            }

            inline void emit_sx(std::vector<Gate> &out, IdxType qubit)
            {
                out.emplace_back(OP::SX, qubit);
            }

            inline std::vector<Gate> circuit_rzsx(double theta,
                                                  double phi,
                                                  double lam,
                                                  IdxType qubit,
                                                  bool allow_x_pi)
            {
                std::vector<Gate> circuit;

                if (std::abs(theta) < kAngleEps)
                {
                    emit_rz(circuit, lam + phi, qubit);
                    return circuit;
                }
                if (std::abs(theta - PI / 2.0) < kAngleEps)
                {
                    emit_rz(circuit, lam - PI / 2.0, qubit);
                    emit_sx(circuit, qubit);
                    emit_rz(circuit, phi + PI / 2.0, qubit);
                    return circuit;
                }
                if (std::abs(theta - PI) < kAngleEps)
                {
                    phi -= lam;
                    lam = 0.0;
                }
                if (std::abs(mod_2pi(lam + PI, kAngleEps)) < kAngleEps ||
                    std::abs(mod_2pi(phi, kAngleEps)) < kAngleEps)
                {
                    lam += PI;
                    theta = -theta;
                    phi += PI;
                }
                theta += PI;
                phi += PI;

                emit_rz(circuit, lam, qubit);
                if (allow_x_pi && std::abs(mod_2pi(theta, kAngleEps)) < kAngleEps)
                {
                    circuit.emplace_back(OP::X, qubit);
                }
                else
                {
                    emit_sx(circuit, qubit);
                    emit_rz(circuit, theta, qubit);
                    emit_sx(circuit, qubit);
                }
                emit_rz(circuit, phi, qubit);
                return circuit;
            }

            inline std::vector<Gate> circuit_rr(double theta,
                                                double phi,
                                                double lam,
                                                IdxType qubit)
            {
                std::vector<Gate> circuit;
                if (std::abs(mod_2pi((phi + lam) / 2.0, kAngleEps)) < kAngleEps)
                {
                    if (std::abs(theta) > kAngleEps)
                    {
                        circuit.emplace_back(OP::PRX, qubit, -1, -1, 1, theta, mod_2pi(PI / 2.0 + phi, kAngleEps));
                    }
                }
                else
                {
                    if (std::abs(theta - PI) > kAngleEps)
                    {
                        circuit.emplace_back(OP::PRX, qubit, -1, -1, 1, theta - PI, mod_2pi(PI / 2.0 - lam, kAngleEps));
                    }
                    circuit.emplace_back(OP::PRX, qubit, -1, -1, 1, PI, mod_2pi(0.5 * (phi - lam + PI), kAngleEps));
                }
                return circuit;
            }

            inline bool basis_contains(const std::unordered_set<std::string> &basis, const std::string &gate)
            {
                if (basis.find(gate) != basis.end())
                {
                    return true;
                }
                if (gate == "r")
                {
                    return basis.find("prx") != basis.end();
                }
                return false;
            }

            inline bool basis_supports(const std::unordered_set<std::string> *basis,
                                       std::initializer_list<const char *> required)
            {
                if (!basis || basis->empty())
                {
                    return true;
                }
                for (const auto &gate : required)
                {
                    if (!basis_contains(*basis, gate))
                    {
                        return false;
                    }
                }
                return true;
            }

            enum class EulerBasis
            {
                U321,
                RR,
                ZYZ,
                ZXZ,
                ZSXX,
                ZSX
            };

            struct GateRun
            {
                IdxType qubit = -1;
                std::vector<IdxType> indices;
                Mat2 unitary;

                GateRun() : unitary(identity()) {}
            };

            inline std::vector<EulerBasis> possible_bases(const std::unordered_set<std::string> *basis)
            {
                std::vector<EulerBasis> out;
                out.reserve(5);
                if (basis_supports(basis, {"u3", "u2", "u1"}))
                {
                    out.push_back(EulerBasis::U321);
                }
                if (basis_supports(basis, {"r"}))
                {
                    out.push_back(EulerBasis::RR);
                }
                if (basis_supports(basis, {"rz", "ry"}))
                {
                    out.push_back(EulerBasis::ZYZ);
                }
                if (basis_supports(basis, {"rz", "rx"}))
                {
                    out.push_back(EulerBasis::ZXZ);
                }
                if (basis_supports(basis, {"rz", "sx", "x"}))
                {
                    out.push_back(EulerBasis::ZSXX);
                }
                else if (basis_supports(basis, {"rz", "sx"}))
                {
                    out.push_back(EulerBasis::ZSX);
                }
                return out;
            }

            inline void angles_from_unitary(const Mat2 &unitary,
                                            EulerBasis basis,
                                            double &theta,
                                            double &phi,
                                            double &lam)
            {
                switch (basis)
                {
                case EulerBasis::U321:
                case EulerBasis::ZSX:
                case EulerBasis::ZSXX:
                case EulerBasis::RR:
                case EulerBasis::ZYZ:
                    params_zyz(unitary, theta, phi, lam);
                    break;
                case EulerBasis::ZXZ:
                    params_zxz(unitary, theta, phi, lam);
                    break;
                }
            }

            inline std::vector<Gate> generate_circuit(EulerBasis basis,
                                                      double theta,
                                                      double phi,
                                                      double lam,
                                                      IdxType qubit)
            {
                switch (basis)
                {
                case EulerBasis::ZYZ:
                    return circuit_kak(theta, phi, lam, AxisGate::RZ, AxisGate::RY, qubit);
                case EulerBasis::ZXZ:
                    return circuit_kak(theta, phi, lam, AxisGate::RZ, AxisGate::RX, qubit);
                case EulerBasis::U321:
                    return circuit_u321(theta, phi, lam, qubit);
                case EulerBasis::ZSX:
                    return circuit_rzsx(theta, phi, lam, qubit, false);
                case EulerBasis::ZSXX:
                    return circuit_rzsx(theta, phi, lam, qubit, true);
                case EulerBasis::RR:
                    return circuit_rr(theta, phi, lam, qubit);
                }
                return {};
            }

            inline bool is_single_qubit_gate(const Gate &gate)
            {
                return gate.ctrl < 0 && gate.extra < 0 && gate.qubit >= 0;
            }

            inline bool has_error_data(const std::shared_ptr<Chip> &chip, IdxType qubit)
            {
                if (!chip)
                {
                    return false;
                }
                if (qubit < 0 || qubit >= static_cast<IdxType>(chip->single_qubit_errors.size()))
                {
                    return false;
                }
                return !chip->single_qubit_errors[static_cast<std::size_t>(qubit)].empty();
            }

            inline double lookup_error(const std::shared_ptr<Chip> &chip, IdxType qubit, const std::string &gate_name)
            {
                if (!chip)
                {
                    return 0.0;
                }
                if (qubit < 0 || qubit >= static_cast<IdxType>(chip->single_qubit_errors.size()))
                {
                    return 0.0;
                }
                const auto &err_map = chip->single_qubit_errors[static_cast<std::size_t>(qubit)];
                auto it = err_map.find(gate_name);
                if (it == err_map.end())
                {
                    return 0.0;
                }
                return it->second;
            }

            inline double compute_sequence_error(const std::vector<Gate> &sequence,
                                                 const std::shared_ptr<Chip> &chip,
                                                 IdxType qubit,
                                                 bool use_errors)
            {
                if (!use_errors)
                {
                    return static_cast<double>(sequence.size());
                }
                double fidelity = 1.0;
                for (const auto &gate : sequence)
                {
                    const std::string name = gate.lower_name();
                    fidelity *= (1.0 - lookup_error(chip, qubit, name));
                }
                return 1.0 - fidelity;
            }

            inline double compute_run_error(const std::vector<Gate> &gates,
                                            const std::vector<IdxType> &run,
                                            const std::shared_ptr<Chip> &chip,
                                            IdxType qubit,
                                            bool use_errors)
            {
                if (!use_errors)
                {
                    return static_cast<double>(run.size());
                }
                double fidelity = 1.0;
                for (IdxType gate_idx : run)
                {
                    const Gate &gate = gates[static_cast<std::size_t>(gate_idx)];
                    fidelity *= (1.0 - lookup_error(chip, qubit, gate.lower_name()));
                }
                return 1.0 - fidelity;
            }

            inline bool gate_in_basis(const Gate &gate, const std::unordered_set<std::string> *basis)
            {
                if (!basis || basis->empty())
                {
                    return true;
                }
                const std::string name = gate.lower_name();
                return basis->find(name) != basis->end();
            }
        } // namespace detail

        inline bool optimize_1q_gates_decomposition(std::shared_ptr<Circuit> circuit,
                                                    const std::shared_ptr<Chip> &chip,
                                                    const std::unordered_set<std::string> *basis_gates,
                                                    int debug_level = 0,
                                                    const char *profile_label = nullptr)
        {
            (void)debug_level;
            (void)profile_label;
            if (!circuit)
            {
                return false;
            }
            const std::vector<Gate> &gates = circuit->gate_list();
            if (gates.empty())
            {
                return false;
            }

            const IdxType capacity = circuit->num_qubits();
            if (capacity <= 0)
            {
                return false;
            }

            std::vector<detail::GateRun> current_runs(static_cast<std::size_t>(capacity));
            std::vector<detail::GateRun> runs;
            runs.reserve(gates.size());

            auto flush_run = [&](IdxType qubit)
            {
                if (qubit < 0 || qubit >= capacity)
                {
                    return;
                }
                auto &run = current_runs[static_cast<std::size_t>(qubit)];
                if (!run.indices.empty())
                {
                    runs.push_back(std::move(run));
                    run = detail::GateRun{};
                }
            };

            for (IdxType idx = 0; idx < static_cast<IdxType>(gates.size()); ++idx)
            {
                const Gate &gate = gates[static_cast<std::size_t>(idx)];
                if (detail::is_single_qubit_gate(gate))
                {
                    detail::Mat2 mat;
                    if (detail::gate_matrix(gate, mat))
                    {
                        auto &run = current_runs[static_cast<std::size_t>(gate.qubit)];
                        if (run.indices.empty())
                        {
                            run.qubit = gate.qubit;
                        }
                        run.indices.push_back(idx);
                        run.unitary = detail::matmul(mat, run.unitary);
                        continue;
                    }
                }
                if (gate.qubit >= 0)
                    flush_run(gate.qubit);
                if (gate.ctrl >= 0 && gate.ctrl != gate.qubit)
                    flush_run(gate.ctrl);
                if (gate.extra >= 0 && gate.extra != gate.qubit && gate.extra != gate.ctrl)
                    flush_run(gate.extra);
            }

            for (IdxType qubit = 0; qubit < capacity; ++qubit)
            {
                flush_run(qubit);
            }

            if (runs.empty())
            {
                return false;
            }

            const bool use_errors = std::any_of(runs.begin(), runs.end(), [&](const auto &run) {
                return detail::has_error_data(chip, run.qubit);
            });

            std::vector<char> remove_gate(gates.size(), 0);
            std::vector<std::vector<Gate>> replacements(gates.size());

            const std::vector<detail::EulerBasis> bases = detail::possible_bases(basis_gates);
            if (bases.empty())
            {
                return false;
            }

            bool changed = false;
            for (const auto &run : runs)
            {
                if (run.indices.empty())
                {
                    continue;
                }
                const Gate &first_gate = gates[static_cast<std::size_t>(run.indices.front())];
                const IdxType qubit = run.qubit;
                detail::Mat2 unitary = detail::normalize_global_phase(run.unitary);

                double best_error = std::numeric_limits<double>::infinity();
                std::vector<Gate> best_seq;
                for (detail::EulerBasis basis : bases)
                {
                    double theta = 0.0, phi = 0.0, lam = 0.0;
                    detail::angles_from_unitary(unitary, basis, theta, phi, lam);
                    std::vector<Gate> candidate = detail::generate_circuit(basis, theta, phi, lam, qubit);
                    const double err = detail::compute_sequence_error(candidate, chip, qubit, use_errors);
                    if (err < best_error || (std::abs(err - best_error) < 1e-12 && candidate.size() < best_seq.size()))
                    {
                        best_error = err;
                        best_seq = std::move(candidate);
                    }
                }

                const double old_error = detail::compute_run_error(gates, run.indices, chip, qubit, use_errors);

                bool outside_basis = false;
                if (basis_gates && !basis_gates->empty())
                {
                    for (IdxType gate_idx : run.indices)
                    {
                        if (!detail::gate_in_basis(gates[static_cast<std::size_t>(gate_idx)], basis_gates))
                        {
                            outside_basis = true;
                            break;
                        }
                    }
                }

                const bool new_is_identity = best_seq.empty();
                const bool equal_error = std::abs(best_error - old_error) < 1e-12;
                const bool shorter_replacement = equal_error && best_seq.size() < run.indices.size();
                const bool replace = outside_basis ||
                                     (best_error < old_error) ||
                                     shorter_replacement ||
                                     new_is_identity;
                if (!replace)
                {
                    continue;
                }

                changed = true;
                for (IdxType gate_idx : run.indices)
                {
                    remove_gate[static_cast<std::size_t>(gate_idx)] = 1;
                }

                for (auto &gate : best_seq)
                {
                    gate.inherit_logical_metadata(first_gate);
                }
                replacements[static_cast<std::size_t>(run.indices.front())] = std::move(best_seq);
            }

            if (!changed)
            {
                return false;
            }

            std::vector<Gate> out;
            out.reserve(gates.size());
            for (std::size_t idx = 0; idx < gates.size(); ++idx)
            {
                if (!replacements[idx].empty())
                {
                    for (auto &gate : replacements[idx])
                    {
                        out.push_back(gate);
                    }
                }
                if (remove_gate[idx])
                {
                    continue;
                }
                out.push_back(gates[idx]);
            }

            circuit->set_gates(std::move(out));
            return true;
        }
    } // namespace optimize
} // namespace QASMTrans
