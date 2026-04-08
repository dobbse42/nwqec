import nwqec
import pytest
import os

def test_is_clifford_t_sx_sxdg():
    """Test that SX and SXDG are recognized as Clifford gates."""
    # SX gate
    qasm_sx = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
sx q[0];
"""
    with open("temp_sx.qasm", "w") as f:
        f.write(qasm_sx)
    
    try:
        circuit_sx = nwqec.load_qasm("temp_sx.qasm")
        assert circuit_sx.is_clifford_t(), "SX gate should be recognized as Clifford"
    finally:
        if os.path.exists("temp_sx.qasm"):
            os.remove("temp_sx.qasm")
    
    # SXDG gate
    qasm_sxdg = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
sxdg q[0];
"""
    with open("temp_sxdg.qasm", "w") as f:
        f.write(qasm_sxdg)
    
    try:
        circuit_sxdg = nwqec.load_qasm("temp_sxdg.qasm")
        assert circuit_sxdg.is_clifford_t(), "SXDG gate should be recognized as Clifford"
    finally:
        if os.path.exists("temp_sxdg.qasm"):
            os.remove("temp_sxdg.qasm")

def test_pbc_error_propagation():
    """Test that to_pbc raises an exception when encountering an unsupported gate."""
    # ID is not currently supported in VTab's Clifford set
    qasm_id = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[1];
id q[0];
"""
    with open("temp_id.qasm", "w") as f:
        f.write(qasm_id)
    
    try:
        circuit_id = nwqec.load_qasm("temp_id.qasm")
        with pytest.raises(RuntimeError) as excinfo:
            nwqec.to_pbc(circuit_id)
        assert "Non-Clifford Gate encountered" in str(excinfo.value)
    finally:
        if os.path.exists("temp_id.qasm"):
            os.remove("temp_id.qasm")

def test_parse_error_propagation():
    """Test that load_qasm raises an exception on invalid QASM."""
    invalid_qasm = "OPENQASM 2.0;\ninvalid_statement;"
    with open("temp_invalid.qasm", "w") as f:
        f.write(invalid_qasm)
    
    try:
        with pytest.raises(RuntimeError) as excinfo:
            nwqec.load_qasm("temp_invalid.qasm")
        assert "Parse error" in str(excinfo.value)
    finally:
        if os.path.exists("temp_invalid.qasm"):
            os.remove("temp_invalid.qasm")
