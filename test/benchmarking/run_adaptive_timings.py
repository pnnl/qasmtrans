#!/usr/bin/env python3
"""Adaptive timing benchmark using QASMTrans + NWQ-Sim."""

from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Tuple


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]

DEFAULT_DEVICE_NAME = "ibm_brisbane"
DEFAULT_DEVICE_JSON = REPO_ROOT / "data/devices/ibm_brisbane.json"
DEFAULT_DEVICE_QUBITS = 127
DEFAULT_COUNTS = [1, 2, 4, 6]
DEFAULT_INPUT_DIR = REPO_ROOT / "data/benchmarking/space_sharing/inputs"
DEFAULT_TARGETS = ["bb84_n8.qasm", "qaoa_n6.qasm", "qpe9.qasm"]
DEFAULT_OUTPUT_CSV = REPO_ROOT / "data/benchmarking/space_sharing/results/adaptive_timing_ibm_brisbane.csv"
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "data/benchmarking/space_sharing/outputs"
DEFAULT_NWQSIM_EXE = REPO_ROOT / ".." / "NWQ-Sim" / "build" / "qasm" / "nwq_qasm"
DEFAULT_QASMTRANS_BIN = REPO_ROOT / "build" / "QASMTrans"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run adaptive timing benchmark (QASMTrans + NWQ-Sim).")
    parser.add_argument("--device_name", default=DEFAULT_DEVICE_NAME)
    parser.add_argument("--device_json", default=str(DEFAULT_DEVICE_JSON))
    parser.add_argument("--device_qubits", type=int, default=DEFAULT_DEVICE_QUBITS)
    parser.add_argument("--counts", default="1,2,4,6", help="Comma-separated counts of repeated circuits per run.")
    parser.add_argument("--input_dir", default=str(DEFAULT_INPUT_DIR), help="Directory containing target *.qasm circuits.")
    parser.add_argument("--targets", default=",".join(DEFAULT_TARGETS), help="Comma-separated list of circuit filenames to include.")
    parser.add_argument("--output_csv", default=str(DEFAULT_OUTPUT_CSV), help="Destination CSV.")
    parser.add_argument("--output_root", default=str(DEFAULT_OUTPUT_ROOT), help="Where to place transpiled outputs.")
    parser.add_argument("--qasmtrans_bin", default=str(DEFAULT_QASMTRANS_BIN), help="Path to QASMTrans binary.")
    parser.add_argument("--nwqsim_exe", default=str(DEFAULT_NWQSIM_EXE), help="Path to NWQ-Sim executable.")
    parser.add_argument("--nwqsim_backend", default="CPU")
    parser.add_argument("--nwqsim_shots", type=int, default=1024)
    parser.add_argument("--nwqsim_extra", default="", help="Extra args string for NWQ-Sim.")
    parser.add_argument("--mapomatic_limit", type=int, default=50000)
    return parser.parse_args()


def qubit_count_from_qasm(path: Path) -> int:
    pattern = re.compile(r"qreg\s+\w+\[(\d+)\]")
    with path.open("r", encoding="utf-8") as fh:
        for line in fh:
            m = pattern.search(line)
            if m:
                return int(m.group(1))
    raise ValueError(f"Could not determine qubit count in {path}")


def run_qasmtrans_bin(
    qasmtrans_bin: Path,
    inputs: List[Path],
    device_json: Path,
    output_base: Path,
    mapomatic_limit: int,
) -> Tuple[int, str]:
    args: List[str] = [str(qasmtrans_bin)]
    for p in inputs:
        args += ["-i", str(p)]
    args += [
        "-m",
        "ibmq",
        "-c",
        str(device_json),
        "-o",
        str(output_base) + ".qasm",
        "-full_fidelity",
        "-mapomatic_limit",
        str(mapomatic_limit),
    ]
    start = time.perf_counter()
    proc = subprocess.run(args, capture_output=True, text=True)
    end = time.perf_counter()
    elapsed_ms = int((end - start) * 1000)
    if proc.returncode != 0:
        raise RuntimeError(f"QASMTrans failed: {proc.stderr or proc.stdout}")
    return elapsed_ms, proc.stdout or ""


def parse_log_metrics(stdout_text: str) -> Tuple[int, int]:
    part_match = re.search(r"partition_ms=(\d+)", stdout_text)
    total_match = re.search(r"total_ms=(\d+)", stdout_text)
    partition_ms = int(part_match.group(1)) if part_match else 0
    total_ms = int(total_match.group(1)) if total_match else 0
    return partition_ms, total_ms


