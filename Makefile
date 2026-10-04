BUILD_DIR ?= build
JOBS ?= 6
MAKE_FLAGS ?= -j $(JOBS)
# `--no-tests=error`: without it CTest exits 0 when it finds no tests at all, so a build directory
#  configured with `BUILD_TESTING=OFF` silently turns `make test` into a passing run of 0 tests.
TEST_FLAGS ?= -j $(JOBS) --output-on-failure --no-tests=error

# Every target configures the same `$(BUILD_DIR)` by default, and CMake keeps every option it has
#  already seen in `$(BUILD_DIR)/CMakeCache.txt`. A target that leaves an option out therefore
#  inherits whatever an earlier target left there: `make coverage && make release` used to produce a
#  coverage-instrumented Release library, and `make release-lib && make release` (which is what
#  building the Python binding does, in this very directory) built no tests at all.
# Hence every configuring target passes the full set of Mata toggles below, and the specialised
#  targets only override individual values.
MATA_WERROR ?= OFF
MATA_ENABLE_COVERAGE ?= OFF
BUILD_TESTING ?= ON
MATA_BUILD_EXAMPLES ?= ON
MATA_ENABLE_IPO ?= OFF
MATA_ARCH_NATIVE ?= OFF
# `off`, `generate` or `use`; see `just cpp::pgo` and BENCHMARKING.md.
MATA_PGO ?= off
CMAKE_TOGGLES = \
	-DMATA_WERROR:BOOL=$(MATA_WERROR) \
	-DMATA_ENABLE_COVERAGE:BOOL=$(MATA_ENABLE_COVERAGE) \
	-DBUILD_TESTING:BOOL=$(BUILD_TESTING) \
	-DMATA_BUILD_EXAMPLES:BOOL=$(MATA_BUILD_EXAMPLES) \
	-DMATA_ENABLE_IPO:BOOL=$(MATA_ENABLE_IPO) \
	-DMATA_ARCH_NATIVE:BOOL=$(MATA_ARCH_NATIVE) \
	-DMATA_PGO:STRING=$(MATA_PGO)

.PHONY: all debug debug-werror debug-lib release release-werror release-lib release-debuginfo \
	coverage docs test test-coverage test-performance check install uninstall clean

# Rebuilds an already configured build directory.
all:
	@test -f "$(BUILD_DIR)/CMakeCache.txt" || { \
		echo 'No build directory to build: run "make debug" or "make release" first.'; exit 1; }
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything (library, unit tests, integration tests, examples) in debug mode
debug:
	cmake -B $(BUILD_DIR) -S . $(CMAKE_TOGGLES) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything (library, unit tests, integration tests, examples) in debug mode with warnings turned into errors
debug-werror: MATA_WERROR = ON
debug-werror:
	cmake -B $(BUILD_DIR) -S . $(CMAKE_TOGGLES) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds only library in debug mode
debug-lib: BUILD_TESTING = OFF
debug-lib: MATA_BUILD_EXAMPLES = OFF
debug-lib:
	cmake -B $(BUILD_DIR) -S . $(CMAKE_TOGGLES) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything (library, unit tests, integration tests, examples) in release mode
release:
	cmake -B $(BUILD_DIR) -S . $(CMAKE_TOGGLES) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything (library, unit tests, integration tests, examples) in release mode with warnings turned into errors
release-werror: MATA_WERROR = ON
release-werror:
	cmake -B $(BUILD_DIR) -S . $(CMAKE_TOGGLES) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds only library in release mode
release-lib: BUILD_TESTING = OFF
release-lib: MATA_BUILD_EXAMPLES = OFF
release-lib:
	cmake -B $(BUILD_DIR) -S . $(CMAKE_TOGGLES) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything (library, unit tests, integration tests, examples) in release mode with debug information.
release-debuginfo:
	cmake -B $(BUILD_DIR) -S . $(CMAKE_TOGGLES) -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything in debug mode with coverage compiler flags
coverage: MATA_ENABLE_COVERAGE = ON
coverage:
	cmake -B $(BUILD_DIR) -S . $(CMAKE_TOGGLES) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

docs: debug
	$(MAKE) -C $(BUILD_DIR) $(MAKE_FLAGS) docs

# Runs tests
test:
	ctest $(TEST_FLAGS) --test-dir "$(BUILD_DIR)"

# Runs tests and generates coverage report from the results (mata must be built with coverage flags)
test-coverage:
	find "$(BUILD_DIR)" -name '*.gcda' -delete
	ctest $(TEST_FLAGS) --test-dir "$(BUILD_DIR)"
	gcovr --config gcovr.cfg -j $(JOBS) --html-details --output "$(BUILD_DIR)/coverage.html" \
		"$(BUILD_DIR)/src" "$(BUILD_DIR)/tests"

# The job and input files are generated into `$(BUILD_DIR)/tests-integration`, so the benchmarks that
#  run here are always the ones built in `$(BUILD_DIR)`.
BENCH_DIR = $(BUILD_DIR)/tests-integration
PYCOBENCH = ./tests-integration/pycobench/src/pycobench.py
PYCO_PROC = ./tests-integration/pycobench/src/pyco_proc.py
test-performance:
	$(PYCOBENCH) -c "$(BENCH_DIR)/jobs/corr-single-param-jobs.yaml" < "$(BENCH_DIR)/inputs/single-automata.input" -o ./tests-integration/results/corr-single-param-jobs.out
	$(PYCO_PROC) --csv ./tests-integration/results/corr-single-param-jobs.out > ./tests-integration/results/corr-single-param-jobs.csv
	$(PYCOBENCH) -c "$(BENCH_DIR)/jobs/corr-double-param-jobs.yaml" < "$(BENCH_DIR)/inputs/double-automata.input" -o ./tests-integration/results/corr-double-param-jobs.out
	$(PYCO_PROC) --csv --params-num 2 ./tests-integration/results/corr-double-param-jobs.out > ./tests-integration/results/corr-double-param-jobs.csv

# Runs cppcheck over the compilation database of an already configured build directory. Mata always
#  configures with `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, so no reconfiguration is needed here.
check:
	@test -f "$(BUILD_DIR)/compile_commands.json" || { \
		echo 'No compilation database to check: run "make debug" or "make release" first.'; exit 1; }
	cppcheck --project="$(BUILD_DIR)/compile_commands.json" --quiet --error-exitcode=1

install:
	cmake --install $(BUILD_DIR)

uninstall:
	cmake --build $(BUILD_DIR) --target uninstall

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C docs/ clean
	$(MAKE) -C bindings/python/ clean
