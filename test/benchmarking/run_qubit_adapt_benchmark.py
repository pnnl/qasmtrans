#!/usr/bin/env python3

"""Benchmark a local qubit-ADAPT-inspired Pauli pool on the 1000-qubit snake device."""

from __future__ import annotations

import argparse
import csv
import json
import random
import re
import statistics
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    plt = None  # type: ignore

import qiskit
from qiskit import QuantumCircuit, transpile
from qiskit.circuit.library import PauliEvolutionGate
from qiskit.qasm2 import dumps as qasm2_dumps
from qiskit.quantum_info import SparsePauliOp
from qiskit.transpiler import CouplingMap
from qiskit.transpiler.preset_passmanagers import generate_preset_pass_manager


DEFAULT_QUBITS = 100
DEFAULT_TOTAL_OPERATORS = 40
DEFAULT_CONFIGS_PER_POINT = 10
DEFAULT_SEED = 12345
DEFAULT_REPEAT_BLOCKS = 1
TIME_RE = re.compile(r"total QASMTrans time:\s*([-+]?\d*\.?\d+)")
LOCAL_BLOCK_CACHE: dict[tuple[str, float], QuantumCircuit] = {}


@dataclass(frozen=True)
class PoolOperator:
    op_id: str
    family: str
    sparse_label: str
    support: tuple[int, ...]
    window_start: int


@dataclass(frozen=True)
class PlotSeries:
    label: str
    marker: str
    linestyle: str
    avg_ms_key: str
    avg_depth_key: str
    avg_one_qubit_key: str
    avg_two_qubit_key: str
    raw_ms_key: str
    raw_depth_key: str
    raw_one_qubit_key: str
    raw_two_qubit_key: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Benchmark local qubit-ADAPT-inspired Pauli pools on the 1000-qubit snake device."
    )
    parser.add_argument("--qubits", type=int, default=DEFAULT_QUBITS, help="Logical qubit count.")
    parser.add_argument(
        "--total-operators",
        type=int,
        default=DEFAULT_TOTAL_OPERATORS,
        help="Total selected operators per benchmark point.",
    )
    parser.add_argument(
        "--configs-per-point",
        type=int,
        default=DEFAULT_CONFIGS_PER_POINT,
        help="Distinct sampled configurations per regime point.",
    )
    parser.add_argument(
        "--theta",
        type=float,
        default=0.1,
        help="Fixed evolution angle for every selected Pauli generator.",
    )
    parser.add_argument(
        "--repeat-blocks",
        type=int,
        default=DEFAULT_REPEAT_BLOCKS,
        help="Repeat each sampled 40-operator block this many times to scale the input depth.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=DEFAULT_SEED,
        help="Base seed for deterministic configuration sampling.",
    )
    parser.add_argument(
        "--device-json",
        type=Path,
        default=Path("data/devices/ibm_grid_1000.json"),
        help="Path to the existing snake-grid backend JSON.",
    )
    parser.add_argument(
        "--generated-dir",
        type=Path,
        default=Path("data/benchmarking/qubit_adapt/generated"),
        help="Directory for generated QASM inputs.",
    )
    parser.add_argument(
        "--qasmtrans-bin",
        type=Path,
        default=Path("build/QASMTrans"),
        help="Path to the QASMTrans executable.",
    )
    parser.add_argument(
        "--qasmtrans-output-dir",
        type=Path,
        default=Path("data/benchmarking/qubit_adapt/transpiled"),
        help="Directory for QASMTrans outputs.",
    )
    parser.add_argument(
        "--output-runs-csv",
        type=Path,
        default=Path("data/benchmarking/qubit_adapt/results/qubit_adapt_benchmark_runs.csv"),
        help="Destination CSV for per-configuration benchmark rows.",
    )
    parser.add_argument(
        "--output-csv",
        type=Path,
        default=Path("data/benchmarking/qubit_adapt/results/qubit_adapt_benchmark.csv"),
        help="Destination CSV for averaged benchmark rows.",
    )
    parser.add_argument(
        "--timeout-ms",
        type=int,
        default=300000,
        help="Per-circuit timeout for QASMTrans in milliseconds.",
    )
    parser.add_argument("--retries", type=int, default=2, help="Retry count for QASMTrans runs.")
    parser.add_argument(
        "--retry-delay-ms",
        type=int,
        default=1000,
        help="Delay between QASMTrans retries in milliseconds.",
    )
    parser.add_argument(
        "--skip-plots",
        action="store_true",
        help="Skip generating summary plots after writing the benchmark CSVs.",
    )
    parser.add_argument(
        "--plot-output-dir",
        type=Path,
        default=Path("data/benchmarking/qubit_adapt/plots"),
        help="Directory for the benchmark plots.",
    )
    parser.add_argument(
        "--plot-log-y",
        action="store_true",
        help="Plot timing values on a logarithmic y-axis.",
    )
    parser.add_argument(
        "--plot-exec-window-only",
        action="store_true",
        help="Plot only exec-window QASMTrans against the Qiskit baselines.",
    )
    return parser.parse_args()


