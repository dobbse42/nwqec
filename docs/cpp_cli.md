C++ CLI Guide
=============

Overview
--------
The repository provides two C++ command-line tools:
- `./nwqec-cli`: parse OpenQASM, transpile to Clifford+T or PBC, optionally optimize T rotations, count Clifford+T gates without generating the final circuit, and export QASM/statistics.
- `./gridsynth`: synthesize a single RZ angle into a Clifford+T sequence.

**Platform Support:**
NWQEC is supported on macOS and Linux. Both tools are available with automatic prebuilt GMP/MPFR download.

Build Requirements
------------------
- CMake ≥ 3.16 and a C++17 compiler
- GMP/MPFR automatically downloaded from prebuilt binaries

Building
--------
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Optional CMake flags:
- `-DNWQEC_ENABLE_LTO=ON|OFF` (default ON) - Enable link-time optimization
- `-DNWQEC_ENABLE_NATIVE=ON|OFF` (default OFF) - Enable -march=native optimization
- `-DNWQEC_BUILD_PYTHON=ON|OFF` (default OFF) - Build Python bindings

CLI Usage
---------
Basic syntax: `./nwqec-cli [OPTIONS] <INPUT>`

Get help: `./nwqec-cli -h`

### Input Sources
```bash
# Parse QASM file
./nwqec-cli circuit.qasm

# Generate test circuits
./nwqec-cli --qft 4        # QFT circuit with 4 qubits  
./nwqec-cli --shor 3       # Shor test circuit for 3-bit numbers
```

### Transpilation Workflows
The transpilation follows a clear three-step process:

1. **Basic Processing**: `DECOMPOSE` → `REMOVE_TRIVIAL_RZ` → `SYNTHESIZE_RZ`
2. **Choose Format**: Clifford+T (default), PBC, or Clifford Reduction  
3. **Optional Optimization or Analysis**: T-count optimization for PBC, or Clifford+T gate counts without final-circuit generation

```bash
# Default: Clifford+T conversion
./nwqec-cli circuit.qasm

# Clifford+T with CCX gate preservation
./nwqec-cli circuit.qasm --keep-ccx

# Pauli-Based Circuit (PBC) format
./nwqec-cli circuit.qasm --pbc

# PBC with CX gate preservation
./nwqec-cli circuit.qasm --pbc --keep-cx

# PBC with T-count optimization
./nwqec-cli circuit.qasm --pbc --t-opt

# T-count optimization on an existing PBC circuit
./nwqec-cli pbc_circuit.qasm --t-opt

# Count Clifford+T gates without generating the final circuit
./nwqec-cli circuit.qasm --ct-counts

# Short form
./nwqec-cli circuit.qasm -C

# Clifford Reduction optimization
./nwqec-cli circuit.qasm --cr
```

**Important**: Format options (`--pbc`, `--cr`) are mutually exclusive.
T-optimization (`--t-opt`) can be combined with `--pbc`, or used alone when the input is already a PBC circuit.
CX preservation (`--keep-cx`) can only be combined with `--pbc`.
Clifford+T counting (`--ct-counts` or `-C`) cannot be combined with `--pbc`, `--cr`, `--t-opt`, `--remove-pauli`, or `--keep-cx`. It may be combined with `--keep-ccx`, though preserved CCX gates will appear as `ccx` counts rather than decomposed Clifford+T gates.
Clifford Reduction (`--cr`) is based on techniques from Wang et al. "Optimizing FTQC Programs through QEC Transpiler and Architecture Codesign" (2024).

### Output Options
```bash
# Default: saves to <input>_transpiled.qasm
./nwqec-cli circuit.qasm

# Custom output filename
./nwqec-cli circuit.qasm -o my_output.qasm

# Don't save file (display stats only)
./nwqec-cli circuit.qasm --no-save
```

### RZ Synthesis Error
`nwqec-cli` uses a fixed absolute per-RZ synthesis tolerance by default:

```bash
./nwqec-cli circuit.qasm
# equivalent to: --rz-err per-gate --epsilon 1e-10
```

The `--rz-err` policy controls how `--epsilon` is interpreted:

| `--rz-err` | `--epsilon` omitted | With `--epsilon x` |
|---|---:|---:|
| omitted | per-gate `1e-10` | per-gate `x` |
| `per-gate` | per-RZ epsilon `1e-10` | per-RZ epsilon `x` |
| `total` | total budget `1e-2` | total budget `x` |
| `relative` | `abs(theta) * 1e-2` | `abs(theta) * x` |

For `total`, the budget is split evenly over all RZ occurrences after trivial-RZ cleanup.

```bash
# Fixed absolute error per synthesized RZ
./nwqec-cli circuit.qasm --rz-err per-gate --epsilon 1e-12

# Total RZ synthesis error budget
./nwqec-cli circuit.qasm --rz-err total --epsilon 1e-2

# Angle-relative synthesis error
./nwqec-cli circuit.qasm --rz-err relative --epsilon 1e-3
```

### Analysis Options
```bash
# Count the Clifford+T output without generating the final circuit
./nwqec-cli circuit.qasm --ct-counts

# Equivalent short option
./nwqec-cli circuit.qasm -C

# Remove Pauli gates from output
./nwqec-cli circuit.qasm --remove-pauli

# Preserve CCX gates during decomposition
./nwqec-cli circuit.qasm --keep-ccx
```

`--ct-counts` runs the normal Clifford+T workflow but returns gate counts before generating the final circuit. The output contains only gate names and counts that would appear in the resulting circuit.

### Complete Examples
```bash
# Basic transpilation
./nwqec-cli qft_n4.qasm

# PBC with T optimization, custom output
./nwqec-cli circuit.qasm --pbc --t-opt -o optimized.qasm

# Generate QFT, apply Clifford reduction, don't save
./nwqec-cli --qft 8 --cr --no-save

# Exact Clifford+T counts for a large circuit without generating the final circuit
./nwqec-cli large_circuit.qasm --ct-counts --rz-err total --epsilon 1e-2

# Shor circuit with PBC and CX preservation
./nwqec-cli --shor 4 --pbc --keep-cx

# Advanced: PBC with all options
./nwqec-cli large_circuit.qasm --pbc --t-opt --keep-cx --remove-pauli
```

Gridsynth Usage
---------------
**Available on macOS/Linux only**

Syntax: `./gridsynth <angle> [epsilon]`

The optional `epsilon` argument is an absolute error tolerance. If omitted,
the tool defaults to `|theta| * 1e-2`.

```bash
# Synthesize π/8 rotation with ε=1e-12
./gridsynth pi/8 1e-12

# Synthesize π/4 rotation with the default ε=|θ|*1e-2
./gridsynth pi/4

# Synthesize an arbitrary angle with ε=1e-10
./gridsynth 0.785398 1e-10  # approximately π/4
```


Performance Notes
-----------------
- **Large circuits**: QFT >20 qubits or Shor >15 bits may require significant time/memory
- **T optimization**: `--t-opt` can substantially reduce T-count but increases computation time
- **Timing metrics**: The tool reports parsing, transpilation, and file I/O times

Output Format
-------------
- **QASM files**: Standard OpenQASM 2.0 format
- **Statistics**: Gate counts, circuit depth, T-count, and performance metrics
- **Default naming**: Input file with `_transpiled.qasm` suffix

Installation
------------
After building:
```bash
cmake --build build --target install
```

This installs CLI binaries to `${CMAKE_INSTALL_BINDIR}` (typically `/usr/local/bin`) and exports CMake targets under `NWQEC::` namespace for downstream C++ projects.
