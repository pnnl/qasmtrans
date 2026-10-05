OPENQASM 2.0;
include "qelib1.inc";

qreg q[8];

// Simple 8-qubit ansatz placeholder
rx(0.3) q[0];
rx(0.5) q[1];
rx(0.7) q[2];
rx(0.9) q[3];
rx(1.1) q[4];
rx(1.3) q[5];
rx(1.5) q[6];
rx(1.7) q[7];

// Entangle in a line
cx q[0],q[1];
cx q[1],q[2];
cx q[2],q[3];
cx q[3],q[4];
cx q[4],q[5];
cx q[5],q[6];
cx q[6],q[7];

// Second layer of single-qubit rotations
rz(0.4) q[0];
rz(0.6) q[1];
rz(0.8) q[2];
rz(1.0) q[3];
rz(1.2) q[4];
rz(1.4) q[5];
rz(1.6) q[6];
rz(1.8) q[7];

// Final entangling sweep
cx q[7],q[6];
cx q[6],q[5];
cx q[5],q[4];
cx q[4],q[3];
cx q[3],q[2];
cx q[2],q[1];
cx q[1],q[0];