def mean(values: list[float]) -> float:
    return statistics.fmean(values) if values else float("nan")


def metric_values(rows: list[dict[str, object]], key: str) -> list[float]:
    values: list[float] = []
    for row in rows:
        value = row.get(key)
        if value is None:
            continue
        values.append(float(value))
    return values


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0].keys()) if rows else []
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def load_device(cfg_path: Path) -> dict[str, object]:
    with cfg_path.open("r", encoding="utf-8") as handle:
        cfg = json.load(handle)

    coupling = cfg.get("cx_coupling") or cfg.get("coupling_map")
    if not coupling:
        raise ValueError(f"No coupling_map/cx_coupling found in {cfg_path}")

    edges: list[tuple[int, int]] = []
    for entry in coupling:
        if isinstance(entry, str):
            parts = entry.replace("-", "_").split("_")
            if len(parts) != 2:
                raise ValueError(f"Unexpected coupling entry {entry!r} in {cfg_path}")
            u, v = map(int, parts)
        else:
            if len(entry) != 2:
                raise ValueError(f"Unexpected coupling entry {entry!r} in {cfg_path}")
            u, v = entry
        edges.append((u, v))

    coupling_map = CouplingMap(edges)
    basis = cfg.get("basis_gates") or ["rz", "sx", "x", "cx"]
    basis_gates = list(dict.fromkeys(basis))

    return {
        "map": coupling_map,
        "basis": basis_gates,
        "name": cfg.get("name") or cfg_path.stem,
        "num_qubits": int(cfg.get("num_qubits", coupling_map.size())),
    }


def count_gate_types(qc: QuantumCircuit) -> tuple[int, int]:
    one_qubit = 0
    two_qubit = 0
    for instruction in qc.data:
        inst = instruction.operation
        qargs = instruction.qubits
        if inst.name in {"barrier", "measure", "delay"}:
            continue
        qubit_count = len(qargs)
        if qubit_count == 1:
            one_qubit += 1
        elif qubit_count == 2:
            two_qubit += 1
    return one_qubit, two_qubit


def run_qiskit_pass_manager(qc: QuantumCircuit, pass_manager) -> dict[str, float]:
    start = time.perf_counter()
    transpiled_qc = pass_manager.run(qc)
    elapsed_ms = (time.perf_counter() - start) * 1e3
    one_qubit, two_qubit = count_gate_types(transpiled_qc)
    return {
        "time_ms": elapsed_ms,
        "depth": float(transpiled_qc.depth()),
        "one_qubit": float(one_qubit),
        "two_qubit": float(two_qubit),
    }


def point_label(g_count: int, t3_count: int, t4_count: int, t5_count: int) -> str:
    parts: list[str] = []
    if g_count:
        parts.append(f"{g_count}G")
    if t3_count:
        parts.append(f"{t3_count}T3")
    if t4_count:
        parts.append(f"{t4_count}T4")
    if t5_count:
        parts.append(f"{t5_count}T5")
    return " + ".join(parts) if parts else "0"


def build_points(total_operators: int) -> tuple[tuple[int, int, int, int], ...]:
    if total_operators <= 0:
        raise ValueError("total-operators must be positive.")
    if total_operators % 4 != 0:
        raise ValueError("total-operators must be divisible by 4 for the fixed-ratio pool sweep.")

    q = total_operators // 4
    return (
        (4 * q, 0, 0, 0),
        (3 * q, 1 * q, 0, 0),
        (2 * q, 2 * q, 0, 0),
        (1 * q, 3 * q, 0, 0),
        (0, 4 * q, 0, 0),
        (0, 3 * q, 1 * q, 0),
        (0, 2 * q, 2 * q, 0),
        (0, 1 * q, 3 * q, 0),
        (0, 0, 4 * q, 0),
        (0, 0, 3 * q, 1 * q),
        (0, 0, 2 * q, 2 * q),
        (0, 0, 1 * q, 3 * q),
        (0, 0, 0, 4 * q),
    )


