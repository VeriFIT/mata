# Mata: The Automata Library
[![master: Ubuntu (build-&-test)](https://github.com/VeriFIT/toris/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/VeriFIT/toris/actions/workflows/ubuntu.yml)
[![master: MacOS (build-&-test)](https://github.com/VeriFIT/toris/actions/workflows/macos.yml/badge.svg)](https://github.com/VeriFIT/toris/actions/workflows/macos.yml)

[![devel: Ubuntu (build-&-test)](https://github.com/VeriFIT/toris/actions/workflows/ubuntu.yml/badge.svg?branch=devel)](https://github.com/VeriFIT/toris/actions/workflows/ubuntu.yml)
[![devel: MacOS (build-&-test)](https://github.com/VeriFIT/toris/actions/workflows/macos.yml/badge.svg?branch=devel)](https://github.com/VeriFIT/toris/actions/workflows/macos.yml)

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

TODO

## Using the library

TODO

## Using the binding

TODO

# Contributing