def run_nwqsim_on_subchips(
    nwqsim_exe: Path,
    subchip_dir: Path,
    backend: str,
    shots: int,
    extra_args: List[str],
) -> Tuple[str, str, str]:
    if not subchip_dir.is_dir():
        return ("", "", "")
    fidelities: List[float] = []
    for subchip_json in sorted(subchip_dir.glob("circuit*_subchip.json")):
        qasm_path = subchip_json.with_suffix(".qasm")
        if not qasm_path.is_file():
            continue
        cmd = [
            str(nwqsim_exe),
            "--backend",
            backend,
            "--shots",
            str(shots),
            "--sim",
            "dm",
            "--device",
            str(subchip_json),
            "-q",
            str(qasm_path),
            "--fidelity",
        ]
        if extra_args:
            cmd += extra_args
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if proc.returncode != 0:
            raise RuntimeError(f"NWQ-Sim failed: {proc.stderr or proc.stdout}")
        m = re.search(r"State Fidelity:\s*([0-9.eE+-]+)", proc.stdout or "")
        if m:
            fidelities.append(float(m.group(1)))
    if not fidelities:
        return ("", "", "")
    avg = sum(fidelities) / len(fidelities)
    lo = min(fidelities) if len(fidelities) > 1 else ""
    hi = max(fidelities) if len(fidelities) > 1 else ""
    return (f"{avg:.6f}", f"{lo:.6f}" if lo != "" else "", f"{hi:.6f}" if hi != "" else "")


def main() -> None:
    args = parse_args()
    qasmtrans_bin = Path(args.qasmtrans_bin).resolve()
    if not qasmtrans_bin.is_file():
        raise FileNotFoundError(f"QASMTrans binary not found at {qasmtrans_bin}")
    input_dir = Path(args.input_dir).resolve()
    if not input_dir.is_dir():
        raise FileNotFoundError(f"Input directory not found: {input_dir}")
    device_json = Path(args.device_json).resolve()
    nwqsim_exe = Path(args.nwqsim_exe).resolve()
    if not nwqsim_exe.exists():
        raise FileNotFoundError(f"NWQ-Sim executable not found at {nwqsim_exe}")
    counts = [int(x) for x in args.counts.split(",") if x.strip()]
    output_root = Path(args.output_root).resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    out_csv = Path(args.output_csv).resolve()
    out_csv.parent.mkdir(parents=True, exist_ok=True)

    nwqsim_extra = args.nwqsim_extra.split() if args.nwqsim_extra else []

    targets = [t.strip() for t in args.targets.split(",") if t.strip()]
    circuits = [input_dir / t for t in targets if (input_dir / t).is_file()]
    if not circuits:
        print(f"No target circuits found under {input_dir} for names {targets}", file=sys.stderr)
        return

    with out_csv.open("w", encoding="utf-8", newline="") as fh:
        writer = csv.writer(fh)
        writer.writerow(
            [
                "Circuit",
                "Qubit Number",
                "Device",
                "Device Qubit number",
                "Number of circuits compiled to device",
                "Time to transpile single circuit to device (ms)",
                "Time to transpile circuit number to device (ms)",
                "Circuit partitioning time (ms)",
                "Average Fidelity",
                "Minimum Fidelity",
                "Maximum Fidelity",
            ]
        )

        for circuit_path in circuits:
            circuit_name = circuit_path.name
            qubit_count = qubit_count_from_qasm(circuit_path)
            single_ms = ""

            for count in counts:
                output_base = output_root / f"{circuit_path.stem}_N{count}"
                elapsed_ms, stdout_text = run_qasmtrans_bin(
                    qasmtrans_bin, [circuit_path] * count, device_json, output_base, args.mapomatic_limit
                )
                partition_ms, total_ms = parse_log_metrics(stdout_text)
                if total_ms == 0:
                    total_ms = elapsed_ms
                if single_ms == "":
                    single_ms = str(total_ms)
                fidelities = run_nwqsim_on_subchips(
                    nwqsim_exe,
                    output_base.with_name(output_base.name + "_subchips"),
                    args.nwqsim_backend,
                    args.nwqsim_shots,
                    nwqsim_extra,
                )

                writer.writerow(
                    [
                        circuit_name,
                        qubit_count,
                        args.device_name,
                        args.device_qubits,
                        count,
                        single_ms,
                        f"{total_ms:.0f}",
                        partition_ms,
                        fidelities[0],
                        fidelities[1],
                        fidelities[2],
                    ]
                )

    print(f"CSV written to {out_csv}")


if __name__ == "__main__":
    main()
