OPENQASM 2.0;
include "qelib1.inc";

qreg q[3];

// Initial state setup for testing
x q[0];  // Set first control qubit to |1⟩
x q[1];  // Set second control qubit to |1⟩
// q[2] is set to |0⟩

// Apply rccx
rccx q[0],q[1],q[2];

// Measure the qubits
creg c[3];
measure q[0] -> c[0];
measure q[1] -> c[1];
measure q[2] -> c[2];
