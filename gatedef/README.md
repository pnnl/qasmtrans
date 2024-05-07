# Quantum Gate Library
This page is to collect and define quantum gates that are meaningful to quantum algorithms, domain applications, devices, and operations. This will serve as a standard gate set for qasmtrans and our other tools. Other definitions can be found in [Wiki](https://en.wikipedia.org/wiki/List_of_quantum_logic_gates).

## 1-qubit Gates

#### X Gate
X is a Pauli gate which flips the qubit, also known as NOT gate.
```math
X = \begin{bmatrix} 0 & 1 \\ 1 & 0 \end{bmatrix}
```
#### Y Gate
Y is a Pauli gate that applies both a bit-flip and a phase flip.
```math
Y = \begin{bmatrix} 0 & -i \\ i & 0 \end{bmatrix}
```
#### Z Gate
Z is a Pauli gate known as the phase flip gate.
```math
Z = \begin{bmatrix} 1 & 0 \\ 0 & -1 \end{bmatrix}
```
#### H Gate
H, also known as the Hadamard gate, is a Clifford gate that creates superposition.
```math
H = \frac{1}{\sqrt{2}} \begin{bmatrix} 1 & 1 \\ 1 & -1 \end{bmatrix}
```
#### S Gate
S is a Clifford gate, specifically the square root of Z phase gate.
```math
S = \begin{bmatrix} 1 & 0 \\ 0 & i \end{bmatrix}
```
#### SDG Gate 
SDG is the inverse of the S gate.
```math
SDG = \begin{bmatrix} 1 & 0 \\ 0 & -i \end{bmatrix}
```
#### T Gate
T, also known as the sqrt(S) phase gate or T gate. T is not a Clifford gate and is important for QEC.
```math
T = \begin{bmatrix} 1 & 0 \\ 0 & \frac{\sqrt{2}}{2} + i\frac{\sqrt{2}}{2} \end{bmatrix}
```
#### TDG Gate
TDG, also known as the inverse of the T gate, involves a complex phase adjustment.
```math
TDG = \begin{bmatrix} 1 & 0 \\ 0 & \frac{\sqrt{2}}{2} - i\frac{\sqrt{2}}{2} \end{bmatrix}
```
#### RX Gate 
The RX gate performs a rotation around the X axis by an angle ($\theta$). 
```math
RX(\theta) = \begin{bmatrix} \cos(\theta/2) & -i\sin(\theta/2) \\ -i\sin(\theta/2) & \cos(\theta/2) \end{bmatrix}
```
#### RY Gate
The RY gate performs a rotation around the Y axis by an angle $\theta$. 
```math
RY(\theta)= \begin{bmatrix} \cos(\theta/2) & -\sin(\theta/2) \\ \sin(\theta/2) & \cos(\theta/2) \end{bmatrix}
```
#### RZ Gate 
The RZ gate performs a rotation around the Z axis by an angle $\theta$. IBMQ uses a virtual implementation of the RZ gate, see VZ gate.
```math
RZ(\theta) = \begin{bmatrix} \cos(\theta/2) - i\sin(\theta/2) & 0 \\ 0 & \cos(\theta/2) + i\sin(\theta/2) \end{bmatrix}
```

#### SX Gate
The SX gate, also known as the sqrt(X) gate, performs a $\sqrt{X}$ operation, which is a half-way rotation around the X-axis on the Bloch sphere.
```math
SX = \begin{bmatrix} \frac{1+i}{2} & \frac{1-i}{2} \\ \frac{1-i}{2} & \frac{1+i}{2} \end{bmatrix}
```
#### P Gate
The P gate, also known as the phase gate, applies a phase shift $\theta$ to the state of a qubit. It is not a global phase gate but specifically shifts the phase of the |1⟩ state.
```math
P(\theta) = \begin{bmatrix} 1 & 0 \\ 0 & \cos(\theta) + i\sin(\theta) \end{bmatrix}
```
#### U Gate 
The U gate is a general unitary operation defined by three parameters: $\alpha$, $\beta$, and $\gamma$. It represents a comprehensive rotation in the Bloch sphere that can achieve any single qubit quantum gate.
```math
U(\alpha, \beta, \gamma) = \begin{bmatrix}
\cos(\alpha/2) & -(\cos(\gamma) + i\sin(\gamma))\sin(\alpha/2) \\
(\cos(\beta) + i\sin(\beta))\sin(\alpha/2) & (\cos(\beta+\gamma) + i\sin(\beta+\gamma))\cos(\alpha/2)
\end{bmatrix}
```
Quantinuum devices uses the U2 gates with two parameters $\theta$, $\psi, as one of the one-qubit basis gates. See [Quantinuum System Model H1 Product Data Sheet](https://assets.website-files.com/617730fbcf7b7c387194556a/62a8f7244596df4d854c2222_Quantinuum%20H1%20Product%20Data%20Sheet%20v5%2014JUN22.pdf).
```math
U_{1q}(\theta, \psi) = \begin{bmatrix}
\cos(\theta/2) & -\sin(\theta/2)\sin(\psi)-i\sin(\theta/2)cos(\psi) \\
\sin(\theta/2)\cos(\psi)-i\sin(\theta/2)\cos(\psi) & 0
\end{bmatrix}
```


#### ID Gate 
The ID gate leaves the state of a qubit unchanged. It is used to maintain the coherence of qubits that are not currently used or for padding in quantum circuits to align the timing of operations across different qubits. 
```math
\text{ID} = \begin{bmatrix} 1 & 0 \\ 0 & 1 \end{bmatrix}
```
Sometimes, the ID gate can take a timing parameter so that relaxation noise can be modeled.
```math
\text{ID}(t) = \begin{bmatrix} 1 & 0 \\ 0 & 1 \end{bmatrix}
```
#### GPI Gate
The GPI gate performs a π or bit-flip rotation with an embedded phase. It always rotates π radians—hence the name—but can rotate on any longitudinal axis of the Bloch sphere. At a ϕ of 0 this is equivalent to an X gate, and at a ϕ of 0.25 turns (π/2 radians) it’s equivalent to a Y gate, but it can also be mapped to any other azimuthal angle. It is one of the one-qubit basis gates for IonQ devices. See their [doc](https://ionq.com/docs/getting-started-with-native-gates). It is physically implemented as a Rabi oscillation made with a two-photon Raman transition, i.e. driving the qubits on resonance using a pair of lasers in a Raman configuration. See [paper](https://www.nature.com/articles/s41467-019-13534-2).
```math
\text{GPI}(\phi) = \begin{bmatrix}
\cos(\phi/2) - i\sin(\phi/2) & 0 \\
0 & \cos(\phi/2) + i\sin(\phi/2)
\end{bmatrix}
```

#### GPI2 Gate
The GPi2 gate could be considered an RX(π/2) — or RY(π/2) — with an embedded phase. It always rotates π/2 radians but can rotate on any longitudinal axis of the Bloch sphere. At a ϕ of π this is equivalent to RX(π/2), at a ϕ of 0.25 turns (π/2 radians) it’s equivalent to RY(π/2), but it can also be mapped to any other azimuthal angle. It is one of the one-qubit basis gates for IonQ devices. See their [doc](https://ionq.com/docs/getting-started-with-native-gates). It is physically implemented as a Rabi oscillation made with a two-photon Raman transition, i.e. driving the qubits on resonance using a pair of lasers in a Raman configuration. See [paper](https://www.nature.com/articles/s41467-019-13534-2).
```math
\text{GPI2}(\phi) = \begin{bmatrix}
1 & -\sin(\phi)-i\cos(\phi) \\
\sin(\phi)-i\cos(\phi) & 1
\end{bmatrix}
```
#### VZ Gate
IonQ does not expose or implement a Z gate, where it waits for the phase to advance in time, but a Virtual RZ can be performed by simply advancing or retarding the phase of the following operation in the circuit. This does not physically change the internal state of the trapped ion at the time of its implementation; it changes the phases of the future operations such that it is equivalent to an actual Z-rotation around the Bloch sphere. In effect, virtual RZ operations are implied by the phase inputs to later gates. See [doc](https://ionq.com/docs/getting-started-with-native-gates). IBMQ uses similar strategy for their RZ basis gate (known as Virtual Z gate or VZ gate) of the superconducting devices. See detail in [paper](https://arxiv.org/pdf/1612.00858).
```math
\text{VZ} = \begin{bmatrix}
1 & -\sin(\phi)-i\cos(\phi) \\
\sin(\phi)-i\cos(\phi) & 1
\end{bmatrix}
```

 


## 2-qubit Gates
#### CX Gate
The Controlled X gate, also known as CNOT, is used to apply an X gate to a target qubit when the control qubit is in the state |1⟩. This gate is essential for creating quantum entanglement and implementing conditional logic in quantum circuits. It is one of the basis gates for IBMQ devices.
```math
CX = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & 0 & 1 \\
0 & 0 & 1 & 0
\end{bmatrix}
```
#### CY Gate 
The Controlled Y gate, applies a Y gate to a target qubit when the control qubit is set to |1⟩. It plays a crucial role in quantum circuits where conditional phase flips combined with bit flips are necessary.
```math
CY = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & 0 & -i \\
0 & 0 & i & 0
\end{bmatrix}
```
#### CZ Gate 
The Controlled Z gate, applies a Z gate (Pauli-Z gate) to a target qubit when the control qubit is set to |1⟩. This gate is essential for phase manipulation in quantum algorithms and is widely used in creating quantum entanglement. It is one of the basis gates for latest IBMQ devices such as ibm_torino.
```math
CZ = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & 1 & 0 \\
0 & 0 & 0 & -1
\end{bmatrix}
```
#### CH Gate 
The Controlled H gate, applies a Hadamard gate to a target qubit when the control qubit is set to |1⟩. It's used in complex quantum operations that require conditional superposition, serving as a critical component in algorithms that exploit quantum parallelism and entanglement.
```math
CH = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & \frac{1}{\sqrt{2}} & \frac{1}{\sqrt{2}} \\
0 & 0 & \frac{1}{\sqrt{2}} & -\frac{1}{\sqrt{2}}
\end{bmatrix}
```
#### CS Gate 
The Controlled S gate, applies an S gate to a target qubit when the control qubit is set to |1⟩. The S gate applies a phase shift of $\pi/2$, making the CS gate vital for phase control in multi-qubit operations, particularly in algorithms requiring phase kickback or quantum Fourier transforms.
```math
CS = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & 1 & 0 \\
0 & 0 & 0 & i
\end{bmatrix}
```
#### CSDG Gate 
The Controlled SDG gate, applies an SDG gate to a target qubit when the control qubit is set to |1⟩. The SDG gate applies a phase shift of $-\pi/2$, making the CSDG gate essential for precise phase control and corrections, especially those containing inverse phase adjustments.
```math
CSDG = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & 1 & 0 \\
0 & 0 & 0 & -i
\end{bmatrix}
```
#### CT Gate 
The Controlled T gate, applies a T gate to a target qubit when the control qubit is set to |1⟩. The T gate introduces a phase shift of $\pi/4$, making the CT gate  useful in algorithms that require detailed phase control, such as QEC and certain QFT applications.
```math
CT = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & 1 & 0 \\
0 & 0 & 0 & \frac{\sqrt{2}}{2} + i\frac{\sqrt{2}}{2}
\end{bmatrix}
```
#### CTDG Gate
The Controlled TDG gate, applies a TDG gate to a target qubit when the control qubit is set to |1⟩. The TDG gate introduces a phase shift of $-\pi/4$, which is essential for undoing phase operations in quantum algorithms, particularly in processes that reverse operations for error correction and algorithm symmetry.
```math
CTDG = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & 1 & 0 \\
0 & 0 & 0 & \frac{\sqrt{2}}{2} - i\frac{\sqrt{2}}{2}
\end{bmatrix}
```
#### CRX Gate
The Controlled RX gate, applies an RX gate to a target qubit when the control qubit is set to |1⟩. The RX gate performs a rotation around the X axis by an angle $\alpha$.
```math
CRX(\alpha) = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & \cos(\alpha/2) & -i\sin(\alpha/2) \\
0 & 0 & -i\sin(\alpha/2) & \cos(\alpha/2) 
\end{bmatrix}
```
#### CRY Gate 
The Controlled RY gate, applies an RY gate to a target qubit when the control qubit is set to |1⟩. The RY gate performs a rotation around the Y axis by an angle $\alpha$.
```math
CRY(\alpha) = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & \cos(\alpha/2) & -\sin(\alpha/2) \\
0 & 0 & \sin(\alpha/2) & \cos(\alpha/2)
\end{bmatrix}
```
#### CRZ Gate 
The Controlled RZ gate, applies an RZ gate to a target qubit when the control qubit is set to |1⟩. The RZ gate performs a rotation around the Z axis by an angle $\alpha$.
```math
CRZ = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & \cos(\theta/2) - i\sin(\theta/2) & 0 \\
0 & 0 & 0 & \cos(\theta/2) + i\sin(\theta/2)
\end{bmatrix}
```
#### CSX Gate
The Controlled sqrt(X) gate. This gate applies the square root of the Pauli-X operation (SX) when the control qubit is set to 1.
```math
CSX = \begin{bmatrix} 1 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \\ 0 & 0 & \frac{1+i}{2} & \frac{1-i}{2} \\ 0 & 0 & \frac{1-i}{2} & \frac{1+i}{2} \end{bmatrix}
````

#### CP Gate
The Controlled Phase gate. This gate applies a phase operation \( \cos(\theta) + i\sin(\theta) \) when the control qubit is set to 1.
```math
CP = \begin{bmatrix} 1 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & \cos(\theta) + i\sin(\theta) \end{bmatrix}
````

#### CU Gate
The Controlled U gate. This gate applies a unitary operation U($\alpha$, $\beta$, and $\gamma$) when the control qubit is set to 1. The parameters $\alpha$, $\beta$, and $\gamma$ define specific rotations or transformations within the unitary operation.
```math
CU = \begin{bmatrix} 1 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \\ 0 & 0 & \cos(\alpha/2) & -(\cos(\gamma) + i\sin(\gamma))\sin(\alpha/2) \\ 0 & 0 & \cos(\beta) + i\sin(\beta))\sin(\alpha/2) & (\cos(\beta+\gamma) + i\sin(\beta+\gamma))\cos(\alpha/2) \end{bmatrix}
```

#### RXX Gate
The two-qubit rotation gate along XX-axis, also known as XX gate. It's similar to RX, but operates on two qubits simultaneously. 
```math
RXX(\theta) = \begin{bmatrix}
\cos(\theta/2) & 0 & 0 & -i\sin(\theta/2) \\
0 & \cos(\theta/2) & -i\sin(\theta/2) & 0 \\
0 & -i\sin(\theta/2) & \cos(\theta/2) & 0 \\
-i\sin(\theta/2) & 0 & 0 & \cos(\theta/2)
\end{bmatrix}
```

#### RYY Gate
The two-qubit rotation gate along YY-axis, also known as YY gate. It's similar to RY, but operates on two qubits simultaneously. 
```math
RYY(\theta) = \begin{bmatrix}
\cos(\theta/2) & 0 & 0 & i\sin(\theta/2) \\
0 & \cos(\theta/2) & -i\sin(\theta/2) & 0 \\
0 & -i\sin(\theta/2) & \cos(\theta/2) & 0 \\
i\sin(\theta/2) & 0 & 0 & \cos(\theta/2)
\end{bmatrix}
```
#### RZZ Gate
The two-qubit rotation gate along ZZ-axis, also known as ZZ gate. It's similar to RZ, but operates on two qubits simultaneously. 
```math
RZZ(\theta) = \begin{bmatrix}
\cos(\theta/2) - i\sin(\theta/2) & 0 & 0 & 0 \\
0 & \cos(\theta/2) + i\sin(\theta/2) & 0 & 0 \\
0 & 0 & \cos(\theta/2) + i\sin(\theta/2) & 0 \\
0 & 0 & 0 & \cos(\theta/2) - i\sin(\theta/2)
\end{bmatrix}
```
Quantinuum devices use ZZ with certain degree as the two-qubit native entanglement gate.
```math
ZZ = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & i & 0 & 0 \\
0 & 0 & i & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
```


#### SWAP Gate
A two-qubit gate that exchanges the states of two qubits. It is a crucial gate for routing when two remote qubits try to perform operations.
```math
SWAP = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 0 & 1 & 0 \\
0 & 1 & 0 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
```

#### iSWAP Gate
A two-qubit gate that exchanges the states of two qubits and introduces a phase of i (the imaginary unit) to the swapped states. The gate is useful when phase relationships are critical, such as in QFT.
```math
iSWAP = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & 0 & i & 0 \\
0 & i & 0 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
```
#### siSWAP Gate
The square root of the iSWAP gate. It performs half of the operation of the iSWAP gate. It's useful when intermediate entanglement levels are needed. It swaps the amplitudes of two qubits but introduces a phase of π/4 rather than π/2 which is for the full iSWAP gate.
```math
siSWAP = \sqrt{\text{iSWAP}} = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & \frac{1}{\sqrt{2}} & \frac{i}{\sqrt{2}} & 0 \\
0 & \frac{i}{\sqrt{2}} & \frac{1}{\sqrt{2}} & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
```

#### ECR Gate
The Echoed Cross-Resonance (ECR) gate is one of the two-qubit basis gates for IBMQ and is particularly used in superconducting systems. It is derived from the cross-resonance (CR) gate, where an additional echo pulse is used to mitigate unwanted interactions and errors. See [Qiskit Doc](https://docs.quantum.ibm.com/api/qiskit/qiskit.circuit.library.ECRGate).
```math
ECR  = \frac{1}{\sqrt{2}}\begin{bmatrix}
0 & 1 & 0 & i \\
0 & 0 & i & 1 \\
1 & -i & 0 & 0 \\
-i & 1 & 0 & 0
\end{bmatrix}
```

#### G Gate
The Givens Rotation gate, also known as Givens gate or G gate, is used to perform rotations in a two-dimensional subspace. A Givens gate is useful for state preparation and manipulation for quantum chemistry.
```math
G(\theta) = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & \cos(\theta) & -\sin(\theta) & 0 \\
0 & \sin(\theta) & \cos(\theta) & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
```

#### B Gate 
The B gate is a two-qubit unitary operation that performs the diagonalization on a pair of quantum states and then measures in the computational basis, for performing [virtual distillation](https://arxiv.org/pdf/2011.07064.pdf).

```math
B = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & \frac{\sqrt{2}}{2} & -\frac{\sqrt{2}}{2} & 0 \\
0 & \frac{\sqrt{2}}{2} & \frac{\sqrt{2}}{2} & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
```
#### W Gate 
The W gate, which is for QLA, can be found on page 34 of [paper](https://arxiv.org/pdf/1505.06552.pdf). It is defined as W(1,2)=CX(2,1)CX(1,2)CH(1,2)CX(1,2)CX(2,1).
```math
W = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & \frac{\sqrt{2}}{2} & \frac{\sqrt{2}}{2} & 0 \\
0 & \frac{\sqrt{2}}{2} & -\frac{\sqrt{2}}{2} & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
```



#### MS Gate 
The two-qubit Mølmer-Sørenson gate along with single-qubit gates constitutes a universal gate set. For IonQ devices, by irradiating any two ions in the chain with a predesigned set of pulses, they can couple the ions’ internal states with the chain’s normal modes of motion to create entanglement. The two-qubit MS gate is the native two-qubit basis gate of IonQ, see [doc](https://ionq.com/docs/getting-started-with-native-gates). 

For fully entangling MS gate, which is an XX gate. It has two parameters. The first phase parameter refers to the first qubit’s phase as it acts on the second one, the second refers to the second qubit’s phase as it acts on the first one.
```math
\text{FMS}(\phi,\psi) = \begin{bmatrix}
1 & 0 & 0 & -\sin(\phi+\psi)-i\cos(\phi+\psi) \\
0 & 1 & -\sin(\phi-\psi)-i\cos(\phi-\psi) & 0 \\
0 & \sin(\phi-\psi)-i\cos(\phi-\psi) & 1 & 0 \\
\sin(\phi+\psi)-i\cos(\phi+\psi) & 0 & 0 & 1
\end{bmatrix}
```
The partially entangling MS gate is useful in some cases. A third (optional) arbitrary angle θ is added.
```math
\text{PMS}(\phi,\psi,\theta) = \begin{bmatrix}
\cos(\theta/2) & 0 & 0 & -\sin(\phi+\psi)\sin(\theta/2)-i\cos(\phi+\psi)\sin(\theta/2) \\
0 & \cos(\theta/2) & -\sin(\phi-\psi)\sin(\theta/2)-i\cos(\phi-\psi)\sin(\theta/2) & 0 \\
0 & \sin(\phi-\psi)\sin(\theta/2)-i\cos(\phi-\psi)\sin(\theta/2) & \cos(\theta/2) & 0 \\
\sin(\phi+\psi)\sin(\theta/2)-i\cos(\phi+\psi)\sin(\theta/2) & 0 & 0 & \cos(\theta/2)
\end{bmatrix}
```


## 3-qubit Gates

#### CCX Gate 
The Controled-Controlled NOT gate, or CCX, is also known as Toffoli gate. It performs a NOT operation on the third qubit (target) only when the first two qubits (controls) are both in |1⟩. CCX is key for realizing classical logical operations in a reversible manner. It is a universal gate.
```math
CCX = \begin{bmatrix}
1 & 0 & 0 & 0 & 0 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 & 0 & 0 & 0 & 0 \\
0 & 0 & 1 & 0 & 0 & 0 & 0 & 0 \\
0 & 0 & 0 & 1 & 0 & 0 & 0 & 0 \\
0 & 0 & 0 & 0 & 1 & 0 & 0 & 0 \\
0 & 0 & 0 & 0 & 0 & 1 & 0 & 0 \\
0 & 0 & 0 & 0 & 0 & 0 & 0 & 1 \\
0 & 0 & 0 & 0 & 0 & 0 & 1 & 0
\end{bmatrix}
```

#### CSWAP Gate 
The Controlled-SWAP gate, also known as the Fredkin gate, is to swap the second and third qubits if the first control qubit is in ∣1⟩. It is also a universal gate which is used in reversible computing and particularly in QRAM. 
```math
CSWAP = \begin{bmatrix}
1 & 0 & 0 & 0 & 0 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 & 0 & 0 & 0 & 0 \\
0 & 0 & 1 & 0 & 0 & 0 & 0 & 0 \\
0 & 0 & 0 & 1 & 0 & 0 & 0 & 0 \\
0 & 0 & 0 & 0 & 1 & 0 & 0 & 0 \\
0 & 0 & 0 & 0 & 0 & 0 & 1 & 0 \\
0 & 0 & 0 & 0 & 0 & 1 & 0 & 0 \\
0 & 0 & 0 & 0 & 0 & 0 & 0 & 1
\end{bmatrix}
```

#### RCCX Gate
The RCCX gate, also known as the Relative Phase Toffoli or Margolus gate, is a simplified Toffoli gate. It implements the Toffoli gate up to relative phases. 
```math
RCCX = \begin{bmatrix}
1 & 0 & 0 & 0 & 0 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 & 0 & 0 & 0 & 0 \\
0 & 0 & 1 & 0 & 0 & 0 & 0 & 0 \\
0 & 0 & 0 & 1 & 0 & 0 & 0 & 0 \\
0 & 0 & 0 & 0 & 1 & 0 & 0 & 0 \\
0 & 0 & 0 & 0 & 0 & 1 & 0 & 0 \\
0 & 0 & 0 & 0 & 0 & 0 & 0 & 1 \\
0 & 0 & 0 & 0 & 0 & 0 & 1 & 0 
\end{bmatrix}
```



## Multi-qubit Gates
