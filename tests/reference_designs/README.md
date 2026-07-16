# tests/reference_designs/

This directory is intentionally empty. The automated test suite added for
Phase 1 reads reference/scenario data directly from
`data/reference_designs.csv` and `data/test_scenarios.csv` instead of
duplicating worked examples here as JSON - see `tests/python/test_reference_designs.py`
and `tests/cpp/EngineTests.cpp`.
