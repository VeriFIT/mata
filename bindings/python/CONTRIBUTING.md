# Basic info

This guide is for new contributors looking to extend the Python binding for our libmata library. It
covers the binding’s structure, basic steps for extending or fixing it, more advanced operations
like working with shared pointers or C/C++ features, and troubleshooting tips for common issues that
may arise during development.

<!-- TOC -->
* [Basic info](#basic-info)
  * [Getting Started](#getting-started)
  * [File/Directory Structure](#filedirectory-structure)
  * [Adding a new function](#adding-a-new-function)
  * [Adding a new module](#adding-a-new-module)
  * [Adding a new class](#adding-a-new-class)
  * [Adding or modifying the function's signature](#adding-or-modifying-the-functions-signature)
* [Advanced modifications of functions](#advanced-modifications-of-functions)
  * [Declaring function with clashing name](#declaring-function-with-clashing-name)
  * [Declaring function returning complex type](#declaring-function-returning-complex-type)
* [Advanced modifications of classes](#advanced-modifications-of-classes)
  * [Declaring class methods](#declaring-class-methods)
  * [Declaring class attributes, getters and  setters](#declaring-class-attributes-getters-and--setters)
  * [Declaring class with clashing name](#declaring-class-with-clashing-name)
  * [Declaring class usable in other modules](#declaring-class-usable-in-other-modules)
  * [Declaring functions for string representation of objects](#declaring-functions-for-string-representation-of-objects)
  * [Declaring functions for comparison of objects](#declaring-functions-for-comparison-of-objects)
* [Other advanced features](#other-advanced-features)
  * [Dereferencing and retyping the object](#dereferencing-and-retyping-the-object)
    * [Dereferencing shared pointer](#dereferencing-shared-pointer)
    * [Dereferencing iterator](#dereferencing-iterator)
  * [Using null pointer](#using-null-pointer)
  * [Defining C variables in code](#defining-c-variables-in-code)
  * [Working with standard streams](#working-with-standard-streams)
  * [Working with iterators](#working-with-iterators)
  * [Defining type definition](#defining-type-definition)
  * [Calling functions of shared pointer object](#calling-functions-of-shared-pointer-object)
  * [Supporting Python for loops](#supporting-python-for-loops)
  * [Working with strings](#working-with-strings)
  * [Helping Cython to infer types](#helping-cython-to-infer-types)
  * [Using pre- and post-increments (`var++` and `++var`)](#using-pre--and-post-increments-var-and-var)
  * [Working with exceptions](#working-with-exceptions)
* [Importing between modules](#importing-between-modules)
  * [Importing C/C++ defined functions and symbols](#importing-cc-defined-functions-and-symbols)
  * [Importing Python defined functions and symbols](#importing-python-defined-functions-and-symbols)
  * [Importing standard containers and types](#importing-standard-containers-and-types)
* [Troubleshooting](#troubleshooting)
  * [An error when importing Python object from other module](#an-error-when-importing-python-object-from-other-module)
  * [A weird error when Cython cannot retype C/C++ object to Python object](#a-weird-error-when-cython-cannot-retype-cc-object-to-python-object)
  * [An error when Cython cannot find cimported function, even if it is defined](#an-error-when-cython-cannot-find-cimported-function-even-if-it-is-defined)
* [Quick glossary](#quick-glossary)
<!-- TOC -->

## Getting started

The following shows minimal example of relation between C++ library source files and Cython binding
files. It shows link between (1) Cython binding files (`nfa.pxd`, `nfa.pyx`) and C++ library files 
(`nfa-plumbing.h`), (2) Cython binding class (`Nfa`) and C++ library class (`mata::nfa::Nfa`; 
in binding refered as `CNfa`), and (3) Cython binding function (`union` that takes Python 
classes `Nfa`) and C++ library function (`uni` that takes C++ classes `mata::nfa::Nfa`, 
in binding refered as `c_uni` and `CNfa` respectively)

```c++
// @file: nfa-plumbing.h

// Minimal specification of C++ function, we want to wrap in Binding
namespace mata::nfa::plumbing {
    inline void uni(Nfa *unionAutomaton, const Nfa &lhs, const Nfa &rhs) { *unionAutomaton = uni(lhs, rhs); }
//              ^-- C++ function signature with C++ types
}
```

```cython
# @file: nfa.pxd

# Minimal specification of Cython header file.
# This specifies, what from C++ will be visible in Cython

cdef extern from "mata/nfa/nfa.hh" namespace "mata::nfa":
#                 ^-- source C++ file and namespace, where mata::nfa::Nfa class is defined
    cdef cppclass CNfa "mata::nfa::Nfa":
#                 ^-- C++ class made visible in binding; it will be refered by its alias `CNfa`
#                      ^-- full C++ class name
        CNfa()
#       ^-- constructor of C++ class using alias `CNfa`
#           (everything listed here will be visible in binding)
    
cdef extern from "mata/nfa/plumbing.hh" namespace "mata::nfa::plumbing":
#                 ^-- source C++ header and namespace, where the function is defined
    cdef void c_uni "mata::nfa::plumbing::uni" (CNfa*, CNfa&, CNfa&)
#             ^-- C++ function alias used in Binding   ^-- C++ type alias defined above
#                   ^-- full C++ function name
```

```cython
# @file: nfa.pyx

# Minimal example of implementation of Python binding. 
# These classes and function will be visible and callable in Python.

cdef class Nfa:
#          ^- Python class; wrapper for mata::nfa::Nfa
    cdef shared_ptr[CNfa] thisptr
#                         ^-- member that holds wrapped C++ object
    def __cinit__(self, state_number = 0, alph.Alphabet alphabet = None, label=None):
#       ^-- constructor of the Python class
        self.thisptr = make_shared[CNfa](
            mata_nfa.CNfa(state_number, empty_default_state_set, empty_default_state_set, c_alphabet)
        )
#       ^-- creating C++ object, that is wrapped in Python class Nfa

def union(Nfa lhs, Nfa rhs):
#   ^-- Python function; wrapper for mata::nfa::plumbing::uni()
    result = Nfa()
    mata_nfa.c_uni(
        result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get())
    )
#   ^-- call to C++ function `uni()` (it is named as c_uni to avoid collisions with Python)
    return result
#   ^-- function returns wrapper Python object Nfa, that holds instance of `mata::nfa::Nfa` C++ object
```

## File/Directory Structure

There are two types of files:

- `*.pxd`: Cython files containing declarations/definitions of the C/C++ API, i.e., symbols one 
  wants to specifically import and use in the binding (naturally, you do not need to import 
  everything).
- `*.pyx`: Cython files containing the implementation of the Python binding.

The directory structure is as follows:

- `build/`: Compiled binding in the form of shared libraries.
- `dist/`: Packed binding for PyPI, allowing installation via `pip install libmata`.
- `libmata/`: Root Python binding package.
    - `__init__.py`: An empty file necessary to define the package.
    - `nfa/`: Package for nondeterministic finite automata, corresponding to the `mata::nfa::` namespace.
        - `nfa.*`: Wrappers for `mata::nfa::Nfa` members and related structures or algorithms.
        - `strings.*`: Wrappers for `mata::strings` members and related structures or algorithms.
    - `alphabets.*`: Wrappers for `mata::Alphabet` members and related structures or algorithms.
    - `parser.*`: Wrappers for `mata::parser` members and related structures or algorithms.
    - `plotting.*`: Helper functions for displaying `libmata` members in Python code, interpreters, Jupyter notebooks, etc.
    - `utils.*`: Wrappers for `mata::utils` members and related structures or algorithms.
- `tests/`: Tests for the Cython wrapper.
    - `conftest.py`: Global configuration of tests, containing fixtures, setups, etc.
- `Manifest.in`: Specification of the contents of the wrapper distribution for PyPI.
- `pyproject.toml`: Specification for the wrapper distribution for PyPI.
- `requirements.txt`: Specification of the wrapper's requirements and dependencies. Run `make init` or `pip3 install -r requirements.txt` to install them.
- `setup.py`: Specification of the build process.

## Adding a new function

  1. Declare the function in `.pxd` file:
    
     - For a class function, declare it within the class definition:

```cython
cdef extern from "mata/nfa.hh" namespace "mata::nfa":
#                ^-- source file          ^-- namespace
    cdef cppclass CNfa "mata::nfa::Nfa":
    #             ^-- class name used in binding
    #                  ^-- full C/C++ name used in C/C++ library
        void make_initial(State)
        #    ^- signature of class function
```

   - For a non-class function, declare it under its corresponding environment:

```cython
cdef extern from "mata/nfa-plumbing.hh" namespace "mata::nfa::plumbing":
#                ^-- source file                  ^-- namespace
    cdef void get_elements(StateSet*, CBoolVector)
    #    ^-- function signature using previously defined 
    #         Cython types from C/C++ API as parameters
```

   - You need to specify: (1) the file where the function is declared (e.g., nfa-plumbing.hh), 
     (2) the namespace where the function is a member, and (3) the signature of the function. 
     The signature can be partial; the binding will only use the number of defined parameters. 
     The typing can be conservative, e.g., const references do not have to be kept.

  2. Implement the function in the `.pyx` file:

```cython
cimport libmata.nfa.nfa as mata_nfa
#       ^-- root package   ^-- typedef name
#              ^--- nfa package
#                  ^-- nfa.pyx
def get_elements_from_bool_vec(bool_vec: list[int]):
    #                                    ^-- typing (helps Cython)
    cdef CBoolVector c_bool_vec = CBoolVector(bool_vec)
    cdef StateSet c_states
    #    ^-- C/C++ type variables
    mata_nfa.get_elements(&c_states, c_bool_vec)
    #        ^-- call to wrapped functions
    return { state for state in c_states }
    #      ^-- conversion to Python set
```

## Adding a new module

  1. Create a `module.pxd` file and declare the functions from libmata that you want to use in the binding.
  2. Create a `module.pyx` file, where you will `cimport module` and write the implementation for functions calling the declared functions as `module.declared_function(...)`.
  3. Add the module to the list of extensions in `setup.py`:

```cython
extensions = [
    Extension(
        f"libmata.{pkg}",
        sources=[f"libmata{os.sep}{pkg.replace('.', os.sep)}.pyx"] + project_sources,
        include_dirs=project_includes,
        language="c++",
        extra_compile_args=["-std=c++20", "-DNO_THROW_DISPATCHER"],
    ) for pkg in (
        'nfa.nfa', 'alphabets', 'utils', 'parser', 'nfa.strings', 'plotting', 'module'
    )
]
```

## Adding a new class

  1. Declare the class in the `.pxd` file

```cython
cdef extern from "mata/alphabet.hh" namespace "mata":
#                ^-- source file              ^-- namespace
    cdef cppclass COnTheFlyAlphabet "mata::OnTheFlyAlphabet" (CAlphabet):
        
        #         ^-- name in bind  ^-- full name           ^-- inheritance (previously defined) 
        StringToSymbolMap symbol_map
        # ^-- previously defined type in binding        
        #                 ^--- property name
        COnTheFlyAlphabet(StringToSymbolMap) except +
        # ^-- constructor                    ^-- can fire exception
        StringToSymbolMap get_symbol_map()
        # ^-- class method
```

  2. In `pyx` file declare the Python object that will serve as a wrapper:

```cython
cdef class OnTheFlyAlphabet(Alphabet):
    pass
```

  3. In the wrapper object define the variable that holds pointer to the original C/C++ object.

```cython
cdef class OnTheFlyAlphabet(Alphabet):
    cdef COnTheFlyAlphabet *thisptr
    #    ^-- pointer to OnTheFlyAlphabet in the C/C++ library
```

  4. Create `__cinit__` and `__dealloc__` functions which are created when constructing new Python object.

```cython
cdef class OnTheFlyAlphabet(Alphabet):
    cdef COnTheFlyAlphabet *thisptr

    def __cinit__(self, State initial_symbol = 0):
    # ^-- constructor function
        self.thisptr = new COnTheFlyAlphabet(initial_symbol)
    #   ^-- populates the pointer through visible constructors and `new` keyword

    def __dealloc__(self):
    # ^-- destructor function
        del self.thisptr
```

  5. You can now define your class as `OnTheFlyAlphabet()` in Python code.

## Adding or modifying the function's signature

  1. Locate the function in the `.pxd` file:

```cython
cdef extern from "mata/re2parser.hh" namespace "mata::parser":
#                ^-- source file               ^-- source namespace
    cdef void create_nfa(CNfa*, string) except +
    #         ^-- function signature    ^-- can fire exceptions
```
   
  2. Now, you can add new parameter to the function:

```cython
cdef extern from "mata/re2parser.hh" namespace "mata::parser":
    #                ^-- source file               ^-- source namespace
    cdef void create_nfa(CNfa*, string, bool) except +
    #         ^-- function signature    ^-- new parameter
```

  3. Or you can define new signature:

```cython
cdef extern from "mata/re2parser.hh" namespace "mata::parser":
#                ^-- source file               ^-- source namespace
    cdef void create_nfa(CNfa*, string) except +
    cdef void create_nfa(CNfa*, string, bool) except +
    #         ^-- new visible signature
```

# Advanced modifications of functions

## Declaring function with clashing name

  * **Problem**: You want C/C++ function `func()` to be named the same in Python wrapping
  * **Solution**: You can define the name of the function any way you want, but you need to follow
    it with its full name in quotes (i.e. you can name the function in the `.pxd` as `c_my_func` 
    followed by full name `"Namespace::my_func"`; the function is then called as `c_my_func(..)`
    in Python). 

```cython
cdef NoodleSequence c_noodlify "mata::strings::seg_nfa::noodlify" (CNfa&, Symbol, bool)
#                   ^-- name visible in Cython
#                              ^-- original full C/C++ name
``` 

## Declaring function returning complex type

  * **Problem**: You want to wrap a C/C++ function `func()`, that returns some complex custom type.
  * **Solution**: Cython does not support `std::move` of any kind, hence function returning complex
    types, e.g. `mata::nfa::Nfa`. Hence, you have to implement additional helper function, that
    will return the type as a pointer, i.e., convert `MyClass func()` to `void func(MyClass*)`

```cython
cdef extern from "mata/nfa-plumbing.hh" namespace "mata::nfa::plumbing":
#                ^-- source file                  ^-- source namespace
    cdef void intersection(CNfa*, CNfa&, CNfa&, bool, umap[pair[State, State], State]*)
    #         ^-- definition of function with returns result through first CNfa* parameter
    # cdef CNfa intersection(CNfa&, CNfa&, bool, umap[pair[State, State], State]*)
    # ^--- this definition cannot be efficiently implemented in Cython
```

  * Then you can implement the actual function as follows:

```cython
def intersection(Nfa lhs, Nfa rhs, preserve_epsilon: bool = False):
#                ^-- Python-typed parameters         ^-- typing as help for compiler
    result = Nfa()
    #        ^-- creating returned Python object
    mata_nfa.intersection(
        result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), preserve_epsilon, NULL
    #   ^-- pointer to mata::nfa::Nfa object
    #                         ^-- equivalent of *lhs 
    )
    return result
```

  * Note, that this assumes that you have the actual object stored as pointer in the underlying
  Python class as `thisptr`.

# Advanced modifications of classes

## Declaring class methods

  * **Problem**: You want to declare C/C++ class method in the wrapper.
  * **Solution**: You simply declare the method under the class in `.pxd` file.

```cython
cdef cppclass COnTheFlyAlphabet "mata::OnTheFlyAlphabet" (CAlphabet):
#             ^-- visible class name
#                               ^-- full C/C++ name
#                                                         ^-- inheritance
    COrdVector[Symbol] get_alphabet_symbols()
    #                  ^-- class method declaration
```

  * You can then access the method from the C/C++ typed object in the `.pyx` file.
```cython
cdef umap[string, Symbol] c_symbol_map = self.thisptr.get_symbol_map()
#    ^-- unordered_map    ^-- C/C++ var  ^-- calling wrapper function through C/C++ object in thisptr
``` 

* **Problem**: You want to declare some method as class function in Python.
* **Solution**: You implement the method in `pyx` file with `@classmethod` decorator.

```cython
@classmethod
# ^-- classmethod decorator
def determinize_with_subset_map(cls, Nfa lhs):
    pass
```

* The function is then called as `module.Class.method`.
 
## Declaring class attributes, getters and  setters

* **Problem**: You want to declare an attribute to be visible in class.
* **Solution**: First, you declare the symbols in `.pxd` file. Attributes not declared here
  will simply not be visible.

```cython
cdef cppclass CMove "mata::nfa::SymbolPost":
    Symbol symbol
    StateSet targets
#   ^-- class attributes
```

* Implement the getters and setters in Cython class

```cython
cdef class SymbolPost:
# ^-- cython class
    cdef mata_nfa.CMove *thisptr
#   ^-- wrapped C/C++ object

    @property
    def symbol(self):
        return self.thisptr.symbol
    # ^-- getter property, accessed as object.symbol

    @symbol.setter
    def symbol(self, value):
        self.thisptr.symbol = value
    # ^-- setter property accessed as object.symbol = value
```

## Declaring class with clashing name

* **Problem**: You want to declare C/C++ class and have Python object with same name.
* **Solution**: You simply declare the C/C++ class with custom name followed by its full name.

```cython
cdef extern from "mata/nfa.hh" namespace "mata::nfa":
    #                ^-- source file          ^-- namespace
    cdef cppclass CNfa "mata::nfa::Nfa":
#             ^-- class name and its full C/C++ name
```

## Declaring class usable in other modules

* **Problem**: You want to declare Python class that is used in other modules in the binding.
* **Solution**: You need to forward declare the Python class in the `.pxd` file.

```cython
# @file: nfa.pxd
cdef class Transition:
# ^-- forward declaration of the Transition class
    cdef CTrans* thisptr
    cdef copy_from(self, CTrans trans)
    # ^-- all of properties (and preferably some cdef methods) must be declared as well
```

```cython
# @file: strings.pyx
cimport libmata.nfa.nfa as mata_nfa
result[epsilon_depth_pair.first].append(mata_nfa.Transition(trans.source, trans.symbol, trans.target))
#                                       ^-- now Transition can be used in other pyx files.
```

## Declaring functions for string representation of objects

* **Problem**: You want to print your classes in the Python interpreter or jupyter notebook.
* **Solution**: You have to define at least one of the two functions:
  1. `__str__`: returns string representation; usually returned by calling function `str()` explicitely.
  2. `__repr__` returns computer readable representation (usually so it is parsable by other code); this is called in jupyter notebooks when you simply write the name of the vairable.

```cython
cdef class SymbolPost:
    def __str__(self):
    # ^-- called in notebook/interpret by typing `str(myvar)`
        trans = "{" + ",".join(map(str, self.targets)) + "}"
        return f"[{self.symbol}]\u2192{trans}"

    def __repr__(self):
    # ^-- called in notebook by simply typing `myvar`
        return str(self)
```

## Declaring functions for comparison of objects

* **Problem**: You want to compare your wrapper objects based on C/C++ comparison functions.
* **Solution**: You declare the operator functions in `.pxd` files and then implement Python comparison function in `pyx` file.

* First, declare the corresponding C/C++ operators in `pxd` file.
```cython
# @file: nfa.pxd
cdef extern from "mata/nfa.hh" namespace "mata::nfa":
    cdef cppclass CMove "mata::nfa::SymbolPost":
        bool operator<(CMove)
        bool operator<=(CMove)
        bool operator>(CMove)
        bool operator>=(CMove)
```

* Second, implement the corresponding Python operators in `pyx` file.

```cython
# @file: nfa.pyx
cdef class SymbolPost:
    def __lt__(self, SymbolPost other):
        return dereference(self.thisptr) < dereference(other.thisptr)

    def __gt__(self, SymbolPost other):
        return dereference(self.thisptr) > dereference(other.thisptr)

    def __le__(self, SymbolPost other):
        return dereference(self.thisptr) <= dereference(other.thisptr)

    def __ge__(self, SymbolPost other):
        return dereference(self.thisptr) >= dereference(other.thisptr)
```

* Now, you can compare objects in Python.

# Other advanced features

## Dereferencing and retyping the object

* **Problem**: You have a pointer (e.g., returned from function) and need to dereference it and 
  retype as well, as Cython cannot infer the type.
* **Solution**: You `cimport` the `dereference` operator and then explicitly retype using `<CType>` notation.

```cython
from cython.operator import dereference
# ^-- import from cython.operator the implementation of dereference
<CAlphabet>dereference(alphabet.as_base())
#                       ^-- the function returns CAlphabet* type
#           ^-- dereference returns CAlphabet
# ^-- the topmost function helps Cython to infer, that it is CAlphabet
```

* Note: You might need to use parenthesis to restrict the scope of the the retype, i.e. `(<CType>...)`.

### Dereferencing shared pointer

* **Problem**: You want to dereference shared pointer in the `pyx` code.
* **Solution**: You simply call `.get()` function and then you can safely dereference the pointer.

```cython
from cython.operator import dereference
dereference(rhs.thisptr.get())
# ^-- cython implementation of dereference (operator*)
#           ^-- rhs object holds variable `shared_ptr[T] thisptr`
#                       ^-- get() returns T*
```

### Dereferencing iterator

* **Problem**: You want to access the value of the iterator in `pyx` code.
* **Solution**: You simply dereference the iterator.

```cython
from cython.operator import dereference
dereference(it).symbol
#           ^-- it holds some iterator object
```

## Using null pointer

* **Problem**: You want to use null pointer in your `pyx` code.
* **Solution**: You simply use `NULL` constant, that does not need to be imported and is supported
  by Cython. `nullptr` is trivially replaced with C-style `NULL`

## Defining C variables in code

* **Problem**: In `pyx` code you need to work with variables of C/C++ types.
* **Solution**: You define the variable together with its types using `cdef` keyword. 
  Note: the type needs to be specified so that Cython can infer it. You can also help
  Cython with Python variables and parameters by using the typing notations (i.e., `var : type`)

```cython
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.pair cimport pair

cdef AutSequence c_segments = self.thisptr.get_segments()
#    ^-- variable holding custom type
cdef umap[pair[State, State], State] c_product_map
#    ^-- C/C++ std::unordered_map
#         ^-- C/C++ std::pair
```

## Working with standard streams

* **Problem**: You need to work with standard streams (`ostream`, `fstream`, etc..
* **Solution**: You need to define the streams in the `pxd` header first. Then you can use
them in the `pyx` code.

```cython
cdef extern from "<iostream>" namespace "std":
    cdef cppclass ostream:
        ostream& write(const char*, int) except +

cdef extern from "<fstream>" namespace "std":
    cdef cppclass ofstream(ostream):
        ofstream(const char*) except +

cdef extern from "<sstream>" namespace "std":
    cdef cppclass stringstream(ostream):
        stringstream(string) except +
        string str()
```

* Then, you can call function that takes `ofstream` as follows:
 
```cython
cimport libmata.nfa.nfa as mata_nfa
cdef mata_nfa.ofstream* output
output = new mata_nfa.ofstream(output_file.encode('utf-8'))
#        ^-- creates new ofstream
#                              ^-- you need to encode the Python string to utf-8
try:
    self.thisptr.get().print_to_dot(dereference(output))
    #                               ^-- call the function with the ofstream
finally:
# ^-- this assures that the `output` will be cleaned
    del output
```

## Using pre- and post-increments (`var++` and `++var`)

* **Problem**: You want to manually use pre/post increments in your code.
* **Solution**: You have to import  `postincrement` and `preincrement` from `cython.operator`.

```cython
from cython.operator import postincrement as postinc, preincrement as preinc
postinc(it)
preinc(iterator)
```

## Working with iterators

* **Problem**: You want to iterate over some structure using iterators.
* **Solution**: You use the iterator functions, `begin()`, `end()`, and so on;
  however, you might need to make them visible. The standard iterators are visible,
  your custom classes need to define nested iterator class as shown below.

```cython
cdef cppclass CNfa "mata::nfa::Nfa":
    cppclass const_iterator:
    # ^-- definition of nested iterator; this can be accessed as CNfa.const_iterator type
        const_iterator()
        CTrans operator*()
        const_iterator& operator++()
        bool operator==(const_iterator&)
        bool operator!=(const_iterator&)
        void refresh_trans()
```

* Working with iterators in Cython, is then similar to working with iterators in C/C++.

```cython
from libcpp.vector cimport vector
from cython.operator import dereference, postincrement as postinc
cdef vector[mata_nfa.CMove].iterator it = transitions_list.begin()
cdef vector[mata_nfa.CMove].iterator end = transitions_list.end()
# ^-- define begin and end iterators
transsymbols = []
while it != end:
#     ^-- comparsion of two iterators
    t = SymbolPost(
        dereference(it).symbol,
        dereference(it).targets.to_vector()
        # ^-- to access the value of iterator you need to dereference
    )
    postinc(it)
    # ^-- ++it
    transsymbols.append(t)
return transsymbols
```

## Defining type definition

* **Problem**: You want to create some custom type or type alias.
* **Solution**: You simply define `ctypedef` in `pxd` file. This can then be cimported from module.

```cython
ctypedef umap[EpsilonDepth, TransitionSequence] EpsilonDepthTransitions
```

## Calling functions of shared pointer object

* **Problem**: You need to work with shared pointers.
* **Solution**: You cimport `shared_ptr` type, which can be used as any other type. For dereferencing
  or working with the object use `.get()` function which returns the pointer.

```cython
from libcpp.memory cimport shared_ptr
lhs.thisptr.get().add_state()
```

## Supporting Python for loops

* **Problem**: You want iterate over your structure in Python `for` loops.
* **Solution**: The standard function should support this by default (i.e., `std::` containers),
  your custom classes needs to support basic iterators (`begin()`, `end()`, etc.).


```cython
cdef vector[CTrans] c_transitions = self.thisptr.get().get_transitions_to(state_to)
trans = []
for c_transition in c_transitions:
#   ^-- classic for each loop over std::vector<Transition> does everything behind the scenes
    trans.append(Transition(c_transition.source, c_transition.symbol, c_transition.target))
```

## Working with strings

* **Problem**: You want to work with strings and pass them between binding and wrapped library.
* **Solution**: You need to use `encode()` when passing strings to C/C++ functions, and `decode()`,
  when recieving results.

```cython
k.encode('utf-8')
# ^- when passing strings to C/C++ library you need to encode
result.decode('utf-8')
# ^- when retrieving results from C/C++ you need to decode
```

## Helping Cython to infer types

* **Problem**: Cython fails to infer types for the variables and cannot compile the binding.
* **Solution**: You need to help Cython a little: add types to parameters, add types to cdefined 
  variables, or manually retype the variable.

```cython
(<mata_nfa.Nfa>noodle_segment).thisptr = c_noodle_segment
# ^-- tell Cython that this variable is of given type
objects: List[subprocess.Popen] = []
# ^-- tell Cython the type of the Python object
cdef mata_nfa.stringstream* output_stream
# ^-- define variable as C/C++ type
def union(cls, Nfa lhs, Nfa rhs):
    pass
#              ^-- tell Cython types of parameters
```

## Working with exceptions

* **Problem**: Your function can fire exceptions.
* **Solution**: You have to tell Cython how to handle these.

```cython
void add(CTrans) except -1
#                ^-- exceptions are returned as -1 in Cython
void add(CTrans) except +
#                ^-- function may fire exception; C/C++ exceptions will be translated to Python exceptions
void add(CTrans) except +MemoryError
#                ^-- function may fire MemoryError (corresponds to bad_alloc) 
void add(CTrans) noexcept +
#                ^-- function will not fire exception
```

* Note, there are other ways of handing exceptions, but these should suffice.

# Importing between modules

## Importing C/C++ defined functions and symbols

* **Problem**: You want to import symbols visible from wrapped `libmata` library. 
* **Solution**: You need to use `cimport`, which tells Cython to import C/C++ typed definitions.

```cython
cimport libmata.alphabets as alph
# ^-- import libmata.alphabets and its C/C++ functions and symbols, can be then acccesed as `alph.`.
from libmata.alphabets cimport CAlphabet
# ^-- imports C/C++ class CAlphabet
```

## Importing Python defined functions and symbols

* **Problem**: You want to import symbols from other non-C/C++ modules.
* **Solution**: You can try using `cimport`ed module; otherwise simply use `import`.

## Importing standard containers and types

* **Problem**: You want to use some standard C/C++ symbol, operation or container.
* **Solution**: You need to import from `cython.operator`, `libc`, or `libcpp`, which
  contains number of helper types, functions, etc.

```cython
from cython.operator import dereference, postincrement as postinc, preincrement as preinc
from libc.stdint cimport uintptr_t, uint8_t
from libcpp cimport bool
from libcpp.list cimport list as clist
from libcpp.memory cimport shared_ptr, make_shared
from libcpp.pair cimport pair
from libcpp.set cimport set as cset
from libcpp.string cimport string
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.unordered_set cimport unordered_set as uset
from libcpp.utility cimport pair
from libcpp.vector cimport vector
```

# Troubleshooting

## An error when importing Python object from other module

* **Problem**: You imported some other Cython module and want to use non-C/C++ class. The Cython,
  however, says that it does not know the requested member.
* **Solution**: You have to forward-declare the Python class in `.pxd` file.
 
* An example of such error is as below:

```shell
Error compiling Cython file:
------------------------------------------------------------
...
for epsilon_depth_pair in c_epsilon_transitions:
for trans in epsilon_depth_pair.second:
if epsilon_depth_pair.first not in result:
result[epsilon_depth_pair.first] = []

                result[epsilon_depth_pair.first].append(mata_nfa.Transition(trans.source, trans.symbol, trans.target))
                                                               ^
------------------------------------------------------------

libmata/nfa/strings.pyx:37:64: cimported module has no attribute 'Transition'
```

* The fix is to add the following forward declaration to `.pxd` file.

```cython
# @file: nfa.pxd
cdef class Transition:
    cdef CTrans* thisptr
    cdef copy_from(self, CTrans trans)
```

## A weird error when Cython cannot retype C/C++ object to Python object

* **Problem**: You want to store some C/C++ type to a C/C++ member of the Python object. Cython, 
  however, treats the object as Python object and cannot infer types.
* **Solution**: You have to explicitly retype the underlying Python object to help Cython with the rest.

* The following is an example of the error.

```shell
Error compiling Cython file:
------------------------------------------------------------
...
    )
    for c_noodle in c_noodle_segments:
        noodle = []
        for c_noodle_segment in c_noodle:
            noodle_segment = mata_nfa.Nfa()
            noodle_segment.thisptr = c_noodle_segment
                                    ^
------------------------------------------------------------

libmata/nfa/strings.pyx:135:37: Cannot convert 'shared_ptr[CNfa]' to Python object
```

* The fix is bellow: we help Cython by telling him that `noodle_segment` certainly is `mata_nfa.Nfa`
  class and now he can infer the type of its member `thisptr` and allow the assignment.

```cython
cimport libmata.nfa.nfa as mata_nfa
for c_noodle in c_noodle_segments:
    noodle = []
    for c_noodle_segment in c_noodle:
        noodle_segment = mata_nfa.Nfa()
        (<mata_nfa.Nfa>noodle_segment).thisptr = c_noodle_segment
        # ^-- here we help Cython to know this is Nfa() (despite it was defined at previous line >.<)
        noodle.append(noodle_segment)

    noodle_segments.append(noodle)
```

## An error when Cython cannot find cimported function, even if it is defined

* **Problem**: You defined some function `f` you wish to use in binding, you implement the function
  with same name in the binding, but Cython first says, that 
  you are redefining the function `f` and then says it cannot `cimport` it.
* **Solution**: You have to explicitly name the function `f` to, e.g.. `c_f`.

* The following is an example of the error.
 
```shell
warning: libmata/nfa/nfa.pyx:690:0: Overriding cdef method with def method.

Error compiling Cython file:
------------------------------------------------------------
...
:param Nfa lhs: non-deterministic finite automaton
:return: deterministic finite automaton, subset map
"""
result = Nfa()
cdef umap[StateSet, State] subset_map
mata_nfa.determinize(result.thisptr.get(), dereference(lhs.thisptr.get()), &subset_map)
       ^
------------------------------------------------------------

libmata/nfa/nfa.pyx:687:12: cimported module has no attribute 'determinize'
```

* An example of the solution is as follows:

```git
-    cdef void determinize(CNfa*, CNfa&, umap[StateSet, State]*)
+    cdef void c_determinize "mata::nfa::plumbing::determinize" (CNfa*, CNfa&, umap[StateSet, State]*)
```

# Quick glossary

  - `cdef` = function available in C code; 
  - `cpdef` = function available in C/C++ and Python, 
  - `def` = function available in Python code.
  - `cimport module` = import C/C++ visible functions; use `module.` prefix to access.
  - `from module cimport func, Class, type` = import selected classes, types or functions; juse no prefix to access.
