### Profiling and Performance Testing of `libmata`

#### Structure

1. `automata/`: short database of approximately 70 automata of various complexity. The automata come from different
  benchmarks. `easiest` benchmarks should take less then second, while, `hardest` can take over 60 seconds.

2. `bin/`: target folder for compiling binaries used for profiling or performance testing.
3. `src/`: source codes that will be compiled to `bin` directory. The directory contains several templates:
  * `template.cc` contains basic template for arbitrary code to be profiled or performance tested.
  * `template-with-cli.args.cc` contains basic template that takes file as an command line argument, parses the automaton
     and computes minterms.
  * `template-with-list-of-automata.cc` contains basic template that without arguments takes a hardcoded list of input 
     automata taken from `automata` directory; in case it is run with arguments, it reads the input files that contains
     paths to source automata. 
4. `pycobench`: a performance testing script
5. `pyco_proc`: a parser of performance results
6. `scripts/`: several helper scripts for profiling
  * `scripts/run_callgrind.sh` to print top functions (exclusive time, greater than 1%)
  * `scripts/run_massif.sh` to print memory peak in [B]

#### How to profile

1. Copy one of the `template*.cc` files with boilerplate code.
2. Find comment `HERE COMES YOUR CODE THAT YOU WANT TO PROFILE` and fill your code.
3. Run `make` from `src` or compile your binary.
4. Run `callgrind` or custom profile on the binary.
5. Note, you can use one of the scripts (`scripts/run_callgrind.sh` or `scripts/run_massif.sh`)

#### How to test performance

1. Copy one of the `template*.cc` files with boilerplate code.
2. Find comment `HERE COMES YOUR CODE THAT YOU WANT TO PROFILE` and fill your code.
3. Run `make` from `src` or compile your binary.
4. Add the new binary to the `jobs.yaml` as follows:
5. Note, you can use one of the scripts (`scripts/run_callgrind.sh` or `scripts/run_massif.sh`)
 
```
    new-binary:
       cmd: ./bin/new-binary $1
```

5. Run `./pycobench -c jobs.yaml -o some.output < single-automata.input`
  * Alternatively, one can pass any shell command that returns list of automata,
  e.g. `< ls -1 ./automata/**/aut1.mata`
6. Run `./pyco_proc --csv some.output > some.csv`
7. Alternatively run `make perftest`
