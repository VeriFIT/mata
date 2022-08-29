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

    ```
    #include <mata/nfa.hh>

    using namespace Mata::Nfa;

    int main() {
        Nfa aut(4);

        aut.initialstates = {0, 1};
        aut.finalstates = {2, 3};
        aut.add_trans(0, 0, 2);
        aut.add_trans(1, 1, 3);

        aut.print_to_DOT(std::cout);
    }
    ```

## Using the binding

    ```
    import mata

    if __name__ == "__main__":
    aut = mata.Nfa(4)

        aut.initial_states = {0, 1}
        aut.final_states = {2, 3}
        aut.add_trans_raw(0, 0, 2)
        aut.add_trans_raw(1, 1, 3)

        print(aut.to_dot_str())
    ```

# Contributing

If you'd like to contribute to the libmata, 
please [fork the repository](https://github.com/VeriFIT/mata/fork), create a new 
feature branch, and finally [create a new pull request](https://github.com/VeriFIT/mata/compare).

In case you run into some unexpected behaviour, error or anything suspicions
either contact us directly through mail or 
[create a new issue](https://github.com/VeriFIT/mata/issues/new/choose).
When creating a new issue, please, try to include everything necessary for us to know
(such as the version, operation system, etc.) so we can sucessfully replicate the issue.

# Links

    - Project (origin) repository: <https://github.com/verifit/mata>
    - Issue tracker: <https://github.com/verifit/mata/issues>
      - In case of some sensitive bugs (like security vulnerabilities),
        please contact us directly, instead of using issue tracker.
        We value your effort to improve the security and privacy of this project!
    - Project documentation: <https://verifit.github.io/mata>

Also, check out our research group focusing on program analysis, static and dynamic analysis,
formal methods, verification and many more: 
<http://www.fit.vutbr.cz/research/groups/verifit/index.php.en>.

# Licensing

The code of this project is licensed under GNU GPLv3 license.

# Contacts

  - **Lukáš Holík** ([kilohsakul](https://github.com/kilohsakul)): the supreme leader, the emperor of theory;
  - **Ondřej Lengál** ([ondrik](https://github.com/ondrik)): prototype developer and world's talest hobbit;
  - Martin Hruška ([martinhruska](https://github.com/martinhruska)): library maintainer;
  - Tomáš Fiedor ([tfiedor](https://github.com/tfiedor)): python binding maintainer;
  - David Chocholatý ([Adda0](https://github.com/Adda0)) library and binding developer;
  - Juraj Síč ([jurajsic](https://github.com/jurajsic)): library developer;
  - Tomáš Vojnar ([vojnar](https://github.com/vojnar)): spiritual leader;

# Acknowledgements

We thank for the support received from the Brno University of Technology 
([BUT FIT](https://www.fit.vutbr.cz/)).

Development of this tool has been supported by the following projects:.

This tool as well as the information provided on this web page reflects
only the author's view and no organization is responsible for any use
that may be made of the information it contains.