def build_pool(qubits: int) -> dict[str, list[PoolOperator]]:
    pools: dict[str, list[PoolOperator]] = {"G": [], "T3": [], "T4": [], "T5": []}
    for qubit in range(qubits):
        pools["G"].append(
            PoolOperator(
                op_id=f"G_Y_{qubit}",
                family="G",
                sparse_label="Y",
                support=(qubit,),
                window_start=qubit,
            )
        )
    for start in range(qubits - 1):
        pools["G"].append(
            PoolOperator(
                op_id=f"G_ZY_{start}",
                family="G",
                sparse_label="ZY",
                support=(start, start + 1),
                window_start=start,
            )
        )
    for start in range(qubits - 2):
        pools["T3"].append(
            PoolOperator(
                op_id=f"T3_ZZY_{start}",
                family="T3",
                sparse_label="ZZY",
                support=(start, start + 1, start + 2),
                window_start=start,
            )
        )
        pools["T3"].append(
            PoolOperator(
                op_id=f"T3_ZIY_{start}",
                family="T3",
                sparse_label="ZIY",
                support=(start, start + 1, start + 2),
                window_start=start,
            )
        )
    for start in range(qubits - 3):
        pools["T4"].append(
            PoolOperator(
                op_id=f"T4_ZZZY_{start}",
                family="T4",
                sparse_label="ZZZY",
                support=(start, start + 1, start + 2, start + 3),
                window_start=start,
            )
        )
        pools["T4"].append(
            PoolOperator(
                op_id=f"T4_ZZIY_{start}",
                family="T4",
                sparse_label="ZZIY",
                support=(start, start + 1, start + 2, start + 3),
                window_start=start,
            )
        )
    for start in range(qubits - 4):
        pools["T5"].append(
            PoolOperator(
                op_id=f"T5_ZZZZY_{start}",
                family="T5",
                sparse_label="ZZZZY",
                support=(start, start + 1, start + 2, start + 3, start + 4),
                window_start=start,
            )
        )
        pools["T5"].append(
            PoolOperator(
                op_id=f"T5_ZZZIY_{start}",
                family="T5",
                sparse_label="ZZZIY",
                support=(start, start + 1, start + 2, start + 3, start + 4),
                window_start=start,
            )
        )
    return pools


def build_local_block(sparse_label: str, theta: float) -> QuantumCircuit:
    key = (sparse_label, theta)
    cached = LOCAL_BLOCK_CACHE.get(key)
    if cached is not None:
        return cached
    width = len(sparse_label)
    op = SparsePauliOp.from_sparse_list(
        [(sparse_label, list(range(width)), 1.0)],
        num_qubits=width,
    )
    circuit = QuantumCircuit(width, name=f"exp_{sparse_label}")
    circuit.append(PauliEvolutionGate(op, time=theta), range(width))
    circuit = transpile(
        circuit,
        basis_gates=["rz", "sx", "x", "cx"],
        optimization_level=0,
    )
    LOCAL_BLOCK_CACHE[key] = circuit
    return circuit


def build_ansatz_circuit(
    num_qubits: int,
    theta: float,
    selected_ops: list[PoolOperator],
    circuit_name: str,
    repeat_blocks: int,
) -> QuantumCircuit:
    circuit = QuantumCircuit(num_qubits, name=circuit_name)
    ordered_ops = sorted(selected_ops, key=lambda op: (op.window_start, len(op.support), op.op_id))
    for _ in range(repeat_blocks):
        for op in ordered_ops:
            block = build_local_block(op.sparse_label, theta)
            circuit.compose(block, qubits=list(op.support), inplace=True)
    return circuit


def sample_family_configuration(
    pool: list[PoolOperator],
    count: int,
    rng: random.Random,
) -> list[PoolOperator]:
    if count == 0:
        return []
    return rng.sample(pool, count)


def sample_configuration(
    pools: dict[str, list[PoolOperator]],
    g_count: int,
    t3_count: int,
    t4_count: int,
    t5_count: int,
    base_seed: int,
    point_index: int,
    config_index: int,
    seen_signatures: set[tuple[str, ...]],
) -> list[PoolOperator]:
    nonce = 0
    while True:
        seed = base_seed + point_index * 1000 + config_index * 100 + nonce
        rng = random.Random(seed)
        selected = []
        selected.extend(sample_family_configuration(pools["G"], g_count, rng))
        selected.extend(sample_family_configuration(pools["T3"], t3_count, rng))
        selected.extend(sample_family_configuration(pools["T4"], t4_count, rng))
        selected.extend(sample_family_configuration(pools["T5"], t5_count, rng))
        signature = tuple(sorted(op.op_id for op in selected))
        if signature not in seen_signatures:
            seen_signatures.add(signature)
            return selected
        nonce += 1


def run_qasmtrans_identity(
    exe: Path,
    circuit_path: Path,
    device_json: Path,
    output_dir: Path,
    timeout_sec: float,
    retries: int,
    retry_delay_sec: float,
    optimize_1q: bool,
    routing_mode: str | None = None,
) -> dict[str, float | None]:
    output_dir.mkdir(parents=True, exist_ok=True)
    variant_suffix = "opt1q" if optimize_1q else "base"
    for attempt in range(1, retries + 1):
        out_qasm = output_dir / f"{circuit_path.stem}_{variant_suffix}_attempt{attempt}.qasm"
        cmd = [
            str(exe),
            "-i",
            str(circuit_path),
            "-m",
            "ibmq",
            "-c",
            str(device_json),
            "-o",
            str(out_qasm),
            "-v",
            "1",
            "--disable_mapomatic",
            "--identity-layout",
        ]
        if routing_mode is not None:
            cmd.extend(["--routing-mode", routing_mode])
        if optimize_1q:
            cmd.append("--optimize-1q")
        start = time.perf_counter()
        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout_sec,
                check=False,
            )
            wall_ms = (time.perf_counter() - start) * 1e3
        except subprocess.TimeoutExpired:
            if attempt < retries:
                time.sleep(retry_delay_sec)
                continue
            return {
                "reported_ms": None,
                "wall_ms": None,
                "depth": None,
                "one_qubit": None,
                "two_qubit": None,
            }
        if proc.returncode != 0 or not out_qasm.exists():
            if attempt < retries:
                time.sleep(retry_delay_sec)
                continue
            return {
                "reported_ms": None,
                "wall_ms": wall_ms,
                "depth": None,
                "one_qubit": None,
                "two_qubit": None,
            }
        match = TIME_RE.search(proc.stdout or "")
        reported_ms = float(match.group(1)) if match else wall_ms
        out_circuit = QuantumCircuit.from_qasm_file(out_qasm)
        one_qubit, two_qubit = count_gate_types(out_circuit)
        return {
            "reported_ms": reported_ms,
            "wall_ms": wall_ms,
            "depth": float(out_circuit.depth()),
            "one_qubit": float(one_qubit),
            "two_qubit": float(two_qubit),
        }
    return {
        "reported_ms": None,
        "wall_ms": None,
        "depth": None,
        "one_qubit": None,
        "two_qubit": None,
    }


