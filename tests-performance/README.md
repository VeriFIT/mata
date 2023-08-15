### Profiling and Performance Testing of `libmata`

#### Structure

1. `automata/`: short database of approximately 70 automata of various complexity. The automata come from different
  benchmarks. `easiest` benchmarks should take less than second, while, `hardest` can take over 60 seconds.
2. `src/`: source codes that will be compiled to `build` directory. The directory contains several templates that
   one can use to build ones own benchmarks:
  * `template.cc` contains basic template for arbitrary code to be profiled or performance tested.
  * `template-with-cli.args.cc` contains basic template that takes file as a command line argument, parses the automaton
     and computes minterms.
  * `template-with-list-of-automata.cc` contains basic template that without arguments takes a hardcoded list of input 
     automata taken from `automata` directory; in case it is run with arguments, it reads the input files that contains
     paths to source automata.
3. `pycobench`: a performance testing script
4. `pyco_proc`: a parser of performance results
5. `scripts/`: several helper scripts for profiling
  * `scripts/run_callgrind.sh` to print top functions (exclusive time, greater than 1%)
  * `scripts/run_massif.sh` to print memory peak in [B]
6. `jobs` definitions of jobs for `pycobench`
7. `inputs` definitions of inputs for `pycobench`

#### How to profile

1. Make a copy one of the `template*.cc` files with boilerplate code to start (e.g. `cp src/template.cc src/myprofiling.cc`).
2. Find comment `HERE COMES YOUR CODE THAT YOU WANT TO PROFILE` and fill it your code.
3. Run `make release` from root directory or build using `cmake`.
4. Run `callgrind` or custom profile on the binary.
5. Note, you can use one of the scripts (`scripts/run_callgrind.sh` or `scripts/run_massif.sh`)

#### How to test performance

1. Make a copy one of the `template*.cc` files with boilerplate code to start (e.g. `cp src/template.cc src/myprofiling.cc`).
2. Find comment `HERE COMES YOUR CODE THAT YOU WANT TO PROFILE` and fill your code.
3. Run `make release` from root directory or build using `cmake`.
4. Add the new binary to one of the yaml files in `jobs/*.yaml` as follows:
```
    new-binary:
       cmd: ./bin/new-binary $1
```
5. Run `./pycobench -c jobs/jobs.yaml -o some.output < inputs/single-automata.input`
  * Alternatively, one can pass any shell command that returns list of automata,
  e.g. `< ls -1 ./automata/**/aut1.mata`
6. Run `./pyco_proc --csv some.output > some.csv`
7. Alternatively run `make test-performance` from project root directory.
