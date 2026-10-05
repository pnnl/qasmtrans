OPENQASM 2.0;
include "qelib1.inc";

// Simple BB84-style 8-qubit placeholder: prepare random bases and entangle pairs.
qreg q[8];

h q[0];
h q[1];
h q[2];
h q[3];

// Basis choices (X-basis on half)
h q[4];
h q[5];

// Entangle pairs
cx q[0],q[4];
cx q[1],q[5];
cx q[2],q[6];
cx q[3],q[7];

// Some phase kicks
rz(0.3) q[4];
rz(0.6) q[5];
rz(0.9) q[6];
rz(1.2) q[7];

// Swap back
cx q[0],q[4];
cx q[1],q[5];
cx q[2],q[6];
cx q[3],q[7];
