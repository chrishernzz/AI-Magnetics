"""
Loads the real material/core/core-loss-coefficient snapshot into magnetics_cpp so
tests/python/*.py can call magnetics_cpp.run_inductor_design directly without
spinning up the HTTP server. Calls python/app.py's actual startup hook
(load_real_magnetics_data) directly rather than re-implementing the field
mapping here a second time - a previous version of this fixture did duplicate
that mapping by hand and had drifted out of sync (missing the core-loss-coefficient
load entirely, and the vendor field on cores), which silently made every core-loss-
dependent code path untestable through pytest. Calling the real function means
this fixture can never drift from what the FastAPI app actually loads again.
"""
import sys
from pathlib import Path

import pytest

PYTHON_DIR = Path(__file__).resolve().parents[2] / "python"
sys.path.insert(0, str(PYTHON_DIR))


@pytest.fixture(scope="session", autouse=True)
def load_real_data():
    from app import load_real_magnetics_data

    load_real_magnetics_data()
