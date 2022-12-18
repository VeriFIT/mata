# Mata: The Automata Library

[![GitHub tag](https://img.shields.io/github/tag/VeriFIT/mata.svg)](https://github.com/VeriFIT/mata)

[![master: Ubuntu (build-&-test)](https://github.com/VeriFIT/mata/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/VeriFIT/mata/actions/workflows/ubuntu.yml)
[![master: MacOS (build-&-test)](https://github.com/VeriFIT/mata/actions/workflows/macos.yml/badge.svg)](https://github.com/VeriFIT/mata/actions/workflows/macos.yml)

[![devel: Ubuntu (build-&-test)](https://github.com/VeriFIT/mata/actions/workflows/ubuntu.yml/badge.svg?branch=devel)](https://github.com/VeriFIT/mata/actions/workflows/ubuntu.yml)
[![devel: MacOS (build-&-test)](https://github.com/VeriFIT/mata/actions/workflows/macos.yml/badge.svg?branch=devel)](https://github.com/VeriFIT/mata/actions/workflows/macos.yml)

Mata is an open source automata library that offers interface for different kinds of automata (NFA,
AFA, etc.). Currently, Mata offers two interfaces:

  1. An efficient library implemented in C/C++
  2. A flexible wrapper implemented in Python that uses the efficient library

# Building from sources

To build the the library run the following:

```
git clone https://github.com/VeriFIT/mata
cd mata
make release
```

In order, to verify the functionality of the library, you can run the test suite:

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

# Building the Python binding from sources

Mata offers binding of its efficient library to Python. You can install the binding as an Python
package on your system as follows. First, install the necessary requirements for Python and your
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

  1. C/C++ example programs. To run the program run the following:

```
make -C examples
./examples/example01-simple
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
#include <mata/nfa.hh>
```

We recommend to use the `Mata::Nfa` namespace for easier usage:

```cpp
using namespace Mata::Nfa;
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
    aut.add_trans(0, 0, 2);
    aut.add_trans(1, 1, 3);
```

You can verify the state of your automaton by generating the automaton in `.dot` format.

```cpp
    aut.print_to_DOT(std::cout);

    return 0;
}
```

Finally, compile the code using the following Makefile:

```makefile
CFLAGS=-std=c++14 -pedantic-errors -Wextra -Wall -Wfloat-equal -Wctor-dtor-privacy -Weffc++ -Woverloaded-virtual -fdiagnostics-show-option -g

INCLUDE=-I../include -I../3rdparty/simlib/include -I../3rdparty/re2/include
LIBS_ADD=-L../build/src -L../build/3rdparty/re2 -L../build/3rdparty/simlib
LIBS=-lmata -lsimlib -lre2

.PHONY: all clean

all: $(patsubst %.cc,%,$(wildcard *.cc)) ../build/src/libmata.a

example: example.cc
	g++ $(CFLAGS) $(INCLUDE) $(LIBS_ADD) $< $(LIBS) -o $@
```

## Using the binding

The python binding is installed (by default) to your local python package repository. You can
either use the binding in your own scripts or in the python interpreter.

You can start using the binding by importing the `mata` package.

```python
import mata
```

In your own scripts, we recommend to use the standard guard for running the scripts, as follows.

```python
if __name__ == "__main__":
```

The usage of the binding copies (to certain levels) the usage of the C++ library.

```python
    aut = mata.Nfa(4)

    aut.initial = {0, 1}
    aut.final = {2, 3}
    aut.add_trans_raw(0, 0, 2)
    aut.add_trans_raw(1, 1, 3)

    print(aut.to_dot_str())
```

You can either run your scripts directly using `python` or compile it using the `cython` project.

# Contributing

If you'd like to contribute to the libmata, 
please [fork the repository](https://github.com/VeriFIT/mata/fork), create a new 
feature branch, and finally [create a new pull request](https://github.com/VeriFIT/mata/compare).

In case you run into some unexpected behaviour, error or anything suspicions
either contact us directly through mail or 
[create a new issue](https://github.com/VeriFIT/mata/issues/new/choose).
When creating a new issue, please, try to include everything necessary for us to know
(such as the version, operation system, etc.) so we can sucessfully replicate the issue.

## Note to main contributors

By default, each merge automatically increases the `minor` version of the library
(i.e., `0.0.0 -> 0.1.0` ). This can be overruled using either tag `#patch` (increasing 
patch version, i.e., `0.0.0 -> 0.0.1`) or `#major` (increasing major version, i.e., 
`0.0.0 -> 1.0.0`). This tag is specified in the merge message. 

Generally, it is recommended to use `#major` for changes that introduces backward-incompatible
changes for people that used previous versions, and `#patch` for minor changes, such as bug-fixes,
performance fixes or refactoring.

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

The code of this project is licensed under GNU GPLv3 license.

# Contacts

  - **Lukáš Holík** ([kilohsakul](https://github.com/kilohsakul)): the supreme leader, the emperor of theory;
  - **Ondřej Lengál** ([ondrik](https://github.com/ondrik)): prototype developer and the world's talest hobbit;
  - Martin Hruška ([martinhruska](https://github.com/martinhruska)): library maintainer;
  - Tomáš Fiedor ([tfiedor](https://github.com/tfiedor)): python binding maintainer;
  - David Chocholatý ([Adda0](https://github.com/Adda0)) library and binding developer;
  - Juraj Síč ([jurajsic](https://github.com/jurajsic)): library developer;
  - Tomáš Vojnar ([vojnar](https://github.com/vojnar)): the spiritual leader;

# Acknowledgements

We thank for the support received from the Brno University of Technology 
([BUT FIT](https://www.fit.vutbr.cz/)).

Development of this tool has been supported by the following projects: ???.

This tool as well as the information provided on this web page reflects
only the author's view and no organization is responsible for any use
that may be made of the information it contains.

