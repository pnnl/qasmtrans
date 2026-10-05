OPENQASM 2.0;
include "qelib1.inc";

qreg q[7];

// Toy SAT-like entangling pattern across 7 qubits
h q[0];
h q[1];
h q[2];
h q[3];
h q[4];
h q[5];
h q[6];

cx q[0],q[3];
cx q[1],q[4];
cx q[2],q[5];
ccx q[0],q[1],q[6];
ccx q[2],q[3],q[6];
ccx q[4],q[5],q[6];

rz(0.5) q[6];

ccx q[4],q[5],q[6];
ccx q[2],q[3],q[6];
ccx q[0],q[1],q[6];

// Uncompute
cx q[2],q[5];
cx q[1],q[4];
cx q[0],q[3];
