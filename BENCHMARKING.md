# Guidelines for Profiling, Performance, and Integration Testing of the Mata library

# Repositories

We have several repositories for all the NFA benchmarks and the benchmarking infrastructure:

- **[VeriFIT/mata](https://github.com/VeriFIT/mata/)**
  - The main repository for Mata: A fast and simple automata library
  - `./tests-integration/`
    - Repository with instructions on how to benchmark Mata using pycobench benchmarking tool
- **[VeriFIT/nfa-bench](https://github.com/VeriFIT/nfa-bench)**
  - Extensive benchmark for reasoning about regular properties
  - This repository contains benchmark automata that can be used to evaluate libraries accepting non-deterministic finite automata in `.mata` format.
- **[VeriFIT/nfa-program-parser](https://github.com/VeriFIT/nfa-program-parser/)**
  - parsers and interpreters for the .emp files
- **[VeriFIT/mata-comparison](https://github.com/VeriFIT/mata-comparison)**
  - Master repo for comparison of the libmata library
- [automata-bench](https://github.com/VeriFIT/automata-bench)
  - Extensive benchmark for reasoning about regular properties
- [automata-benchmarks](https://github.com/ondrik/automata-benchmarks)
  - A repository with benchmarks for automata
  - No .mata format, thus unusable in Mata as-is
  - Could serve as a source of additional benchmarks when converted to .mata
- [mata-comparison-results](https://github.com/VeriFIT/mata-comparison-results)
  - results from Mata comparison with other tools and libraries

# Benchmarking in VeriFIT/mata

We first explain how to compile the library and test binary sources in `./tests-integration/src/`. We will explain how to profile or test performance in separate sections.

First, build the Mata library with the release version:

```sh
make release
```

If you plan to use the existing benchmarking infrastructure in `./tests-integration/`, enter the Python environment in `./tests-integration/`:

```sh
python3 -m venv .venv
pip install -r requirements.txt
source .venv/bin/activate
```

If you want to use Mata as a library called from external programs outside of the VeriFIT/Mata repository, also install Mata as a system library:

```sh
sudo make install
```

## Benchmarking through VeriFIT/mata `./tests-integration/`

We provide a benchmarking infrastructure directly in VeriFIT/mata repository.

### Structure of the benchmarking infrastructure

- `./tests-integration/automata/`: short database of approximately 70 automata of various complexity. The automata come from different
  benchmarks. The `easiest` benchmarks should take less than a second, while `hardest` can take more than 60 seconds.
  The current database is preliminary and might contain errors and issues when running.
- `./tests-integration/src/`: directory with source codes; these will be compiled to `@LIBMATA_ROOT@/build` directory.
  Only sources in the `src/` directory will be compiled using the `cmake` build system!
  You can build upon sources in `src/templates` to start with your own benchmarks or integration tests:
  - `src/templates/template.cc` contains a basic template for arbitrary code to be profiled or integration/performance tested.
  - `src/templates/template-with-cli.args.cc` contains a basic template that takes file as a command line argument, parses the automaton, and computes minterms.
  - `src/templates/template-with-list-of-automata.cc` contains a basic template that without arguments takes a hardcoded list of input automata taken from `automata` directory;
    in case it is run with arguments, it reads the input files that contain paths to source automata.
- `./tests-integration/pycobench`: a performance testing script (see below or `./pycobench --help` for more info)
- `./tests-integration/pyco_proc`: a parser of performance results (see below or `./pyco_proc --help` for more information)
- `./tests-integration/scripts/`: helper scripts used for profiling or testing:
  - `./tests-integration/scripts/run_callgrind.sh` prints the top most time-consuming functions (in terms of exclusive time; outputs only functions with time greater than 1% of overall time spent in program)
  - `./tests-integration/scripts/run_massif.sh` prints the memory peak of the program in `[B]`
- `./tests-integration/jobs/` definitions of jobs for `pycobench`; jobs specify binaries that are run in parallel using `pycobench`; jobs are passed to `pycobench` as `./pycobench -c job.yaml`.
- `./tests-integration/inputs/` definitions of inputs for `pycobench`; inputs specify inputs that are passed to binaries run by `pycobench`; inputs are passed to `pycobench` as `./pycobench < input.input`.

### Creating new benchmarking tests

- To create new tests, the following should be done:
  1. We recommend you to make a copy of one of the `./tests-integration/src/templates/template*.cc` files which contains basic boilerplate code:

     ```sh
     cp ./tests-integration/src/templates/template.cc ./tests-integration/src/<YOUR_FILENAME>.cc
     ```

     where `<YOUR_FILENAME>.cc` source code must be in `./tests-integration/src/` directory; this will ensure it will be compiled using the cmake infrastructure.

  1. If you are using our template, locate the comment `HERE COMES YOUR CODE THAT YOU WANT TO PROFILE` and fill it with the code you want to profile, measure or test.
     - The measurement is performed using our timing macros (namely `TIME_BEGIN` and `TIME_END`).
     - If you want to use our timing macros (`TIME_BEGIN`, `TIME_END`, `TIME_STATEMENT`, or `TIME_BLOCK`) include them from `./tests-integration/src/utils/`.
  1. Create a new `<YOUR-BENCHMARK-NAME>.yaml.in` YAML configuration file in `./tests-integration/jobs/`.
     The file has the following structure:

     ```yaml
     <BENCHMARK_NAME>:
       cmd: @PATH@/<YOUR_FILENAME> [$1 $2 $3...]
     ```

     where the `cmd` contains terminal execution of the binary, the `$1` corresponds to positional arguments that are fed by `pycobench`, `BENCHMARK_NAME` is the name under which the results will be registered (e.g. in the `csv` file, this will correspond to the column).
     - We recommend using the `cmake` variable `@CMAKE_CURRENT_BINARY_DIR@` together with cmake to (re)generate the jobs files with resolved paths (so one can call the `pycobench` from any machine and from any folder).

  1. Create a new input file `.input.in` as a CSV file (with `;` as column separator) with a list of benchmark instances, one instance per row.
     For example, for a binary `<YOUR_FILENAME>` from above which take as parameters 3 arguments, two mata NFAs in `.mata` format, and one number, the format of the input file could be:

     ```text
     path/to/input_automaton_1.mata;path/to/input_automaton_2.mata;41
     path/to/input_automaton_3.mata;path/to/input_automaton_4.mata;42
     path/to/input_automaton_1.mata;path/to/input_automaton_3.mata;43
     ```

     - We recommend using the `cmake` variable `@CMAKE_CURRENT_BINARY_DIR@` together with cmake to (re)generate the inputs files with resolved paths (so one can call the `pycobench` from any machine and from any folder).

  1. Build sources, jobs, and inputs using `make release` or `make debug` from the project root directory or build it yourself using `cmake` from `./tests-integration/` directory.

### Run on one benchmark instance

To run the benchmarking on a single benchmark instance, we use custom binaries written in `./tests-integration/src/` with custom macros (defined in `tests-integration/src/utils/`) to measure specific operations, a sequence of operations, or a single command, etc. See some of the examples in the `./tests-integration/src/`, e.g., `./tests-integration/src/bench-automata-inclusion.cc` for an example of benchmarking automata inclusion operation with two variants of the inclusion algorithm, using the mentioned macros. When the project is compiled, the binary can be run on a single instance like this:

1. Choose a specific binary from `./tests-integration/src/*.cc` (each `.cc` file here has been converted into a single binary). For example, `./tests-integration/bench-automata-inclusion`.

1. Run the binary with the required number of inputs. Here, `./tests-integration/bench-automata-inclusion` requires two input automata in the `.mata` format:

   ```sh
   ./build/tests-integration/bench-automata-inclusion <path/to/automaton1/file>.mata <path/to/automaton2/file>.mata
   ```

   E.g.:

   ```sh
   ./build/tests-integration/bench-automata-inclusion ./tests-integration/automata/b-armc-incl-easiest/aut1.mata ./tests-integration/automata/b-armc-incl-easiest/aut2.mata
   ```

### Run multiple benchmarks through the benchmarking tool `pycobench`

To run the benchmarks, we use our own benchmarking tool `pycobench`, from the main repository `VeriFIT/mata` in the folder `./tests-integration/`.

We suggest using the benchmarks from `VeriFIT/nfa-bench` (added as a submodule to the main repository `VeriFIT/mata` inside `./tests-integration/` directory). The description of the benchmarks can be found in the repository README, and additional information in our [paper from TACAS'24 introducing Mata](https://doi.org/10.1007/978-3-031-57249-4_7).

You can find the exact instances and operations used in the paper in the mata-comparison repository. There, we use a simple automata program format `.emp` (encoding automata operations) that is interpreted by each of the measured libraries on the automata instances passed (`.mata` files) as interpreter arguments. To find the exact instances used in the paper, see, e.g.,:

- `/programs/email-filter.emp` and `/programs/automata-inclusion.emp`
  - `email-filter.emp` loads the automata in the order they were passed as the command line arguments, performs an intersection on the first 4, and checks inclusion of the resulting intersection automaton in the last loaded automaton from the command line.
- `automata-inclusion.emp` just loads two automata in the order they were passed as the command line arguments and performs an inclusion check in the same order.
- `/jobs/tacas-24-email-filter.yaml` and `/jobs/tacas-24-automata-inclusion.yaml`
  - They contain specific commands that are run for a specific benchmark instance (a collection of automata for a single program interpretation run) for each library, including Mata.
- `/inputs/bench-quintuple-email-filter.input` and `/inputs/bench-double-automata-inclusion.input`
  - They contain the paths of the specific automata instances (one line equals one benchmark instance) used in the interpretation of the aforementioned programs, in the order they are defined in the file.

The binaries take files with NFAs written in the `.mata` automata format as a command line input, with one automaton per file. The pycobench tool automates running these binaries:

- with a pre-defined set of benchmark instances, specified in `.input` files inside `tests-integration/inputs/`.
  - One can make their own set of specific benchmark instances by creating a `.input.in` file (see examples there) and recompiling the project (which will create a corresponding `.input` file of the same name), or directly specifying external benchmark instances directly into a new `.input` file.

- with the pre-defined set of operations (executed binaries), specified in `.yaml` configuration files inside `tests-integration/jobs/`.
  - Once can make their own set of operations to be performed (binaries to be executed) by creating a `.yaml.in` file and recompiling the project (which will create a corresponding `.yaml` configuration file of the same name), or directly specifying external operations (binaries) into a new `.yaml` file.

  The format of the configuration file is:

  ```text
  name_of_the_benchmark_test:
    cmd: <path/to/the/binary/to/execute> <INPUTS...>               `
  ```

  where `<INPUTS...>` is a sequence of `.mata` files to be loaded by the binary, passed as `$1` for the first automaton (in the first file), `$2` for the second automaton in the second file, etc.

- with specific options such as setting the timeout, number of parallel jobs, the identifier appended to the results file, etc.

To run a set of benchmark instances on a set of operations through `pycobench`, use the utility script `run_pyco.sh` under `./tests-integration/scripts/`:

```sh
./tests-integration/scripts/run_pyco.sh --config <./tests-integration/jobs/JOBS>.yaml --methods <method1;method2;...;methodn> --timeout <timeout_in_seconds> --jobs <number_of_parallel_jobs> [--suffix "<string_depicting_the_benchmarking_test>"] [<path/to/input_file>.input...]
```

You can specify the following parameters:

1. `--config <jobs/JOBS.yaml>`: picks `YAML` file, where commands for measured binaries are specified.
1. (optional) `--methods <method1;method2;...;methodn>`: list of commands specified in the `YAML` file, which will only be measured; by default, everything is measured.
1. (optional) `--timeout <TIMEOUT>`: timeout in seconds for each binary run; (default = 60s).
1. (optional) `--jobs <JOBS>`: number of parallel jobs that are run; (default = 6 jobs).
1. (optional) ` --suffix <SUFFIX_TAG>`: adds suffix to the generated `.csv` file, for better organization; (default = empty).
1. `<inputs/INPUT.input>`: any number of input files, where the list of automata that are fed into the binaries are listed.

E.g.:

```sh
./tests-integration/scripts/run_pyco.sh --config ./tests-integration/jobs/corr-double-param-jobs.yaml --methods binary --timeout 15 --jobs 12 --suffix "-with_optimized_inclusion_checking" ./tests-integration/inputs/bench-double-automata-inclusion.input
```

<!-- The results are saved in `results` directory in `csv` format. In addition, a short summary is printed at the end of the benchmarking process. -->

<!-- When all tests are run, you can generate CSV files with all the raw results by running a pyco_proc processing tool through a script `tests-integration/scripts/process_pyco.sh` -->

<!-- ```
./tests-integration/process_pyco.sh ../results/<results_file>.csv
``` -->

For additional details, see tests-integration README and our replication package for the TACAS'24 paper introducing Mata.

- Running pychobench manually
  1. Run `./tests-integration/pycobench`:

     ```sh
     ./tests-integration/pycobench -c ./tests-integration/jobs/JOB.yaml -o some.output < tests-integration/inputs/INPUT.input
     ```

     - Note, one can pass any shell command that returns list of automata, e.g. `< ls -1 ./automata/**/aut1.mata`
     - `./tests-integration/pycobench` generates `some.output` file; this format is supported by `./tests-integration/pyco_proc` script that parses the output to `csv`/`html`/`text` format.

  1. Run `./pyco_proc --csv some.output > some.csv` to generate the output `.csv` file (change `csv` to `html` or `text` to generate a different format).

- Alternatively, run `make test-performance` from the project root directory

- this will run selected jobs registered in `jobs/corr-single-param-jobs.yaml` (for binaries accepting single argument) and `jobs/corr-double-param-jobs.yaml` (for binaries accepting two arguments).

- This will generate CSV report of the measurement of binaries registered in `./tests-integration/jobs/*.yaml` files on automata listed in the `./tests-integration/input/*.input` files.

### Additional benchmarking options

- Use the [Catch2 micro-benchmarking](https://github.com/catchorg/Catch2/blob/devel/docs/benchmarks.md) options through VeriFIT/mata `./tests/`

- Use the [`hyperfine`](https://github.com/sharkdp/hyperfine) tool to manually measure the run time of a binary.

## Profiling

We provide the following profiling infrastructure. The following instructions assume a Linux environment. If you are using MacOS, you will have to use a virtual environment or some other profiling tool (e.g., `instruments`).

### Heap and stack space profiling

- Install `massif` and optionally a visualizer for its output files, e.g., `massif-visualizer`.
- run `massif`:

  ```sh
  ./tests-integration/scripts/run_massif.sh ./build/release/tests-integration/bench-automata-inclusion ./tests-integration/automata/b-armc-incl-easiest/aut1.mata ./tests-integration/automata/b-armc-incl-easiest/aut2.mata
  massif-visualizer <massif-output>
  ```

  Alternatively, you can run the corresponding scripts:

  ```sh
  ./tests-integration/scripts/run_massif.sh <...>
  ```

### Callgraph

- Install `valgrind` and optionally a visualizer for its output files, e.g., `kcachegrind` for Linux, `qachegrind` for MacOS.
- Run `valgrind`'s `callgrind` tool:

  ```sh
  valgrind --tool=callgrind -- ./build/release/tests-integration/bench-automata-inclusion ./tests-integration/automata/b-armc-incl-easiest/aut1.mata ./tests-integration/automata/b-armc-incl-easiest/aut2.mata
  kcachegrind <valgrind-output> # On Linux
  qachegrind <valgrind-output> # On MacOS
  ```

  Alternatively, you can run the corresponding scripts:

  ```sh
    ./tests-integration/scripts/run_callgrind.sh <...>
  ```
