import sys
from qiskit import Aer, QuantumCircuit, transpile, assemble
from qiskit.test.mock import FakeToronto

def compare_counts(counts1, counts2, threshold=0.005):
    """Compare two count dictionaries. Consider them the same if the absolute difference
    for each key is less than the threshold times the total shots."""
    total_shots = sum(counts1.values())
    diff_rate = 0
    for key in set(counts1.keys()).union(counts2.keys()):  # Get all keys from both dicts
        diff = abs(counts1.get(key, 0) - counts2.get(key, 0))
        diff_rate += diff / total_shots
    print(f'the fidelity difference is {diff_rate}')
    if diff_rate > threshold:
        return False
    return True

def run_circuit_from_qasm_file(qasm_file_path, backend):
    # Load QASM file into a QuantumCircuit
    with open(qasm_file_path, 'r') as qasm_file:
        qasm_content = qasm_file.read()
        circuit = QuantumCircuit.from_qasm_str(qasm_content)

    # Transpile the circuit for the specified backend
    transpiled_circuit = transpile(circuit, backend=backend,routing_method='sabre')

    # Use Aer's QasmSimulator for simulation
    simulator = Aer.get_backend('qasm_simulator')

    # Assemble the circuits into a Qobj that can be executed
    # qobj = assemble(transpiled_circuit)

    # Execute the circuit on the simulator
    result = simulator.run(transpiled_circuit,shots=8000000).result()

    # Return the result counts
    return result.get_counts(transpiled_circuit)
def run_circuit_from_qasm_file2(qasm_file_path, backend):
    # Load QASM file into a QuantumCircuit
    with open(qasm_file_path, 'r') as qasm_file:
        qasm_content = qasm_file.read()
        circuit = QuantumCircuit.from_qasm_str(qasm_content)

    # Transpile the circuit for the specified backend
    simulator = Aer.get_backend('qasm_simulator')
   

    # Execute the circuit on the simulator
    result = simulator.run(circuit,shots=8000000).result()

    # Return the result counts
    return result.get_counts(circuit)

def main():
    # Check for input arguments
    if len(sys.argv) != 2:
        print("Usage: python script_name.py <path_to_qasm_file>")
        sys.exit(1)

    # Use FakeToronto backend for transpilation
    backend = FakeToronto()

    # Get results for both circuits
    cmd_line_counts = run_circuit_from_qasm_file(sys.argv[1], backend)
    build_output_counts = run_circuit_from_qasm_file2("/Users/huaf990/Library/CloudStorage/OneDrive-PNNL/Documents/sim_test/qasmtrans/build/output.qasm", backend)

    # Compare the results
    if compare_counts(cmd_line_counts, build_output_counts):
        print("The results of the two circuits are considered the same!")
    else:
        print("The results of the two circuits differ!")
        print("Command line circuit results:", cmd_line_counts)
        print("build/output.qasm circuit results:", build_output_counts)

if __name__ == "__main__":
    main()
