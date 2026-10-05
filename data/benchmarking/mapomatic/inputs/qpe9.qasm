OPENQASM 2.0;
include "qelib1.inc";

// 1 ancilla + 8 work qubits (minimal placeholder QPE-style circuit)
qreg q[9];

// Hadamards on ancilla and a few work qubits
h q[0];
h q[1];
h q[2];
h q[3];
h q[4];

// Controlled phase rotations to mimic phase estimation steps
cu1(0.25*pi) q[0],q[5];
cu1(0.5*pi) q[1],q[6];
cu1(1.0*pi) q[2],q[7];
cu1(2.0*pi) q[3],q[8];

// Simple controlled unitary placeholders
cx q[0],q[5];
cx q[1],q[6];
cx q[2],q[7];
cx q[3],q[8];

// Inverse QFT on ancilla register (q[0..4])
// Swap q0<->q4, q1<->q3
swap q[0],q[4];
swap q[1],q[3];

h q[4];
cu1(-pi/2) q[4],q[3];
h q[3];
cu1(-pi/4) q[4],q[2];
cu1(-pi/2) q[3],q[2];
h q[2];
cu1(-pi/8) q[4],q[1];
cu1(-pi/4) q[3],q[1];
cu1(-pi/2) q[2],q[1];
h q[1];
cu1(-pi/16) q[4],q[0];
cu1(-pi/8) q[3],q[0];
cu1(-pi/4) q[2],q[0];
cu1(-pi/2) q[1],q[0];
h q[0];
