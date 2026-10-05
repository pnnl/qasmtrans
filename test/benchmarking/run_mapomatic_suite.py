#!/usr/bin/env python3
"""Run Mapomatic benchmarks and emit CSV + fidelity/timing plots."""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import subprocess
import sys
import time
from collections import defaultdict, OrderedDict
from pathlib import Path
from typing import Dict, Iterable, List, Optional

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError:
    plt = None  # type: ignore

import qasmtrans


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT_DIR = REPO_ROOT / "data/benchmarking/mapomatic/inputs"
DEFAULT_CIRCUITS = ("qpe9.qasm", "sat7.qasm", "shor7.qasm", "vqe8.qasm")


def parse_mapomatic_ms(output: str) -> float:
    """
    Extract total Mapomatic time (ms) from a qasmtrans verbose line:
    "Mapomatic timing (ms): critical-path prep 0.004, embedding 0.046, scoring 0.975, apply 0.031"
    """
    line = None
    for ln in output.splitlines():
        if "Mapomatic timing" in ln:
            line = ln
            break
    if not line:
        return 0.0
    nums = re.findall(r"([0-9]*\.?[0-9]+)", line)
    if not nums:
        return 0.0
    try:
        return sum(float(x) for x in nums[-4:])  # prep, embedding, scoring, apply
    except Exception:
        return 0.0


def run_qasmtrans(
    circuit: Path,
    mode: str,
    device_json: Path,
    out_dir: Path,
    mapomatic_limit: int,
) -> Dict[str, object]:
    out_prefix = out_dir / f"{circuit.stem}_{mode}"
    helper = """
import os, sys, time, json, qasmtrans

qasm_text = sys.stdin.read()
opts = qasmtrans.TranspileOptions()
opts.backend_config = os.environ["QASMTRANS_DEVICE"]
opts.output_path = os.environ["QASMTRANS_OUT"]
opts.mode = "ibmq"
opts.mapomatic_limit = int(os.environ["QASMTRANS_MAPLIMIT"])
mode = os.environ["QASMTRANS_MODE"]
if mode == "nomap":
    opts.disable_mapomatic = True
elif mode == "product":
    opts.disable_mapomatic = False
    opts.full_fidelity = False
    opts.verbose = 2
elif mode == "full":
    opts.disable_mapomatic = False
    opts.full_fidelity = True
    opts.verbose = 2
elif mode == "hybrid":
    opts.disable_mapomatic = False
    opts.full_fidelity = False
    opts.verbose = 2
else:
    raise SystemExit(f"unknown mode {mode}")
start = time.perf_counter()
res = qasmtrans.transpile_qasm(qasm_text, opts)
elapsed = (time.perf_counter() - start) * 1e3
print(json.dumps({"elapsed_ms": elapsed, "log": res.log}))
"""

    env = {
        "QASMTRANS_DEVICE": str(device_json),
        "QASMTRANS_OUT": str(out_prefix) + ".qasm",
        "QASMTRANS_MAPLIMIT": str(mapomatic_limit),
        "QASMTRANS_MODE": mode,
        **os.environ,
    }
    proc = subprocess.run(
        [sys.executable, "-c", helper],
        input=circuit.read_text(encoding="utf-8"),
        text=True,
        capture_output=True,
        env=env,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"qasmtrans failed ({mode}) for {circuit.name}: {proc.stderr or proc.stdout}")
    stdout = proc.stdout or ""
    try:
        payload = json.loads(stdout.splitlines()[-1])
        total_ms = float(payload.get("elapsed_ms", 0.0))
        log_text = payload.get("log", "") or ""
    except Exception as exc:
        raise RuntimeError(f"Failed to parse qasmtrans output for {circuit.name} ({mode}): {exc}. stdout={stdout}") from exc
    # Combine stdout (for mapomatic timing) and log
    combined = stdout + "\n" + log_text
    map_ms = parse_mapomatic_ms(combined)
    return {"total_ms": float(total_ms), "mapomatic_ms": map_ms, "out_prefix": out_prefix}


def run_nwqsim(nwqsim_exe: Path, device_json: Path, subchip_qasm: Path, backend: str, shots: int, extra_args: List[str]) -> float:
    cmd = [str(nwqsim_exe), "--backend", backend, "--shots", str(shots), "--sim", "dm", "--device", str(device_json), "--fidelity", "-q", str(subchip_qasm)]
    if extra_args:
        cmd += extra_args
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"NWQ-Sim failed: {proc.stderr or proc.stdout}")
    match = re.search(r"State Fidelity:\s*([0-9.eE+-]+)", proc.stdout or "")
    if not match:
        raise RuntimeError(f"Could not parse fidelity from NWQ-Sim output:\n{proc.stdout}")
    return float(match.group(1))


# ---------- Plotting ----------
MODE_ORDER_FIDELITY = ["nomap", "product", "full", "hybrid"]
MODE_LABELS_FIDELITY = OrderedDict(
    [
        ("nomap", "Baseline"),
        ("product", "Critical Path"),
        ("full", "Full Circuit"),
        ("hybrid", "Hybrid"),
    ]
)
MODE_COLORS = {
    "nomap": "#6c757d",
    "product": "#1f77b4",
    "full": "#2ca02c",
    "hybrid": "#ff7f0e",
}