def average_metric_rows(
    point_index: int,
    label: str,
    g_count: int,
    t3_count: int,
    t4_count: int,
    t5_count: int,
    requested: int,
    rows: list[dict[str, object]],
) -> dict[str, object]:
    return {
        "point_index": point_index,
        "point_label": label,
        "g_count": g_count,
        "t3_count": t3_count,
        "t4_count": t4_count,
        "t5_count": t5_count,
        "total_operators": g_count + t3_count + t4_count + t5_count,
        "repeat_blocks": rows[0]["repeat_blocks"] if rows else 0,
        "effective_operator_applications": (g_count + t3_count + t4_count + t5_count)
        * (rows[0]["repeat_blocks"] if rows else 0),
        "configs_requested": requested,
        "configs_succeeded": len(rows),
        "input_depth_mean": mean(metric_values(rows, "input_depth")),
        "input_one_qubit_mean": mean(metric_values(rows, "input_one_qubit")),
        "input_two_qubit_mean": mean(metric_values(rows, "input_two_qubit")),
        "qasmtrans_reported_ms_mean": mean(metric_values(rows, "qasmtrans_reported_ms")),
        "qasmtrans_depth_mean": mean(metric_values(rows, "qasmtrans_depth")),
        "qasmtrans_one_qubit_mean": mean(metric_values(rows, "qasmtrans_one_qubit")),
        "qasmtrans_two_qubit_mean": mean(metric_values(rows, "qasmtrans_two_qubit")),
        "qasmtrans_opt1q_reported_ms_mean": mean(metric_values(rows, "qasmtrans_opt1q_reported_ms")),
        "qasmtrans_opt1q_depth_mean": mean(metric_values(rows, "qasmtrans_opt1q_depth")),
        "qasmtrans_opt1q_one_qubit_mean": mean(metric_values(rows, "qasmtrans_opt1q_one_qubit")),
        "qasmtrans_opt1q_two_qubit_mean": mean(metric_values(rows, "qasmtrans_opt1q_two_qubit")),
        "qasmtrans_exec_window_opt1q_reported_ms_mean": mean(
            metric_values(rows, "qasmtrans_exec_window_opt1q_reported_ms")
        ),
        "qasmtrans_exec_window_opt1q_depth_mean": mean(
            metric_values(rows, "qasmtrans_exec_window_opt1q_depth")
        ),
        "qasmtrans_exec_window_opt1q_one_qubit_mean": mean(
            metric_values(rows, "qasmtrans_exec_window_opt1q_one_qubit")
        ),
        "qasmtrans_exec_window_opt1q_two_qubit_mean": mean(
            metric_values(rows, "qasmtrans_exec_window_opt1q_two_qubit")
        ),
        "qiskit_o0_routed_ms_mean": mean(metric_values(rows, "qiskit_o0_routed_ms")),
        "qiskit_o0_routed_depth_mean": mean(metric_values(rows, "qiskit_o0_routed_depth")),
        "qiskit_o0_routed_one_qubit_mean": mean(metric_values(rows, "qiskit_o0_routed_one_qubit")),
        "qiskit_o0_routed_two_qubit_mean": mean(metric_values(rows, "qiskit_o0_routed_two_qubit")),
        "qiskit_o1_routed_ms_mean": mean(metric_values(rows, "qiskit_o1_routed_ms")),
        "qiskit_o1_routed_depth_mean": mean(metric_values(rows, "qiskit_o1_routed_depth")),
        "qiskit_o1_routed_one_qubit_mean": mean(metric_values(rows, "qiskit_o1_routed_one_qubit")),
        "qiskit_o1_routed_two_qubit_mean": mean(metric_values(rows, "qiskit_o1_routed_two_qubit")),
    }


def compact_point_label(label: str) -> str:
    return label.replace(" + ", "\n")


