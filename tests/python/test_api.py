import importlib.util
import math
from pathlib import Path
import pytest


def _require_nwqec():
    spec = importlib.util.find_spec("nwqec")
    if spec is None:
        pytest.skip("nwqec module not installed")
    return spec


def test_import_nwqec():
    _require_nwqec()


def test_load_and_stats(tmp_path):
    _require_nwqec()
    import nwqec

    repo_root = Path(__file__).resolve().parents[2]
    qasm_path = repo_root / "tests" / "python" / "fixtures" / "fixture_circuit.qasm"
    assert qasm_path.exists()

    circuit = nwqec.load_qasm(str(qasm_path))
    assert circuit.num_qubits() > 0
    stats = circuit.stats()
    assert "Circuit Statistics" in stats
    counts = circuit.count_ops()
    assert isinstance(counts, dict)

    clifford = nwqec.to_clifford_t(circuit, rz_err="per-gate", epsilon=1e-6)
    assert clifford.is_clifford_t()

    pbc = nwqec.to_pbc(circuit, rz_err="relative", epsilon=1e-2)
    fused = nwqec.fuse_t(pbc)
    assert fused.count_ops().get("t_pauli", 0) <= pbc.count_ops().get("t_pauli", 0)

    budgeted = nwqec.to_clifford_t(circuit, rz_err="total", epsilon=1e-2)
    assert budgeted.is_clifford_t()

    ct_counts = nwqec.get_clifford_t_counts(circuit, rz_err="per-gate", epsilon=1e-6)
    assert ct_counts.get("t", 0) > 0

    simple_rz = nwqec.Circuit(1)
    simple_rz.rz(0, math.pi / 8)
    simple_counts = nwqec.get_clifford_t_counts(simple_rz, rz_err="per-gate", epsilon=1e-6)
    assert simple_counts.get("t", 0) > 0

    no_rz = nwqec.Circuit(2)
    no_rz.h(0).t(0).cx(0, 1)
    no_rz_counts = nwqec.get_clifford_t_counts(no_rz)
    assert no_rz_counts == no_rz.count_ops()

    with pytest.raises(Exception, match="rz_err"):
        nwqec.to_clifford_t(circuit, rz_err="absolute")

    out_file = tmp_path / "out.qasm"
    clifford.to_qasm_file(str(out_file))
    assert out_file.exists()
