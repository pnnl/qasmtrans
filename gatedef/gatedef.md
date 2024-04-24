# Quantum Gate Library
This repository is to collect and define quantum gates that are meaningful to quantum algorithms, domain applications, devices, and operations. This will serve as a standard for the support of our software tools.

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
The RZ gate performs a rotation around the Z axis by an angle $\theta$.
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


## 2-qubit Gates
#### CX Gate
The Controlled X gate, also known as CNOT, is used to apply an X gate to a target qubit when the control qubit is in the state |1⟩. This gate is essential for creating quantum entanglement and implementing conditional logic in quantum circuits.
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
The Controlled Z gate, applies a Z gate (Pauli-Z gate) to a target qubit when the control qubit is set to |1⟩. This gate is essential for phase manipulation in quantum algorithms and is widely used in creating quantum entanglement.
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


## 3-qubit Gates



## Multi-qubit Gates