def build_plot_series(exec_window_only: bool) -> list[PlotSeries]:
    if exec_window_only:
        return [
            PlotSeries(
                label="QASMTrans",
                marker="X",
                linestyle="-",
                avg_ms_key="qasmtrans_exec_window_opt1q_reported_ms_mean",
                avg_depth_key="qasmtrans_exec_window_opt1q_depth_mean",
                avg_one_qubit_key="qasmtrans_exec_window_opt1q_one_qubit_mean",
                avg_two_qubit_key="qasmtrans_exec_window_opt1q_two_qubit_mean",
                raw_ms_key="qasmtrans_exec_window_opt1q_reported_ms",
                raw_depth_key="qasmtrans_exec_window_opt1q_depth",
                raw_one_qubit_key="qasmtrans_exec_window_opt1q_one_qubit",
                raw_two_qubit_key="qasmtrans_exec_window_opt1q_two_qubit",
            ),
            PlotSeries(
                label="Qiskit O0",
                marker="^",
                linestyle="--",
                avg_ms_key="qiskit_o0_routed_ms_mean",
                avg_depth_key="qiskit_o0_routed_depth_mean",
                avg_one_qubit_key="qiskit_o0_routed_one_qubit_mean",
                avg_two_qubit_key="qiskit_o0_routed_two_qubit_mean",
                raw_ms_key="qiskit_o0_routed_ms",
                raw_depth_key="qiskit_o0_routed_depth",
                raw_one_qubit_key="qiskit_o0_routed_one_qubit",
                raw_two_qubit_key="qiskit_o0_routed_two_qubit",
            ),
            PlotSeries(
                label="Qiskit O1",
                marker="D",
                linestyle="--",
                avg_ms_key="qiskit_o1_routed_ms_mean",
                avg_depth_key="qiskit_o1_routed_depth_mean",
                avg_one_qubit_key="qiskit_o1_routed_one_qubit_mean",
                avg_two_qubit_key="qiskit_o1_routed_two_qubit_mean",
                raw_ms_key="qiskit_o1_routed_ms",
                raw_depth_key="qiskit_o1_routed_depth",
                raw_one_qubit_key="qiskit_o1_routed_one_qubit",
                raw_two_qubit_key="qiskit_o1_routed_two_qubit",
            ),
        ]
    return [
        PlotSeries(
            label="QASMTrans",
            marker="P",
            linestyle="-",
            avg_ms_key="qasmtrans_opt1q_reported_ms_mean",
            avg_depth_key="qasmtrans_opt1q_depth_mean",
            avg_one_qubit_key="qasmtrans_opt1q_one_qubit_mean",
            avg_two_qubit_key="qasmtrans_opt1q_two_qubit_mean",
            raw_ms_key="qasmtrans_opt1q_reported_ms",
            raw_depth_key="qasmtrans_opt1q_depth",
            raw_one_qubit_key="qasmtrans_opt1q_one_qubit",
            raw_two_qubit_key="qasmtrans_opt1q_two_qubit",
        ),
        PlotSeries(
            label="QASMTrans Exec-Window",
            marker="X",
            linestyle="-",
            avg_ms_key="qasmtrans_exec_window_opt1q_reported_ms_mean",
            avg_depth_key="qasmtrans_exec_window_opt1q_depth_mean",
            avg_one_qubit_key="qasmtrans_exec_window_opt1q_one_qubit_mean",
            avg_two_qubit_key="qasmtrans_exec_window_opt1q_two_qubit_mean",
            raw_ms_key="qasmtrans_exec_window_opt1q_reported_ms",
            raw_depth_key="qasmtrans_exec_window_opt1q_depth",
            raw_one_qubit_key="qasmtrans_exec_window_opt1q_one_qubit",
            raw_two_qubit_key="qasmtrans_exec_window_opt1q_two_qubit",
        ),
        PlotSeries(
            label="Qiskit O0",
            marker="^",
            linestyle="--",
            avg_ms_key="qiskit_o0_routed_ms_mean",
            avg_depth_key="qiskit_o0_routed_depth_mean",
            avg_one_qubit_key="qiskit_o0_routed_one_qubit_mean",
            avg_two_qubit_key="qiskit_o0_routed_two_qubit_mean",
            raw_ms_key="qiskit_o0_routed_ms",
            raw_depth_key="qiskit_o0_routed_depth",
            raw_one_qubit_key="qiskit_o0_routed_one_qubit",
            raw_two_qubit_key="qiskit_o0_routed_two_qubit",
        ),
        PlotSeries(
            label="Qiskit O1",
            marker="D",
            linestyle="--",
            avg_ms_key="qiskit_o1_routed_ms_mean",
            avg_depth_key="qiskit_o1_routed_depth_mean",
            avg_one_qubit_key="qiskit_o1_routed_one_qubit_mean",
            avg_two_qubit_key="qiskit_o1_routed_two_qubit_mean",
            raw_ms_key="qiskit_o1_routed_ms",
            raw_depth_key="qiskit_o1_routed_depth",
            raw_one_qubit_key="qiskit_o1_routed_one_qubit",
            raw_two_qubit_key="qiskit_o1_routed_two_qubit",
        ),
    ]


