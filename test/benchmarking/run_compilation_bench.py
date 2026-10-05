#!/usr/bin/env python3
"""Benchmark QASMTrans vs Qiskit O0/O1/O2/O3 for the compilation corpus."""
from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Dict, List, Optional, Sequence

from qiskit import QuantumCircuit, qasm2
from qiskit.transpiler import CouplingMap
from qiskit.transpiler.preset_passmanagers import generate_preset_pass_manager


TIME_RE = re.compile(r"total QASMTrans time:\s*([-+]?\d*\.?\d+)")


def has_unsupported_classical_control(qasm_path: Path) -> bool:
    text = qasm_path.read_text()
    return "if(" in text or "if (" in text


def has_measurements(qasm_path: Path) -> bool:
    text = qasm_path.read_text()
    return bool(re.search(r"^\s*measure\b", text, flags=re.M))


def load_device(cfg_path: Path) -> Dict[str, object]:
    with cfg_path.open("r") as fh:
        cfg = json.load(fh)

    coupling = cfg.get("cx_coupling") or cfg.get("coupling_map")
    if not coupling:
        raise ValueError(f"No coupling_map/cx_coupling found in {cfg_path}")

    edges = []
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

    cmap = CouplingMap(edges)
    basis = cfg.get("basis_gates") or ["rz", "sx", "x", "cx"]
    basis_gates = list(dict.fromkeys(basis))

    return {
        "map": cmap,
        "basis": basis_gates,
        "name": cfg.get("name") or cfg_path.stem,
        "num_qubits": int(cfg.get("num_qubits", cmap.size())),
    }


def build_pass_managers(
    device: Dict[str, object],
    levels: Sequence[int],
    seed: int,
):
    pass_managers = {}
    for lvl in levels:
        pass_managers[lvl] = generate_preset_pass_manager(
            optimization_level=lvl,
            coupling_map=device["map"],
            basis_gates=device["basis"],
            seed_transpiler=seed,
        )
    return pass_managers


def run_pass_managers(
    qc: QuantumCircuit,
    pass_managers,
    levels: Sequence[int],
) -> Dict[int, Dict[str, float]]:
    results: Dict[int, Dict[str, float]] = {}
    for lvl in levels:
        start = time.perf_counter()
        tc = pass_managers[lvl].run(qc)
        elapsed_ms = (time.perf_counter() - start) * 1e3
        one, two = count_gate_types(tc)
        results[lvl] = {
            "time_ms": elapsed_ms,
            "depth": tc.depth(),
            "one_qubit": one,
            "two_qubit": two,
        }
    return results


def count_gate_types(qc: QuantumCircuit) -> tuple[int, int]:
    one = two = 0
    for instruction in qc.data:
        inst = instruction.operation
        qargs = instruction.qubits
        if inst.name in {"barrier", "measure", "delay"}:
            continue
        qubit_count = len(qargs)
        if qubit_count == 1:
            one += 1
        elif qubit_count == 2:
            two += 1
    return one, two



