# Integration Tests for Mata

This directory contains the integration tests, benchmarking framework, and associated scripts for the automata library Mata.

## Directory Structure

- `automata/` - Small example automata instances.
- `nfa-bench/` - Submodule of benchmark sets for benchmarking automata operations.
- `inputs/` - Input definitions for benchmarking sets.
- `jobs/` - Configurations for testing jobs.
- `scripts/` - Auxiliary scripts for testing.
- `results/` - Output directory for test and benchmarking results.

## Requirements

The testing suite relies on Python for orchestration and data processing.
See `pyproject.toml` for Python dependencies.
Ensure that the main `mata` library is built correctly before executing integration tests.

## Usage

To use the integration tests infrastructure, see [BENCHMARKING.md](../BENCHMARKING.md).