def build_error_bars(per_run_rows: list[dict[str, object]], series: list[PlotSeries]) -> dict[int, dict[str, float]]:
    grouped: dict[int, dict[str, list[float]]] = {}
    raw_metric_keys = {
        series_spec.raw_ms_key: series_spec.avg_ms_key for series_spec in series
    }
    raw_metric_keys.update({series_spec.raw_depth_key: series_spec.avg_depth_key for series_spec in series})
    raw_metric_keys.update({series_spec.raw_one_qubit_key: series_spec.avg_one_qubit_key for series_spec in series})
    raw_metric_keys.update({series_spec.raw_two_qubit_key: series_spec.avg_two_qubit_key for series_spec in series})

    for row in per_run_rows:
        point_index = int(row["point_index"])
        point_metrics = grouped.setdefault(point_index, {key: [] for key in raw_metric_keys.values()})
        for raw_key, avg_key in raw_metric_keys.items():
            value = row.get(raw_key)
            if value is None:
                continue
            point_metrics[avg_key].append(float(value))

    spreads: dict[int, dict[str, float]] = {}
    for point_index, metrics in grouped.items():
        spreads[point_index] = {}
        for avg_key, values in metrics.items():
            spreads[point_index][avg_key] = statistics.stdev(values) if len(values) >= 2 else 0.0
    return spreads


def save_figure(fig: plt.Figure, output_path: Path, *, tight_rect: tuple[float, float, float, float]) -> None:
    fig.tight_layout(rect=tight_rect)
    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    pdf_path = output_path.with_suffix(".pdf")
    fig.savefig(pdf_path, bbox_inches="tight")
    print(f"Wrote plot to {output_path}", flush=True)
    print(f"Wrote plot to {pdf_path}", flush=True)


def render_axis(
    ax,
    x_values: list[int],
    average_rows: list[dict[str, object]],
    error_bars: dict[int, dict[str, float]],
    series: list[PlotSeries],
    value_key_index: int,
    title: str,
    ylabel: str,
    *,
    log_y: bool = False,
) -> None:
    for series_spec in series:
        avg_keys = [
            series_spec.avg_ms_key,
            series_spec.avg_depth_key,
            series_spec.avg_one_qubit_key,
            series_spec.avg_two_qubit_key,
        ]
        avg_key = avg_keys[value_key_index]
        values = [float(row[avg_key]) for row in average_rows]
        errs = [error_bars.get(int(row["point_index"]), {}).get(avg_key, 0.0) for row in average_rows]
        ax.errorbar(
            x_values,
            values,
            yerr=errs,
            marker=series_spec.marker,
            linestyle=series_spec.linestyle,
            linewidth=2,
            markersize=5,
            capsize=3,
            label=series_spec.label,
        )
    ax.set_title(title)
    ax.set_ylabel(ylabel)
    if log_y:
        ax.set_yscale("log")
    ax.grid(True, linestyle="--", linewidth=0.6, alpha=0.7)
    ax.set_xticks(x_values)


def plot_benchmark_results(
    average_rows: list[dict[str, object]],
    per_run_rows: list[dict[str, object]],
    output_dir: Path,
    *,
    log_y: bool,
    exec_window_only: bool,
) -> None:
    if plt is None:
        raise SystemExit("matplotlib is required for plotting; install it with `pip install matplotlib`.")

    output_dir.mkdir(parents=True, exist_ok=True)
    series = build_plot_series(exec_window_only)
    error_bars = build_error_bars(per_run_rows, series)
    x_values = list(range(len(average_rows)))
    x_labels = [compact_point_label(str(row["point_label"])) for row in average_rows]

    plt.style.use("seaborn-v0_8-whitegrid")
    plt.rcParams.update(
        {
            "font.size": 12,
            "axes.titlesize": 13,
            "axes.labelsize": 12,
            "legend.fontsize": 11,
            "xtick.labelsize": 10,
            "ytick.labelsize": 10,
        }
    )

    fig, axes = plt.subplots(2, 2, figsize=(10.6, 10.0), sharex=True)
    ax_time, ax_depth, ax_oneq, ax_twoq = axes.flat
    render_axis(ax_time, x_values, average_rows, error_bars, series, 0, "Transpile Time", "Time (ms)", log_y=log_y)
    render_axis(ax_depth, x_values, average_rows, error_bars, series, 1, "Critical Path Depth", "Depth")
    render_axis(ax_oneq, x_values, average_rows, error_bars, series, 2, "1Q Gate Count", "1Q gates")
    render_axis(ax_twoq, x_values, average_rows, error_bars, series, 3, "2Q Gate Count", "2Q gates")
    ax_oneq.set_xlabel("Pool composition")
    ax_twoq.set_xlabel("Pool composition")
    for ax in axes.flat:
        ax.set_xticklabels(x_labels, rotation=25, ha="right")

    handles, labels = ax_time.get_legend_handles_labels()
    ax_time.legend(handles, labels, loc="upper left", frameon=True)
    save_figure(fig, output_dir / "qubit_adapt_benchmark.png", tight_rect=(0, 0, 1, 1))
    plt.close(fig)

    panel_specs = [
        ("time", 0, "Transpile Time", "Time (ms)", log_y),
        ("depth", 1, "Critical Path Depth", "Depth", False),
        ("one_qubit", 2, "1Q Gate Count", "1Q gates", False),
        ("two_qubit", 3, "2Q Gate Count", "2Q gates", False),
    ]
    for suffix, value_key_index, title, ylabel, log_scale in panel_specs:
        panel_fig, panel_ax = plt.subplots(figsize=(6.8, 6.0))
        render_axis(
            panel_ax,
            x_values,
            average_rows,
            error_bars,
            series,
            value_key_index,
            title,
            ylabel,
            log_y=log_scale,
        )
        panel_ax.set_xlabel("Pool composition")
        panel_ax.set_xticklabels(x_labels, rotation=25, ha="right")
        handles, labels = panel_ax.get_legend_handles_labels()
        panel_ax.legend(handles, labels, loc="upper left", frameon=True)
        save_figure(
            panel_fig,
            output_dir / f"qubit_adapt_benchmark_{suffix}.png",
            tight_rect=(0, 0, 1, 1),
        )
        plt.close(panel_fig)