def plot_fidelity(csv_path: Path, pdf_path: Path) -> None:
    if plt is None:
        raise SystemExit("matplotlib is required for plotting; install with `pip install matplotlib`.")
    data: Dict[str, Dict[str, float]] = defaultdict(dict)
    with csv_path.open("r", newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            circuit = row["circuit"]
            mode = row["mode"]
            try:
                fidelity = float(row["fidelity"])
            except Exception:
                fidelity = 0.0
            data[circuit][mode] = fidelity

    circuits = sorted(data.keys())
    x = range(len(circuits))
    width = 0.18
    fig, ax = plt.subplots(figsize=(7.5, 4.2))
    num_modes = len(MODE_LABELS_FIDELITY)
    for idx, (mode, label) in enumerate(MODE_LABELS_FIDELITY.items()):
        offsets = [pos + (idx - (num_modes - 1) / 2) * width for pos in x]
        vals = [data[c].get(mode, 0.0) for c in circuits]
        bars = ax.bar(offsets, vals, width=width, label=label, color=MODE_COLORS.get(mode))
        for bar in bars:
            h = bar.get_height()
            ax.text(bar.get_x() + bar.get_width() / 2, h + 0.01, f"{h:.2f}", ha="center", va="bottom", fontsize=8)

    ax.set_ylabel("Simulated Fidelity", fontsize=11)
    ax.set_xticks(list(x))
    ax.set_xticklabels([c.upper() for c in circuits], fontsize=11)
    ax.set_ylim(0.0, 1.05)
    ax.legend(frameon=False, fontsize=9)
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.6)
    fig.tight_layout()
    fig.savefig(pdf_path, bbox_inches="tight")
    plt.close(fig)


