BUILD_DIR=build
MAKE_FLAGS=-j 6
TEST_FLAGS=-j 50 --output-on-failure

.PHONY: all debug debug-werror release release-werror coverage doc clean test test-coverage test-performance

all:
	mkdir -p $(BUILD_DIR)
	$(MAKE) -C $(BUILD_DIR) $(MAKE_FLAGS) || echo "Type either \"make debug\" or \"make release\"!"

# Builds everything (library, unit tests, integration tests, examples) in debug mode
debug:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything (library, unit tests, integration tests, examples) in debug mode with warnings turned into errors
debug-werror:
	cmake -B $(BUILD_DIR) -S . -DWERROR:BOOL=ON -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds only library in debug mode
debug-lib:
	cmake -B $(BUILD_DIR) -S . -DMATA_BUILD_EXAMPLES:BOOL=OFF -DBUILD_TESTING:BOOL=OFF -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything (library, unit tests, integration tests, examples) in release mode
release:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything (library, unit tests, integration tests, examples) in debreleaseug mode with warnings turned into errors
release-werror:
	cmake -B $(BUILD_DIR) -S . -DWERROR:BOOL=ON -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds only library in release mode
release-lib:
	cmake -B $(BUILD_DIR) -S . -DMATA_BUILD_EXAMPLES:BOOL=OFF -DBUILD_TESTING:BOOL=OFF -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything (library, unit tests, integration tests, examples) in release mode with debug information.
release-debuginfo:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

# Builds everything in debug mode with coverage compiler flags
coverage:
	cmake -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=Debug -DMATA_ENABLE_COVERAGE:BOOL=ON
	cmake --build $(BUILD_DIR) --parallel $(MAKE_FLAGS)

doc:
	$(MAKE) -C $(BUILD_DIR) $(MAKE_FLAGS) doc

# Runs tests
test:
	ctest $(TEST_FLAGS) --test-dir "$(BUILD_DIR)"

# Runs tests and generates coverage report from the results (mata must be built with coverage flags)
test-coverage:
	find . -name "**.gcda" -delete
	ctest $(TEST_FLAGS) --test-dir="$(BUILD_DIR)"
	gcovr -p -e "3rdparty/*" -j 6 --exclude-unreachable-branches --exclude-throw-branches $(BUILD_DIR)/src $(BUILD_DIR)/tests

test-performance:
	./tests-integration/pycobench -c ./tests-integration/jobs/corr-single-param-jobs.yaml < ./tests-integration/inputs/single-automata.input -o ./tests-integration/results/corr-single-param-jobs.out
	./tests-integration/pyco_proc --csv ./tests-integration/results/corr-single-param-jobs.out > ./tests-integration/results/corr-single-param-jobs.csv
	./tests-integration/pycobench -c ./tests-integration/jobs/corr-double-param-jobs.yaml < ./tests-integration/inputs/double-automata.input -o ./tests-integration/results/corr-double-param-jobs.out
	./tests-integration/pyco_proc --csv --param-no 2 ./tests-integration/results/corr-double-param-jobs.out > ./tests-integration/results/corr-double-param-jobs.csv

check:
	cd $(BUILD_DIR) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .. && cppcheck --project=compile_commands.json --quiet --error-exitcode=1

install:
	$(MAKE) -C $(BUILD_DIR) install

uninstall:
	$(MAKE) -C $(BUILD_DIR) uninstall

clean:
	cd $(BUILD_DIR) && rm -rf *
	find doc/_build -type f -not \( -name '.nojekyll' \) -exec rm -d {} +
