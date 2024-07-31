# Mata: The Automata Library

[![GitHub tag](https://img.shields.io/github/tag/VeriFIT/mata.svg)](https://github.com/VeriFIT/mata)
[![Quality](https://github.com/VeriFIT/mata/actions/workflows/code-quality.yml/badge.svg)](https://github.com/VeriFIT/mata/actions/workflows/code-quality.yml)
[![Python-Binding (build-&-test)](https://github.com/VeriFIT/mata/actions/workflows/python-binding.yml/badge.svg?branch=devel)](https://github.com/VeriFIT/mata/actions/workflows/python-binding.yml)
[![codecov](https://codecov.io/gh/VeriFIT/mata/branch/devel/graph/badge.svg?token=9VAVD19N4D)](https://codecov.io/gh/VeriFIT/mata)

Mata is an open source automata library that offers interface for different kinds of automata (NFA, etc.). Currently, Mata offers two interfaces:

  1. An efficient library implemented in C/C++
  2. A flexible wrapper implemented in Python that uses the efficient library

# Requirements and dependencies

For a successful installation of Mata, `cmake` of version `3.15.0` (or higher) and a C++ compiler with a support of C++-20 standard is required. 
From optional requirements, `doxygen` is required for a generation of the documentation and `catch2` is required for the unit testing (Mata can still be compiled without these optional dependencies). 

The Mata library further depends on the following libraries, included in the `3rdparty` directory:
- `cudd` for BDD manipulation,
- `re2` for regular expression parsing and the corresponding automata construction, and
- `simlib` for a simulation computation.

# Building and installing from sources

To build the library, run the following:

```
git clone https://github.com/VeriFIT/mata
cd mata
make release
```

In order to install the library, you can run 

```
sudo make install
```

In order to verify the functionality of the library, you can run the test suite:

```
make test
```

You might, need to install the dependencies to measure the coverage of the tests. 
Run the following to install the dependencies for MacOS:

```
brew install lcov gcovr
```

Run the following to install the dependencies for Ubuntu:

```
sudo apt-get install -y build-essential lcov gcovr xdg-utils
```

# Python binding

Mata offers binding of its efficient library to Python. You can install the binding as an Python
package on your system as follows. 

### Install from PyPI

To install a latest version from the PyPI repository, run 
```
pip3 install libmata
```

### Building from sources

To build from sources first, install the necessary requirements for Python and your
system. We recommend using the virtual environemnt (`venv`) to install and use the library.

```
python -m pip install --upgrade pip
make -C bindings/python init

sudo apt-get -qq update 
sudo apt-get -qq install -y graphviz graphviz-dev
```

Now, you can install the library.

```
make -C bindings/python install
```

Finally, you can verify the binding woks as expected by running the test suite:

```
make -C bindings/python test
```

# Getting started

To get started, we refer to the [examples](examples/) in our repository.
This directory contains examples of various usage in form of:

  1. C/C++ example programs. By default, they are built with the library. To run for example the first example:

```
./build/examples/example01-simple
```

  3. Python example scripts. To run the scripts run the following.

```
python examples/example01-python-binding.py
```

  4. Python jupyter notebooks. To run the jupyter notebook, one needs to have jupyter installed as
  a prerequisite. The run the jupyter notebook, that creates an instance on your local server.
  Navigate to generated link to see the available jupyter notebooks:
   
```
pip3 install jupyter
jupyter notebook
```

## Using the library

The library can be used directly in the C/C++ code. The result of compilation is a static
or dynamic library, that can be linked to ones project. Note, that the library is dependent
on several other 3rd party libraries (e.g., `libre2` or `libsimlib`), which are included in
the repository.

First import the library in your code. If the library is properly installed, you can use
the standard include.

```cpp
#include <mata/nfa/nfa.hh>
```

We recommend to use the `mata::nfa` namespace for easier usage:

```cpp
using namespace mata::nfa;
```

Start by creating an automaton with fixed number of states.

```cpp
int main() {
    Nfa aut(4);
```

You can set the initial and final states directly using the initializers.

```cpp
    aut.initial = {0, 1};
    aut.final = {2, 3};
```

Further, you can add transitions in form of tripple `(state_from, symbol, targets)`:

```cpp
    aut.delta.add(0, 0, 2);
    aut.delta.add(1, 1, 3);
```

You can verify the state of your automaton by generating the automaton in `.dot` format.

```cpp
    aut.print_to_dot(std::cout);

    return 0;
}
```

We recommend `cmake` for building projects using Mata. Provided the Mata is installed in the system directories,
the `CMakeLists.txt` file for the example may look like:

```cmake
cmake_minimum_required (VERSION 3.15.0)
project (mata-example)
set (CMAKE_CXX_STANDARD 20)

find_library(LIBMATA mata REQUIRED)

add_executable(mata-example
    mata-example.cc)
target_link_libraries(mata-example PUBLIC ${LIBMATA})
```

## Using the Python binding

The python binding is installed (by default) to your local python package repository. You can
either use the binding in your own scripts or in the python interpreter.

You can start using the binding by importing the `libmata` package.

```python
import libmata.nfa.nfa as mata_nfa
```

In your own scripts, we recommend to use the standard guard for running the scripts, as follows.

```python
if __name__ == "__main__":
```

The usage of the binding copies (to certain levels) the usage of the C++ library.

```python
    aut = mata_nfa.Nfa(4)

    aut.initial_states = {0, 1}
    aut.final_states = {2, 3}
    aut.add_transition(0, 0, 2)
    aut.add_transition(1, 1, 3)

    print(aut.to_dot_str())
```

You can either run your scripts directly using `python` or compile it using the `cython` project.

# Publications
- Chocholatý, D., Fiedor, T., Havlena, V., Holík, L., Hruška, M., Lengál, O., & Síč, J. (2023). [Mata, a Fast and Simple Finite Automata Library](https://doi.org/10.1007/978-3-031-57249-4_7). In *Proc. of TACAS'24*, volume 14571 of LNCS, pages 130-151, 2024. Springer.
    - Chocholatý, D., Fiedor, T., Havlena, V., Holík, L., Hruška, M., Lengál, O., Síč, J.: [A replication package for reproducing the results of paper “Mata: A fast and simple finite automata library”](https://doi.org/10.5281/zenodo.10044515) (Oct 2023).

# Contributing

Please refer to our [contributing guidelines](CONTRIBUTING.md).

# Links

  - Project (origin) repository: <https://github.com/verifit/mata>
  - Issue tracker: <https://github.com/verifit/mata/issues>
    - In case of some sensitive bugs (like security vulnerabilities),
      please contact us directly, instead of using issue tracker.
      We value your effort to improve the security and privacy of this project!
  - Project documentation: <https://verifit.github.io/mata>
  - Jupyter notebooks demonstrating `mata` usage: <https://github.com/VeriFIT/mata/tree/devel/examples/notebooks>
    - [Example 01: WS1S Logic](https://github.com/VeriFIT/mata/tree/devel/examples/notebooks/example-01-ws1s-formulae.ipynb)
    - [Example 02: ReDoS Attacks](https://github.com/VeriFIT/mata/tree/devel/examples/notebooks/example-02-redos-attacks.ipynb)
    - [Example 03: Exploring Maze](https://github.com/VeriFIT/mata/tree/devel/examples/notebooks/example-03-exploring-maze.ipynb)

Also, check out our research group focusing on program analysis, static and dynamic analysis,
formal methods, verification and many more: 
<http://www.fit.vutbr.cz/research/groups/verifit/index.php.en>.

# Licensing

The code of Mata is licensed under MIT licence. See [LICENSE](LICENSE).

The folder [`3rdparty/`](3rdparty) contains 3rd party applications licensed under their own licences, included with the code.

# Contacts

See [AUTHORS.md](AUTHORS.md)

# Acknowledgements

We thank for the support received from the Brno University of Technology 
([BUT FIT](https://www.fit.vutbr.cz/)).

Development of this tool has been supported by the following projects: ???.

This tool as well as the information provided on this web page reflects
only the author's view and no organization is responsible for any use
that may be made of the information it contains.