def main() -> None:
    args = parse_args()
    if args.qubits <= 0:
        raise ValueError("qubits must be positive.")
    if args.total_operators <= 0:
        raise ValueError("total-operators must be positive.")
    if args.configs_per_point <= 0:
        raise ValueError("configs-per-point must be positive.")
    if args.repeat_blocks <= 0:
        raise ValueError("repeat-blocks must be positive.")
    if not args.device_json.is_file():
        raise FileNotFoundError(f"Device JSON not found: {args.device_json}")
    if not args.qasmtrans_bin.is_file():
        raise FileNotFoundError(f"QASMTrans binary not found: {args.qasmtrans_bin}")

    device = load_device(args.device_json)
    if device["num_qubits"] < args.qubits:
        raise ValueError(
            f"Device {device['name']} only has {device['num_qubits']} qubits; need {args.qubits}."
        )
    args.generated_dir.mkdir(parents=True, exist_ok=True)
    args.output_runs_csv.parent.mkdir(parents=True, exist_ok=True)
    args.output_csv.parent.mkdir(parents=True, exist_ok=True)

    print(
        f"Using qiskit {qiskit.__version__}, device {device['name']} "
        f"({device['num_qubits']} qubits), logical subset 0..{args.qubits - 1}",
        flush=True,
    )
    print("Input mode: predecomposed basis-gate blocks", flush=True)
    print("Prebuilding Qiskit preset pass managers with identity layout.", flush=True)
    qiskit_o0_pm = generate_preset_pass_manager(
        optimization_level=0,
        coupling_map=device["map"],
        basis_gates=device["basis"],
        initial_layout=list(range(args.qubits)),
    )
    qiskit_o1_pm = generate_preset_pass_manager(
        optimization_level=1,
        coupling_map=device["map"],
        basis_gates=device["basis"],
        initial_layout=list(range(args.qubits)),
    )
    pools = build_pool(args.qubits)
    points = build_points(args.total_operators)
    print(
        "Pool sizes:"
        f" G={len(pools['G'])},"
        f" T3={len(pools['T3'])},"
        f" T4={len(pools['T4'])},"
        f" T5={len(pools['T5'])}",
        flush=True,
    )

    for g_count, t3_count, t4_count, t5_count in points:
        if g_count + t3_count + t4_count + t5_count != args.total_operators:
            raise ValueError(
                f"Configured point {(g_count, t3_count, t4_count, t5_count)} does not sum to "
                f"--total-operators={args.total_operators}."
            )
        if (
            len(pools["G"]) < g_count
            or len(pools["T3"]) < t3_count
            or len(pools["T4"]) < t4_count
            or len(pools["T5"]) < t5_count
        ):
            raise ValueError("Requested point exceeds the available family pool size.")

    timeout_sec = args.timeout_ms / 1000.0
    retry_delay_sec = args.retry_delay_ms / 1000.0
    generated_dir = args.generated_dir / f"n{args.qubits}_ops{args.total_operators}"
    generated_dir.mkdir(parents=True, exist_ok=True)
    transpiled_dir = args.qasmtrans_output_dir / f"n{args.qubits}_ops{args.total_operators}"

    per_run_rows: list[dict[str, object]] = []
    average_rows: list[dict[str, object]] = []

    for point_index, (g_count, t3_count, t4_count, t5_count) in enumerate(points):
        label = point_label(g_count, t3_count, t4_count, t5_count)
        print(f"[{point_index + 1}/{len(points)}] {label}", flush=True)
        point_seen_signatures: set[tuple[str, ...]] = set()
        point_rows: list[dict[str, object]] = []

        for config_index in range(args.configs_per_point):
            selected_ops = sample_configuration(
                pools,
                g_count,
                t3_count,
                t4_count,
                t5_count,
                args.seed,
                point_index,
                config_index,
                point_seen_signatures,
            )
            circuit_name = (
                f"qubit_adapt_n{args.qubits}_ops{args.total_operators}"
                f"_rep{args.repeat_blocks:02d}"
                f"_g{g_count:02d}_t3{t3_count:02d}_t4{t4_count:02d}_t5{t5_count:02d}_cfg{config_index:02d}"
            )
            circuit_path = generated_dir / f"{circuit_name}.qasm"
            qc = build_ansatz_circuit(
                num_qubits=args.qubits,
                theta=args.theta,
                selected_ops=selected_ops,
                circuit_name=circuit_name,
                repeat_blocks=args.repeat_blocks,
            )
            input_one_qubit, input_two_qubit = count_gate_types(qc)
            circuit_path.write_text(qasm2_dumps(qc), encoding="utf-8")

            qasmtrans_base = run_qasmtrans_identity(
                args.qasmtrans_bin,
                circuit_path,
                args.device_json,
                transpiled_dir,
                timeout_sec,
                args.retries,
                retry_delay_sec,
                optimize_1q=False,
            )
            qasmtrans_opt1q = run_qasmtrans_identity(
                args.qasmtrans_bin,
                circuit_path,
                args.device_json,
                transpiled_dir,
                timeout_sec,
                args.retries,
                retry_delay_sec,
                optimize_1q=True,
            )
            qasmtrans_exec_window_opt1q = run_qasmtrans_identity(
                args.qasmtrans_bin,
                circuit_path,
                args.device_json,
                transpiled_dir,
                timeout_sec,
                args.retries,
                retry_delay_sec,
                optimize_1q=True,
                routing_mode="exec-window",
            )
            qiskit_o0 = run_qiskit_pass_manager(qc, qiskit_o0_pm)
            qiskit_o1 = run_qiskit_pass_manager(qc, qiskit_o1_pm)

            if (
                qasmtrans_base["depth"] is None
                or qasmtrans_opt1q["depth"] is None
                or qasmtrans_exec_window_opt1q["depth"] is None
            ):
                print(
                    f"  cfg {config_index:02d} failed in QASMTrans; skipping this sample.",
                    flush=True,
                )
                continue

            row = {
                "point_index": point_index,
                "point_label": label,
                "config_index": config_index,
                "g_count": g_count,
                "t3_count": t3_count,
                "t4_count": t4_count,
                "t5_count": t5_count,
                "total_operators": len(selected_ops),
                "repeat_blocks": args.repeat_blocks,
                "effective_operator_applications": len(selected_ops) * args.repeat_blocks,
                "input_depth": qc.depth(),
                "input_one_qubit": input_one_qubit,
                "input_two_qubit": input_two_qubit,
                "qasmtrans_reported_ms": qasmtrans_base["reported_ms"],
                "qasmtrans_depth": qasmtrans_base["depth"],
                "qasmtrans_one_qubit": qasmtrans_base["one_qubit"],
                "qasmtrans_two_qubit": qasmtrans_base["two_qubit"],
                "qasmtrans_opt1q_reported_ms": qasmtrans_opt1q["reported_ms"],
                "qasmtrans_opt1q_depth": qasmtrans_opt1q["depth"],
                "qasmtrans_opt1q_one_qubit": qasmtrans_opt1q["one_qubit"],
                "qasmtrans_opt1q_two_qubit": qasmtrans_opt1q["two_qubit"],
                "qasmtrans_exec_window_opt1q_reported_ms": qasmtrans_exec_window_opt1q["reported_ms"],
                "qasmtrans_exec_window_opt1q_depth": qasmtrans_exec_window_opt1q["depth"],
                "qasmtrans_exec_window_opt1q_one_qubit": qasmtrans_exec_window_opt1q["one_qubit"],
                "qasmtrans_exec_window_opt1q_two_qubit": qasmtrans_exec_window_opt1q["two_qubit"],
                "qiskit_o0_routed_ms": qiskit_o0["time_ms"],
                "qiskit_o0_routed_depth": qiskit_o0["depth"],
                "qiskit_o0_routed_one_qubit": qiskit_o0["one_qubit"],
                "qiskit_o0_routed_two_qubit": qiskit_o0["two_qubit"],
                "qiskit_o1_routed_ms": qiskit_o1["time_ms"],
                "qiskit_o1_routed_depth": qiskit_o1["depth"],
                "qiskit_o1_routed_one_qubit": qiskit_o1["one_qubit"],
                "qiskit_o1_routed_two_qubit": qiskit_o1["two_qubit"],
            }
            per_run_rows.append(row)
            point_rows.append(row)

        average_rows.append(
            average_metric_rows(
                point_index,
                label,
                g_count,
                t3_count,
                t4_count,
                t5_count,
                args.configs_per_point,
                point_rows,
            )
        )

    write_csv(args.output_runs_csv, per_run_rows)
    write_csv(args.output_csv, average_rows)
    print(f"Wrote per-run benchmark rows to {args.output_runs_csv}", flush=True)
    print(f"Wrote averaged benchmark rows to {args.output_csv}", flush=True)
    if not args.skip_plots:
        plot_benchmark_results(
            average_rows,
            per_run_rows,
            args.plot_output_dir,
            log_y=args.plot_log_y,
            exec_window_only=args.plot_exec_window_only,
        )


if __name__ == "__main__":
    main()
