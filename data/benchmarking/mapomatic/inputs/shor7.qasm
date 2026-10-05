OPENQASM 2.0;
include "qelib1.inc";

qreg q[7];

// Simple Shor-like placeholder: create GHZ-style state on 7 qubits
h q[0];
cx q[0],q[1];
cx q[1],q[2];
cx q[2],q[3];
cx q[3],q[4];
cx q[4],q[5];
cx q[5],q[6];

// Add some phase rotations to mimic modular exponentiation steps
rz(0.2*pi) q[1];
rz(0.4*pi) q[2];
rz(0.6*pi) q[3];
rz(0.8*pi) q[4];
rz(1.0*pi) q[5];
rz(1.2*pi) q[6];

// Lightweight QFT on a subset (q0..q3) as a placeholder
swap q[0],q[3];
h q[3];
cu1(pi/2) q[3],q[2];
h q[2];
cu1(pi/4) q[3],q[1];
cu1(pi/2) q[2],q[1];
h q[1];
cu1(pi/8) q[3],q[0];
cu1(pi/4) q[2],q[0];
cu1(pi/2) q[1],q[0];
h q[0];
