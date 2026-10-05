"""Python package shim for qasmtrans."""
import importlib.util
from importlib import import_module
import sys
from pathlib import Path


def _load_core_from_path(path):
    spec = importlib.util.spec_from_file_location("qasmtrans_core", path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load qasmtrans_core extension from {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules["qasmtrans_core"] = module
    spec.loader.exec_module(module)
    return module


def _import_core():
    # Try standard import first (site-packages root)
    try:
        return import_module("qasmtrans_core")
    except ModuleNotFoundError:
        pass

    # Next, try a package-relative import (wheel layouts that place the .so under qasmtrans/)
    try:
        return import_module(".qasmtrans_core", package=__name__)
    except ModuleNotFoundError:
        pass

    # Source-tree builds put the extension next to the package directory.
    package_dir = Path(__file__).resolve().parent
    for search_dir in [package_dir, package_dir.parent]:
        candidates = sorted(search_dir.glob("qasmtrans_core*.so"))
        if candidates:
            return _load_core_from_path(candidates[0])

    raise ModuleNotFoundError(
        "Could not import qasmtrans_core. Install qasmtrans or build the C++ extension first."
    )


_core = _import_core()

transpile_qasm = _core.transpile_qasm
TranspileOptions = _core.TranspileOptions
TranspileResult = _core.TranspileResult

__all__ = [
    "transpile_qasm",
    "TranspileOptions",
    "TranspileResult",
]