def run_qasmtrans(
    exe: Path,
    circuit: Path,
    device_cfg: Path,
    backend_name: str,
    output_dir: Path,
    max_time_ms: int,
    retries: int,
    retry_delay_sec: float,
    optimize_1q: bool,
) -> Dict[str, Optional[float]]:
    timeout_sec = None if max_time_ms <= 0 else max_time_ms / 1000.0
    for attempt in range(1, retries + 1):
        tmp_qasm = output_dir / f"{circuit.stem}_attempt{attempt}.qasm"
        cmd = [
            str(exe),
            "-i",
            str(circuit),
            "-m",
            "ibmq",
            "-c",
            str(device_cfg),
            "-o",
            str(tmp_qasm),
            "-v",
            "1",
            "--disable_mapomatic",
        ]
        if optimize_1q:
            cmd.append("--optimize-1q")
        print(f"    QASMTrans attempt {attempt}: {' '.join(cmd)}", flush=True)
        start = time.perf_counter()
        duration_ms = None
        try:
            result = subprocess.run(
                cmd,
                text=True,
                capture_output=True,
                check=False,
                timeout=timeout_sec,
            )
            duration_ms = (time.perf_counter() - start) * 1e3
        except subprocess.TimeoutExpired:
            print(
                f"      Terminated after {max_time_ms} ms timeout.",
                flush=True,
            )
            if attempt < retries:
                time.sleep(retry_delay_sec)
            continue

        if result.returncode != 0:
            print("      QASMTrans failed.", flush=True)
            if result.stderr:
                print(result.stderr.strip(), flush=True)
            if attempt == retries:
                return {
                    "time_ms": None,
                    "reported_ms": None,
                    "one_qubit": None,
                    "two_qubit": None,
                    "depth": None,
                    "backend": backend_name,
                }
            time.sleep(retry_delay_sec)
            continue

        if not tmp_qasm.exists():
            print("      QASMTrans output missing.", flush=True)
            if attempt == retries:
                return {
                    "time_ms": None,
                    "reported_ms": None,
                    "one_qubit": None,
                    "two_qubit": None,
                    "depth": None,
                    "backend": backend_name,
                }
            time.sleep(retry_delay_sec)
            continue

        try:
            qc = QuantumCircuit.from_qasm_file(tmp_qasm)
            one_qubit, two_qubit = count_gate_types(qc)
            depth = qc.depth()
        except Exception as exc:
            print(f"      Failed to parse QASM output: {exc}", flush=True)
            if attempt == retries:
                return {
                    "time_ms": None,
                    "reported_ms": None,
                    "one_qubit": None,
                    "two_qubit": None,
                    "depth": None,
                    "backend": backend_name,
                }
            time.sleep(retry_delay_sec)
            continue

        total_ms = duration_ms
        match = TIME_RE.search(result.stdout)
        reported_ms = float(match.group(1)) if match else None
        print(
            f"      Success: wall={total_ms:.1f} ms, "
            f"reported={'' if reported_ms is None else f'{reported_ms:.3f} ms'}, "
            f"1q={one_qubit}, 2q={two_qubit}, depth={depth}",
            flush=True,
        )
        return {
            "time_ms": total_ms,
            "reported_ms": reported_ms,
            "one_qubit": one_qubit,
            "two_qubit": two_qubit,
            "depth": depth,
            "backend": backend_name,
        }

    return {
        "time_ms": None,
        "reported_ms": None,
        "one_qubit": None,
        "two_qubit": None,
        "depth": None,
        "backend": backend_name,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Run Qiskit + QASMTrans benchmarks.")
    parser.add_argument(
        "--qasm_dir",
        default="data/test_benchmark",
        help="Directory containing *.qasm circuits to benchmark.",
    )
    parser.add_argument(
        "--toronto_config",
        default="data/devices/ibmq_toronto.json",
        help="Device JSON for <=27 qubits.",
    )
    parser.add_argument(
        "--brisbane_config",
        default="data/devices/ibm_brisbane.json",
        help="Device JSON for >27 qubits.",
    )
    parser.add_argument(
        "--qasmtrans_bin",
        default="build/QASMTrans",
        help="Path to QASMTrans executable.",
    )
    parser.add_argument(
        "--max_qubits",
        type=int,
        default=150,
        help="Skip circuits with more than this many qubits.",
    )
    parser.add_argument(
        "--max_time_ms",
        type=int,
        default=1000,
        help="Maximum acceptable QASMTrans time before retry/skip (<=0 disables timeout).",
    )
    parser.add_argument(
        "--retries",
        type=int,
        default=2,
        help="Number of QASMTrans attempts before giving up.",
    )
    parser.add_argument(
        "--retry_delay_ms",
        type=int,
        default=1000,
        help="Delay between QASMTrans retries in milliseconds.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=1,
        help="Seed passed to Qiskit's transpiler.",
    )
    parser.add_argument("--optimize_1q", action="store_true")
    parser.add_argument(
        "--skip_qiskit",
        action="store_true",
        help="Skip Qiskit transpilation and only collect QASMTrans metrics.",
    )
    parser.add_argument(
        "--start_index",
        type=int,
        default=1,
        help="1-based start index in the sorted QASM list.",
    )
    parser.add_argument(
        "--end_index",
        type=int,
        default=0,
        help="1-based end index in the sorted QASM list (0 = no limit).",
    )
    parser.add_argument(
        "--skip_indices",
        default="",
        help="Comma-separated 1-based indices to skip in the sorted QASM list.",
    )
    parser.add_argument(
        "--output_csv",
        default="data/benchmarking/compilation/results/compilation_benchmarks.csv",
        help="Destination CSV file.",
    )
    parser.add_argument(
        "--unitary-only",
        action="store_true",
        help="Skip any circuit containing measurement operations.",
    )
    parser.add_argument(
        "--strip-final-measurements",
        action="store_true",
        help="Remove final measurements before benchmarking both Qiskit and QASMTrans.",
    )
    args = parser.parse_args()

    repo_root = Path.cwd()
    qasm_dir = (repo_root / args.qasm_dir).resolve()
    if not qasm_dir.is_dir():
        raise FileNotFoundError(f"QASM directory not found: {qasm_dir}")

    toronto_cfg = (repo_root / args.toronto_config).resolve()
    brisbane_cfg = (repo_root / args.brisbane_config).resolve()
    qasmtrans_bin = (repo_root / args.qasmtrans_bin).resolve()
    if not qasmtrans_bin.is_file():
        raise FileNotFoundError(f"QASMTrans binary not found: {qasmtrans_bin}")

    devices = {
        "toronto": load_device(toronto_cfg),
        "brisbane": load_device(brisbane_cfg),
    }
    pass_managers = {
        key: build_pass_managers(device, levels=[0, 1, 2, 3], seed=args.seed)
        for key, device in devices.items()
    }

    qasm_files = sorted(qasm_dir.glob("*.qasm"))
    skip_indices = {
        int(x) for x in args.skip_indices.split(",") if x.strip().isdigit()
    }
    start_idx = max(1, args.start_index)
    end_idx = args.end_index if args.end_index and args.end_index > 0 else len(qasm_files)
    qasm_files = [
        p
        for idx, p in enumerate(qasm_files, start=1)
        if start_idx <= idx <= end_idx and idx not in skip_indices
    ]
    print(f"Discovered {len(qasm_files)} circuit(s) under {qasm_dir}.", flush=True)
    if not qasm_files:
        return

    rows: List[Dict[str, object]] = []

    tmp_parent = repo_root / "data" / "benchmarking" / "compilation" / "outputs"
    tmp_parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="qasmtrans_", dir=tmp_parent) as tmp:
        tmp_dir = Path(tmp)
        total = len(qasm_files)
        for idx, qasm_path in enumerate(qasm_files, start=1):
            if has_unsupported_classical_control(qasm_path):
                print(
                    f"[{idx}/{total}] Skipping {qasm_path.name}: unsupported classical control (if-statement).",
                    flush=True,
                )
                continue
            if args.unitary_only and has_measurements(qasm_path):
                print(
                    f"[{idx}/{total}] Skipping {qasm_path.name}: contains measurements (--unitary-only).",
                    flush=True,
                )
                continue
            try:
                qc = QuantumCircuit.from_qasm_file(str(qasm_path))
            except Exception as exc:
                print(
                    f"[{idx}/{total}] Skipping {qasm_path.name}: Qiskit failed to parse ({exc}).",
                    flush=True,
                )
                continue

            benchmark_qasm_path = qasm_path
            benchmark_qc = qc
            if args.strip_final_measurements:
                benchmark_qc = qc.remove_final_measurements(inplace=False)
                benchmark_qasm_path = tmp_dir / f"{qasm_path.stem}_stripped.qasm"
                benchmark_qasm_path.write_text(qasm2.dumps(benchmark_qc))

            qubits = qc.num_qubits
            if args.max_qubits and qubits > args.max_qubits:
                print(
                    f"[{idx}/{total}] Skipping {qasm_path.name}: {qubits} qubits (> {args.max_qubits}).",
                    flush=True,
                )
                continue

            device_key = "toronto" if qubits <= 27 else "brisbane"
            device = devices[device_key]
            print(
                f"[{idx}/{total}] {qasm_path.name}: {qubits} qubits → {device['name']} ({device['num_qubits']} phys)",
                flush=True,
            )

            if args.skip_qiskit:
                print("  Qiskit transpilation skipped (--skip_qiskit).", flush=True)
                qiskit_metrics = {
                    0: {"time_ms": None, "depth": None, "one_qubit": None, "two_qubit": None},
                    1: {"time_ms": None, "depth": None, "one_qubit": None, "two_qubit": None},
                    2: {"time_ms": None, "depth": None, "one_qubit": None, "two_qubit": None},
                    3: {"time_ms": None, "depth": None, "one_qubit": None, "two_qubit": None},
                }
            else:
                qiskit_metrics = run_pass_managers(
                    benchmark_qc,
                    pass_managers[device_key],
                    levels=[0, 1, 2, 3],
                )
                for level in [0, 1, 2, 3]:
                    res = qiskit_metrics[level]
                    print(
                        f"  Qiskit level {level} pm.run: {res['time_ms']:.3f} ms, depth={res['depth']}, "
                        f"1q={res['one_qubit']}, 2q={res['two_qubit']}",
                        flush=True,
                    )

            device_cfg = toronto_cfg if device_key == "toronto" else brisbane_cfg
            qasmtrans_metrics = run_qasmtrans(
                qasmtrans_bin,
                benchmark_qasm_path,
                device_cfg,
                device["name"],
                tmp_dir,
                max_time_ms=args.max_time_ms,
                retries=args.retries,
                retry_delay_sec=args.retry_delay_ms / 1000.0,
                optimize_1q=args.optimize_1q,
            )

            qt_time = qasmtrans_metrics.get("reported_ms")
            qt_reported = qasmtrans_metrics.get("reported_ms")
            qt_wall = qasmtrans_metrics["time_ms"]
            ratio_reported = ""
            o1_time = qiskit_metrics[1]["time_ms"]
            if qt_reported is not None and qt_reported > 0 and o1_time:
                ratio_reported = f"{o1_time / qt_reported:.2f}"

            rows.append(
                {
                    "name": qasm_path.stem,
                    "circuit_file": str(qasm_path),
                    "logical_qubits": qubits,
                    "qiskit_o0_time_ms": f"{qiskit_metrics[0]['time_ms']:.3f}"
                    if qiskit_metrics[0]["time_ms"] is not None
                    else "",
                    "qiskit_o1_time_ms": f"{qiskit_metrics[1]['time_ms']:.3f}"
                    if qiskit_metrics[1]["time_ms"] is not None
                    else "",
                    "qiskit_o2_time_ms": f"{qiskit_metrics[2]['time_ms']:.3f}"
                    if qiskit_metrics[2]["time_ms"] is not None
                    else "",
                    "qiskit_o3_time_ms": f"{qiskit_metrics[3]['time_ms']:.3f}"
                    if qiskit_metrics[3]["time_ms"] is not None
                    else "",
                    "qmap_time_ms": "",
                    "qasmtrans_time_ms": f"{qt_time:.6f}" if qt_time is not None else "",
                    "ratio_o1_over_qt": ratio_reported,
                    "qasmtrans_wall_time_ms": f"{qt_wall:.6f}" if qt_wall is not None else "",
                    "qiskit_o0_single_qubit": qiskit_metrics[0]["one_qubit"] or "",
                    "qiskit_o1_single_qubit": qiskit_metrics[1]["one_qubit"] or "",
                    "qiskit_o2_single_qubit": qiskit_metrics[2]["one_qubit"] or "",
                    "qiskit_o3_single_qubit": qiskit_metrics[3]["one_qubit"] or "",
                    "qasmtrans_single_qubit": qasmtrans_metrics["one_qubit"]
                    if qasmtrans_metrics["one_qubit"] is not None
                    else "",
                    "qiskit_o0_two_qubit": qiskit_metrics[0]["two_qubit"] or "",
                    "qiskit_o1_two_qubit": qiskit_metrics[1]["two_qubit"] or "",
                    "qiskit_o2_two_qubit": qiskit_metrics[2]["two_qubit"] or "",
                    "qiskit_o3_two_qubit": qiskit_metrics[3]["two_qubit"] or "",
                    "qasmtrans_two_qubit": qasmtrans_metrics["two_qubit"]
                    if qasmtrans_metrics["two_qubit"] is not None
                    else "",
                    "qiskit_o0_depth": qiskit_metrics[0]["depth"] or "",
                    "qiskit_o1_depth": qiskit_metrics[1]["depth"] or "",
                    "qiskit_o2_depth": qiskit_metrics[2]["depth"] or "",
                    "qiskit_o3_depth": qiskit_metrics[3]["depth"] or "",
                    "qasmtrans_depth": qasmtrans_metrics["depth"]
                    if qasmtrans_metrics["depth"] is not None
                    else "",
                }
            )

    fieldnames = [
        "name",
        "circuit_file",
        "logical_qubits",
        "qiskit_o0_time_ms",
        "qiskit_o1_time_ms",
        "qiskit_o2_time_ms",
        "qiskit_o3_time_ms",
        "qmap_time_ms",
        "qasmtrans_time_ms",
        "ratio_o1_over_qt",
        "qasmtrans_wall_time_ms",
        "qiskit_o0_single_qubit",
        "qiskit_o1_single_qubit",
        "qiskit_o2_single_qubit",
        "qiskit_o3_single_qubit",
        "qasmtrans_single_qubit",
        "qiskit_o0_two_qubit",
        "qiskit_o1_two_qubit",
        "qiskit_o2_two_qubit",
        "qiskit_o3_two_qubit",
        "qasmtrans_two_qubit",
        "qiskit_o0_depth",
        "qiskit_o1_depth",
        "qiskit_o2_depth",
        "qiskit_o3_depth",
        "qasmtrans_depth",
    ]

    output_path = (repo_root / args.output_csv).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote combined results to {output_path}", flush=True)


if __name__ == "__main__":
    main()
