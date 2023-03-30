### Profiling and Performance Testing of `libmata`

#### Structure

1. `automata/`: short database of approximately 70 automata of various complexity. The automata come from different
  benchmarks. `easiest` benchmarks should take less then second, while, `hardest` can take over 60 seconds.

2. `bin/`: target folder for compiling binaries used for profiling or performance testing.
3. `src/`: source codes that will be compiled to `bin` directory. The directory contains several templates:
  * `template.cc` contains basic template for arbitrary code to be profiled or performance tested.
  * `template-with-cli.args.cc` contains basic template that takes file as an command line argument, parses the automaton
     and computes minterms.
  * `template-with-list-of-automata.cc` contains basic template that has a hardcoded list of input automata taken from `automata` directory.
4. `pycobench`: a performance testing sscript
5. `pycoproc`: a parser of performance results

#### How to profile

1. Copy one of the `template*.cc` files with boilerplate code.
2. Find comment `HERE COMES YOUR CODE THAT YOU WANT TO PROFILE` and fill your code.
3. Run `make` from `src` or compile your binary.
4. Run `callgrind` or custom profile on the binary.

#### How to test performance

1. Copy one of the `template*.cc` files with boilerplate code.
2. Find comment `HERE COMES YOUR CODE THAT YOU WANT TO PROFILE` and fill your code.
3. Run `make` from `src` or compile your binary.
4. Add the new binary to the `jobs.yaml` as follows:
 
```
    new-binary:
       cmd: ./bin/new-binary $1
```

5. Run `./pycobench -c jobs.yaml -o some.output`
6. Run `./pyco_proc -o some.output`
7. Alternatively run `make perftest`
