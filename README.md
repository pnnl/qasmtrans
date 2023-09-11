# QASMTrans
QASMTrans is a quantum transpiler for effectively parsing and translating general OpenQASM[1] circuits to
circuits compiled for a particular NISQ device (e.g., from IBMQ, Rigetti, IonQ, Quantinuum), addressing
the contraints of basis gates and qubit topology. QASMTrans is purely developed in C++ without external
library dependency, facilitate deployment across platforms. It is specially designed for emerging deep
circuits, such as those from HHL, QPE, quantum simulation, etc. QASMTrans is easy to extend for adding 
new optimization passes and backend devices (see [extension](passes/README.md)). For some examplar 
QASM circuits, please check our [QASMBench](https://github.com/pnnl/qasmbench).

Please check our paper for details and performance: https://arxiv.org/pdf/2308.07581.pdf


## Installation
To install the software, follow the steps below:

```bash
git clone https://github.com/pnnl/qasmtrans.git
cd qasmtrans
mkdir build
cd build
cmake ..
make 
```

## Execution
To run the transpiler, use the command below:

```bash
./qasmtrans -i ../data/test_benchmark/bv10.qasm -m ibmq -c ../data/devices/ibmq_toronto.json -v 1
```

## Correctness verification
We also test our QASMTrans correctness, we use the same simulator from qiskit to test our transpiled qasm file and qiskit own transpiled file, we pass the file when the differences is less than 0.5%, to run the test simply run the command below:

```bash
cd test
sh validation_test.sh
```

All the detailed result will be stored in compare_summary
## Options
QASMTrans command-line options:

- `-i`: Input qasm file, e.g.  `data/test_benchmark/bv10.qasm`    

- `-o`: Specify the output file location, the default path is `data/output_qasm_file/{circuit}_{mode}.qasm`.

- `-b`: Sepcify basis gate set {x, y, z} (Future support) 

- `-q`: Take a qasm circuit string as input (Future support)

- `-m`: Set the mode that determines the specific basis gate set for a vendor:
  - `ibmq` : The basis gates for IBMQ here is [rz,sx,x,cx] (default)
  - `ionq`:  The basis gates for IonQ here is [rx(gpi),ry(gpi2),rz(gz),rxx(ms)] 
  - `quantinuum` : The basis gates for Quantinuum here is [U(&#03b8;,&#03d5;),rz,zz]
  - `rigetti` : The basis gates for Rigetti here is [rx,ry,cz] 

- `-c`: Specify the backend device with certain topology. The path is "data/devices/"

    IBMQ Machines (Heavy-hexagon):
    
    - `ibmq_toronto (27 qubits)` (default option)
    - `ibmq_jakarta (7 qubits)`
    - `ibmq_guadalupe (16 qubits)`
    - `ibm_cairo (27 qubits)`
    - `ibm_brisbane (127 qubits)`

    Rigetti Machine (Ring):
    - `aspen_m3 (80 qubits)`

    Quantinuum Machine (All-to-all connected):
    - `h1_2 (12 qubits)`
    - `h1_1 (20 qubits)` 
    
    Dummy Machines (All-to-all connected):
    
    - `dummy_ibmq12 (12 qubits)`
    - `dummy_ibmq14 (14 qubits)`
    - `dummy_ibmq15 (15 qubits)`
    - `dummy_ibmq16 (16 qubits)`
    - `dummy_ibmq30 (30 qubits)` 

- `-limited`: Limit the number of qubits used (i.e., avoid using all physical qubits of the device). Due to more limited topology, more gates can be introduced. This option is
particularly useful for numerical simulation on a classical system, given less qubits.

- `-v`: Set the verbose level for debugging:
  - 0 : No output (default)
  - 1 : Output device_name, gate_ops, transpilation time, output file location
  - 2 : Detailed information, including initial_mapping, transpilation time for different steps during the routing/mapping pass

## Data Structure
The central data structure are:

`Circuit`: each transpilation pass takes in a circuit object and applies the logic of the pass to the circuit:
- `n_qubits`: The total number of qubits in the circuit.
- `gates`: A vector storing the gates (or quantum operations) applied within the circuit.

`Gate`: basic data structure for a gate:
- `op_name`: This attribute specifies the type of gate. Examples include 'CX' (CNOT gate), 'Rz' (Pauli-Z rotation gate), etc.
- `ctrl`: This defines the control qubit for controlled operations.
- `qubit`: This represents the target qubit upon which the gate operation is applied.
- `theta/lambda/phi/gama`: These are parameters representing the rotation angle (where applicable) for the gate operation.

## External Files:
QASMTrans includes two external source header files:
- [lexer.hpp](https://github.com/ArashPartow/lexertk): Lexertk, a simple to use, easy to integrate and extremely fast lexicographical generator.
- [json.hpp](https://github.com/nlohmann/json): a C++ json operation library.

## Developers:
- Fei Hua, Pacific Northwest National Laboratory (Main developer)
- Meng Wang, Pacific Northwest National Laboratory
- Ang Li, Pacific Northwest National Laboratory

Thanks to Gushu Li (University of Pennsylvania) for sharing the Python source code of Sabre[2]).


## Citation format:

- Fei Hua, Meng Wang, Gushu Li, Bo Peng, Chenxu Liu, Muqing Zheng, Samuel Stein, Yufei Ding, Eddy Z. Zhang, Travis S. Humble, Ang Li. "QASMTrans: A QASM based Quantum Transpiler Framework for NISQ Devices." arXiv preprint arXiv:2308.07581 (2023)

Bibtex:
```text
@misc{hua2023qasmtrans,
      title={QASMTrans: A QASM based Quantum Transpiler Framework for NISQ Devices}, 
      author={Fei Hua and Meng Wang and Gushu Li and Bo Peng and Chenxu Liu and Muqing Zheng and Samuel Stein and Yufei Ding and Eddy Z. Zhang and Travis S. Humble and Ang Li},
      year={2023},
      eprint={2308.07581},
      archivePrefix={arXiv},
      primaryClass={quant-ph}
}
```

## References
- [1] Cross, A.W., Bishop, L.S., Smolin, J.A., & Gambetta, J.M. (2017). Open quantum assembly language. arXiv preprint [arXiv:1707.03429](https://arxiv.org/abs/1707.03429).
- [2] Li, G., Ding, Y., & Xie, Y. (2019). Tackling the qubit mapping problem for NISQ-era quantum devices. In Proceedings of the Twenty-Fourth International Conference on Architectural Support for Programming Languages and Operating Systems (pp. 1001-1014).(https://dl.acm.org/doi/abs/10.1145/3297858.3304023)

## Acknowledgments
PNNL IPID: 32821-E, IR: PNNL-SA-188499, Export Control: EAR99, Software DOI: 10.11578/dc.20230814.4

This software is supported by the U.S. Department of Energy, Office of Science, National Quantum Information Science Research Centers,
Co-design Center for Quantum Advantage (C2QA) under contract number DE-SC0012704. The software is also supported by the U.S. 
Department of Energy, Office of Science, National Quantum Information Science Research Centers, Quantum Science Center (QSC). This research used
resources of the Oak Ridge Leadership Computing Facility, which is a DOE Office of Science User Facility supported under Contract 
DE-AC05-00OR22725. This research used resources of the National Energy Research Scientific Computing Center (NERSC), a U.S. Department of Energy 
Office of Science User Facility located at Lawrence Berkeley National Laboratory, operated under Contract No. DE-AC02-05CH11231. The Pacific 
Northwest National Laboratory is operated by Battelle for the U.S. Department of Energy under Contract DE-AC05-76RL01830.