def plot_timing(csv_path: Path, pdf_path: Path) -> None:
    if plt is None:
        raise SystemExit("matplotlib is required for plotting; install with `pip install matplotlib`.")
    raw: Dict[str, Dict[str, float]] = defaultdict(dict)
    with csv_path.open("r", newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            circuit = row["circuit"]
            mode = row["mode"]
            try:
                t = float(row["mapomatic_ms"])
            except Exception:
                t = 0.0
            raw[circuit][mode] = t

    percentages: Dict[str, Dict[str, float]] = defaultdict(dict)
    for circuit, timings in raw.items():
        full_time = timings.get("full", 0.0)
        if full_time <= 0.0:
            continue
        for mode in ("product", "hybrid"):
            mode_time = timings.get(mode, 0.0)
            percentages[circuit][mode] = (mode_time / full_time) * 100.0 if mode_time > 0 else 0.0

    circuits = sorted(percentages.keys())
    x = range(len(circuits))
    width = 0.25
    fig, ax = plt.subplots(figsize=(6.5, 4.0))
    modes = [m for m in ("product", "hybrid") if any(m in percentages[c] for c in circuits)]
    for idx, mode in enumerate(modes):
        offsets = [pos + (idx - (len(modes) - 1) / 2) * width for pos in x]
        vals = [percentages[c].get(mode, 0.0) for c in circuits]
        bars = ax.bar(offsets, vals, width=width, color=MODE_COLORS.get(mode))
        for bar in bars:
            h = bar.get_height()
            ax.text(bar.get_x() + bar.get_width() / 2, h + 2, f"{h:.0f}%", ha="center", va="bottom", fontsize=8)

    ax.set_ylabel("Mapomatic Time vs Full Fidelity", fontsize=11)
    ax.set_xticks(list(x))
    ax.set_xticklabels([c.upper() for c in circuits], fontsize=11)
    max_val = max((max(vals.values()) for vals in percentages.values()), default=0.0)
    ax.set_ylim(0.0, max(110.0, max_val * 1.1))
    ax.grid(axis="y", linestyle="--", linewidth=0.5, alpha=0.6)
    fig.tight_layout()
    fig.savefig(pdf_path, bbox_inches="tight")
    plt.close(fig)


# ---------- Main ----------
def run_suite(
    circuits: Iterable[Path],
    modes: List[str],
    nwqsim_exe: Path,
    device_json: Path,
    output_csv: Path,
    output_dir: Path,
    mapomatic_limit: int,
    nwqsim_backend: str,
    nwqsim_shots: int,
    nwqsim_extra: List[str],
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    output_csv.parent.mkdir(parents=True, exist_ok=True)

    with output_csv.open("w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=["circuit", "mode", "total_ms", "mapomatic_ms", "fidelity"])
        writer.writeheader()
        for circuit in circuits:
            if not circuit.is_file():
                print(f"Skipping missing circuit {circuit}", file=sys.stderr)
                continue
            for mode in modes:
                print(f"[{circuit.name}] mode={mode} ...", flush=True)
                res = run_qasmtrans(circuit, mode, device_json, output_dir, mapomatic_limit)
                sub_dir = Path(f"{res['out_prefix']}_subchips")
                dev = sub_dir / "circuit00_subchip.json"
                sub_qasm = sub_dir / "circuit00_subchip.qasm"
                if not dev.is_file() or not sub_qasm.is_file():
                    raise FileNotFoundError(f"Missing subchip outputs for {circuit} ({mode}) under {sub_dir}")
                fidelity = run_nwqsim(nwqsim_exe, dev, sub_qasm, nwqsim_backend, nwqsim_shots, nwqsim_extra)
                writer.writerow(
                    {
                        "circuit": circuit.stem,
                        "mode": mode,
                        "total_ms": f"{res['total_ms']:.3f}",
                        "mapomatic_ms": f"{res['mapomatic_ms']:.3f}",
                        "fidelity": f"{fidelity:.6f}",
                    }
                )
                fh.flush()
                print(
                    f"  total_ms={res['total_ms']:.1f}, mapomatic_ms={res['mapomatic_ms']:.3f}, fidelity={fidelity:.4f}",
                    flush=True,
                )


def main() -> None:
    parser = argparse.ArgumentParser(description="Run Mapomatic benchmarks and generate plots.")
    parser.add_argument(
        "-i",
        "--input",
        action="append",
        help=(
            "QASM circuit file(s). If omitted, defaults to "
            f"data/benchmarking/mapomatic/inputs/{{{','.join(DEFAULT_CIRCUITS)}}}."
        ),
    )
    parser.add_argument(
        "--modes",
        default="nomap,product,full,hybrid",
        help="Comma-separated modes to run (nomap,product,full,hybrid).",
    )
    parser.add_argument(
        "--device_json",
        default=str(REPO_ROOT / "data" / "devices" / "ibm_brisbane.json"),
        help="Backend JSON.",
    )
    parser.add_argument(
        "--output_csv",
        default=str(REPO_ROOT / "data" / "benchmarking" / "mapomatic" / "results" / "mapomatic_benchmarks.csv"),
        help="Where to write the benchmark CSV.",
    )
    parser.add_argument(
        "--output_dir",
        default=str(REPO_ROOT / "data" / "benchmarking" / "mapomatic" / "outputs"),
        help="Directory to store transpiled outputs.",
    )
    parser.add_argument(
        "--mapomatic_limit",
        type=int,
        default=50000,
        help="Mapomatic candidate limit.",
    )
    parser.add_argument(
        "--nwqsim_exe",
        default=str(REPO_ROOT / ".." / "NWQ-Sim" / "build" / "qasm" / "nwq_qasm"),
        help="Path to NWQ-Sim executable.",
    )
    parser.add_argument("--nwqsim_backend", default="CPU", help="NWQ-Sim backend (CPU/GPU).")
    parser.add_argument("--nwqsim_shots", type=int, default=1024, help="NWQ-Sim shots.")
    parser.add_argument(
        "--nwqsim_extra",
        default="",
        help="Extra args for NWQ-Sim (quoted string).",
    )
    parser.add_argument(
        "--skip_plots",
        action="store_true",
        help="Skip generating plots.",
    )
    parser.add_argument(
        "--fidelity_pdf",
        default=str(REPO_ROOT / "data" / "benchmarking" / "mapomatic" / "results" / "mapomatic_fidelity.pdf"),
        help="Path for fidelity plot PDF.",
    )
    parser.add_argument(
        "--timing_pdf",
        default=str(REPO_ROOT / "data" / "benchmarking" / "mapomatic" / "results" / "mapomatic_timing.pdf"),
        help="Path for timing plot PDF.",
    )
    args = parser.parse_args()

    modes = [m.strip() for m in args.modes.split(",") if m.strip()]
    circuits: List[Path] = []
    if args.input:
        circuits = [Path(p).resolve() for p in args.input]
    else:
        circuits = [(DEFAULT_INPUT_DIR / name).resolve() for name in DEFAULT_CIRCUITS]
        missing = [path for path in circuits if not path.is_file()]
        if missing:
            missing_text = ", ".join(str(path) for path in missing)
            raise FileNotFoundError(
                "Default Mapomatic inputs are missing. "
                f"Provide --input explicitly or restore: {missing_text}"
            )

    nwqsim_extra = args.nwqsim_extra.split() if args.nwqsim_extra else []

    run_suite(
        circuits=circuits,
        modes=modes,
        nwqsim_exe=Path(args.nwqsim_exe).resolve(),
        device_json=Path(args.device_json).resolve(),
        output_csv=Path(args.output_csv).resolve(),
        output_dir=Path(args.output_dir).resolve(),
        mapomatic_limit=args.mapomatic_limit,
        nwqsim_backend=args.nwqsim_backend,
        nwqsim_shots=args.nwqsim_shots,
        nwqsim_extra=nwqsim_extra,
    )

    if not args.skip_plots:
        plot_fidelity(Path(args.output_csv), Path(args.fidelity_pdf))
        plot_timing(Path(args.output_csv), Path(args.timing_pdf))
        print(f"Wrote plots to {args.fidelity_pdf} and {args.timing_pdf}")


if __name__ == "__main__":
    main()
