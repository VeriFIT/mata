# Guidelines for Profiling, Performance and Integration Testing of `libmata` library

## Structure of the infrastructure

  1. `automata/`: short database of approximately 70 automata of various complexity. The automata come from different
    benchmarks. `easiest` benchmarks should take less than second, while, `hardest` can take over 60 seconds.
    Current database is preliminary and might contain errors and issues when running. 
  2. `src/`: directory with source codes; these will be compiled to `@LIBMATA_ROOT@/build` directory. 
    Only sources in `src/` directory will be compiled using `cmake` build system!
    You can build upon sources in `src/templates` to start with your own benchmarks or integration tests:
      * `src/templates/template.cc` contains basic template for arbitrary code to be profiled or integration/performance tested.
      * `src/templates/template-with-cli.args.cc` contains basic template that takes file as a command line argument, parses the automaton and computes minterms.
      * `src/templates/template-with-list-of-automata.cc` contains basic template that without arguments takes a hardcoded list of input automata taken from `automata` directory; 
     in case it is run with arguments, it reads the input files that contains paths to source automata.
  3. `pycobench`: a performance testing script (see below or `./pycobench --help` for more info)
  4. `pyco_proc`: a parser of performance results (see below or `./pyco_proc --help` for more info)
  5. `scripts/`: helper scripts used for profiling or testing:
      * `scripts/run_callgrind.sh` prints top most time-consuming functions (in terms of exclusive time; outputs only functions with time greater than 1% of overall time spent in program)
      * `scripts/run_massif.sh` prints memory peak of the program in `[B]`
  6. `jobs` definitions of jobs for `pycobench`; jobs specify binaries that are run in parallel using `pycobench`; jobs are passed to `pycobench` as `./pycobench -c job.yaml`.
  7. `inputs` definitions of inputs for `pycobench`; inputs specify inputs that are passed to binaries run by `pycobench`; inputs are passed to `pycobench` as `./pycobench < input.input`.

## How to start with the infrastructure `libmata`

We first explain, how to compile sources in the `tests-integration` infrastructure. We will explain, how to profile or test performance in separate sections.

  1. We recommend you to make a copy of one of the `src/templates/template*.cc` files which contains basic boilerplate code, 
    (e.g. from `tests-integration` directory run `cp src/templates/template.cc src/<YOUR_FILENAME>.cc`).
      * Note, that your `<YOUR_FILENAME>.cc` source code has to be in `src/` directory; this will ensure it will be compiled using the cmake infrastructure.
  2. If you are using our template, locate the comment `HERE COMES YOUR CODE THAT YOU WANT TO PROFILE` 
    and fill it with code you want to profile, measure or test. 
    The measurement is performed using our timing macros (namely `TIME_BEGIN` and `TIME_END`).
      *  If you want to use our timing macros (`TIME_BEGIN`, `TIME_END`, `TIME_STATEMENT`, or `TIME_BLOCK`) include 
  3. Build the sources using `make release` or `make debug` from the project root directory or build it yourself using `cmake` from `tests-integration` directory.
 
### How to start profiling `libmata`

In order to profile the code you just built by the above steps:
 
  1. If you are using Linux, you can run `callgrind`. 
      * We recommend you to use one of the scripts to run `callgrind` or `massif` (`scripts/run_callgrind.sh` or `scripts/run_massif.sh`) to measure time or memory respectively.
  2. If you are using MacOS, you will have to use virtual environment or some other profiling tool (e.g. `instruments`) 

### How to test performance

We assume, this is executed from `tests-integration` directory.
In order to test or measure performance of the code you just built by the above steps:

   1. Add your new binaries to one of the yaml files in `jobs/*.yaml`.
      * The `cmd` contains terminal execution of the binary, the `$1` corresponds to positional arguments that are fed by `pycobench`, `new-binary` is the name under which the results will be registered (e.g. in the `csv` file, this will correspond to the column).
      * We recommend you to modify the `jobs/*.yaml.in` files, and use the `cmake` variable `@CMAKE_CURRENT_BINARY_DIR@` together with cmake to (re)generate the jobs file with resolved paths (so one can call the `pycobench` anywhere).
```yaml
    new-binary:
       cmd: @PATH@/new-binary $1
```
  2. Now, use the `jobs.yaml` file, where you added your binary and run `./pycobench -c jobs/jobs.yaml -o some.output < inputs/single-automata.input`
      * Note, one can pass any shell command that returns list of automata, e.g. `< ls -1 ./automata/**/aut1.mata`
      * This generates `some.output` file; this format is supported by `pyco_proc` script that parses the output to `csv`/`html`/`text` format.
  3. Now, run `./pyco_proc --csv some.output > some.csv` to generate `.csv` output (change `csv` to `html` or `text` to generate different format).
  4. Alternatively, run `make test-performance` from the project root directory; this will run selected jobs registered in 
    `jobs/corr-single-param-jobs.yaml` (for binaries accepting single argument) and 
    `jobs/corr-double-param-jobs.yaml` (for binaries accepting two arguments).
  5. This will generate `csv` report of measuring binaries registered in `jobs/*.yaml` files on automata listed in the `input/*.input` files.
 
### How to benchmark and reproduce some of our results

We assume this is executed from `tests-integration` directory.

We recommend to use our `./scripts/run_pyco.sh` scripts which is called as follows:
```shell
./scripts/run_pyco.sh -c <jobs/JOBS.yaml> -m <method1;method2;...;methodn> -t <TIMEOUT> -j <JOBS> -s <SUFFIX_TAG> <inputs/INPUT.input>
```

You can specify following parameters
  1. `-c <jobs/JOBS.yaml>`: picks `YAML` file, where commands for measured binaries are specified.
  2. (optional) `-m <method1;method2;...;methodn>`: list of commands specified in the `YAML` file, which will only be measured; by default everything is measured.
  3. (optional) `-t <TIMEOUT>`: timeout in seconds for each run binary; (default = 60s).
  4. (optional) `-j <JOBS>`: number of parallel jobs that are run; (default = 6 jobs).
  5. (optional) ` -s <SUFFIX_TAG>`: adds suffix to the generated `.csv` file, for better organization; (default = empty).
  6. `<inputs/INPUT.input>`: any number of input file, where list of automata that are fed to binaries are listed.

Note that if you are missing any dependencies (for Python), the script will automatically ask you if
you wish to install them and should work with minimal user input.

In order to reproduce some of our results or run more thorough benchmarking, we recommend running the following 
test cases:

```shell
./scripts/run_pyco.sh -c jobs/bench-cade-23.yaml -m "b-param-diff;b-param-inter" inputs/bench-double-bool-comb-cox.input
./scripts/run_pyco.sh -c jobs/bench-cade-23.yaml -m "b-regex" inputs/bench-quintuple-email-filter_values.input
./scripts/run_pyco.sh -c jobs/bench-cade-23.yaml -m "b-armc-incl" inputs/bench-double-automata-inclusion.input
./scripts/run_pyco.sh -c jobs/bench-cade-23.yaml -m "param-intersect" inputs/bench-variadic-bool-comb-intersect.input
```

The results are saved in `results` directory in `csv` format. Moreover, short summary is printed at the end of the 
benchmarking process.

If you wish, to benchmark something different run the following:

```shell
./scripts/run_pyco.sh -c jobs/<JOBS_FILE>.yaml -m "<COMMAND_DELIMITED_BY_;>" inputs/<INPUT_FILE>.input
```