# QASMTrans Passes

Welcome to the QASMTrans Passes repository. This repository currently includes the following passes:

- `transpiler.hpp`: Main function calls to the passes.
- `routing_mapping.hpp`: Routing and mapping pass.
- `decompose.hpp`: Decomposes the circuit into the basis gates supported by IBM.
- `remapping.hpp`: Remaps the qubits based on user-specified priority settings.

## Future Improvements

Additional enhancements could be made to these passes, including gate optimization, as soon as possible (ASAP) gate scheduling, and others.

To contribute to this project by adding more passes, please follow the steps outlined below:

### 1. Add a Pass File

You should place new pass files under the `passes` folder. For instance, `gate_optimization.hpp` is a planned pass to support gate merging and cancellation. The file will be construct as below:

```cpp

#ifndef GATE_OPTIMIZATION_H
#define GATE_OPTIMIZATION_H

#include "gate.hpp"
#include "circuit.hpp"

using namespace QASMTrans;

void gate_optimization(std::shared_ptr<QASMTrans::Circuit> circuit) 
{
  ... // main function
}

#endif
```

### 2. Link the Pass File

Once you have created `gate_optimization.hpp`, you need to link it to `compiler.hpp` with the following code in the `transpiler.h` file:

```cpp
# include "gate_optimization.hpp"
# include "..."

void transpiler (...){
    // step 1 Decompose 3-qubit gates to 2-qubit gates
    Decompose_three_to_two(circuit);


    // step 2 Routing and Mapping
    Routing(circuit,backend_name,run_with_limit,debug_level);

    //^ step 3 Decompose gates
    Decompose(circuit);
    // insert the gate_optimization
    gate_optimization(circuit);

//...
}


```

### 3. Execute the Pass File

Navigate to the build folder and run `make` to rebuild the project. Run the program using the following command:

```bash
./QASMTrans -i ../data/test_benchmark/bv10.qasm  -v 1 -b ibmq_toronto -m qiskit
```

### 4. Motivation of Gate merge/optimization

The goal of the gate optimization pass is to merge or cancel gates in the circuit. Let's consider an example to better illustrate this. 

Suppose we have the following original set of gates:

"H, S, T"

Without optimization, these gates would decompose into the following sequence of basis gates:

- H gate decomposes into: "Rz(pi/2), SX, Rz(pi/2)"
- S gate decomposes into: "Rz(pi/2)"
- T gate decomposes into: "Rz(pi/4)"

This results in a total of 5 basis gates:

"Rz(pi/2), SX, Rz(pi/2), Rz(pi/2), Rz(pi/4)"


However, with the gate optimization pass, the last three Rz gates can be merged into a single gate, which means the sequence of gates will then look like:

"Rz(pi/2), SX, Rz(-3pi/4)"


This sequence only contains 3 basis gates, which demonstrates the benefits of gate optimization. The optimization reduces the total number of gates needed, which is beneficial for mitigating the effects of errors in quantum computation.

This example represents one policy or strategy for gate optimization. There may be other strategies or rules that could result in different optimized gate sequences. The actual optimization process would depend on the specific gates in the circuit and the rules defined in the `gate_optimization.hpp` pass.

