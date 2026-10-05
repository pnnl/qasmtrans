# QASMTrans Passes

Core passes:
- `transpiler.hpp`: Orchestrates the pass pipeline.
- `routing_mapping.hpp`: Routing and mapping.
- `decompose.hpp`: Decompose into supported basis gates.
- `remapping.hpp`: Priority-based qubit remapping.

Future work:
- Gate optimization (merge/cancel).
- ASAP scheduling.

## Adding a pass

1) Add a header under `include/circuit_passes/`, e.g. `gate_optimization.hpp`:

```cpp
#pragma once
#include "../IR/gate.hpp"
#include "../IR/circuit.hpp"

using namespace QASMTrans;

void gate_optimization(std::shared_ptr<Circuit> circuit) {
  // pass body
}
```

2) Wire it into `transpiler.hpp`:

```cpp
#include "gate_optimization.hpp"

void transpiler(...) {
    Decompose_three_to_two(circuit);
    Routing(circuit, backend_name, run_with_limit, debug_level);
    Decompose(circuit);
    gate_optimization(circuit);
}
```

3) Rebuild and run:

```bash
./QASMTrans -i ../data/test_benchmark/bv10.qasm -v 1
```

## Gate optimization example

Example sequence: `H, S, T`

Decomposition:
- `H → Rz(pi/2), SX, Rz(pi/2)`
- `S → Rz(pi/2)`
- `T → Rz(pi/4)`

Merged:
- `Rz(pi/2), SX, Rz(-3pi/4)`
