# (Auto)Mata Script Language

**Status: specification, partly implemented.** The page records the agreed design of **Mata Script**, a small,
library-independent language for describing automata operations, with Mata as its first implementation. The `mata`
command runs scripts today; [The `mata` command](cli) lists under its implementation notes which operations and
options of this page the current interpreter does not have, and it rejects those by name rather than approximating
them. Mata Script programs use the **`.matasc`** file extension, as in `email-filter.matasc`; `.mata` remains the
extension for automata files.

A script loads automata, applies a sequence of automata operations to them, and reports the outcome: a verdict, a
witness, a printed automaton, or timing records. It replaces two things that exist today:

- the `.emp` programs of [VeriFIT/nfa-program-parser](https://github.com/VeriFIT/nfa-program-parser/), which can only
  express a straight-line sequence drawn from a small fixed set of operations, and
- the hand-written benchmark binaries in `tests-integration/src/`, one C++ file per sequence of operations.

The intended uses are benchmark programs, one-off queries from a shell, regression specifications that assert expected
results, and interactive sessions, including the playground on the website.

## A first example

The script below is the `email-filter` benchmark: it intersects the first four input automata and checks, once with
each inclusion algorithm, whether the intersection is included in the fifth one.

```scheme
#!/usr/bin/env -S mata script run
(mata-script "0.1.0")
(set-info :name "email-filter")
(declare-inputs automata (List Nfa))

(define filters (list:reduce nfa:intersection (list:take automata 4)))
(define (included-with algorithm)
  (time (concat algorithm "-time")
    (nfa:included? filters (list:nth automata 4) :algorithm algorithm)))
(check "naive" (included-with "naive"))
(check "antichains" (included-with "antichains"))
```

Run on five `.mata` files, it prints a timing record and a verdict for each algorithm, for example:

```text
naive-time: 0.048100
naive: true
antichains-time: 0.001200
antichains: true
```

Every record of a run has its own label, so a program that reads the records, such as `pycobench`, can key them by
label.

## Design principles

1. **A script is a sequence of forms, evaluated in order.** The shape is borrowed from SMT-LIB: parenthesised
   commands, `:keyword` attributes, options, a `check` command. The language is only inspired by SMT-LIB. It is not
   conformant, nothing is solved, and every form runs when it is reached.
2. **One application is one library call.** `(nfa:determinize a)` calls `mata::nfa::determinize` and nothing else.
   Arguments are evaluated left to right. No call is added, removed, merged or reordered unless the
   [optimizer](#optimizer) is turned on. The exceptions are listed in
   [One call per application](#one-call-per-application).
3. **Values are immutable.** An operation never changes its operands. A name always denotes the value it was bound
   to.
4. **Operation names are the names of the library.** A name is the C++ name in kebab-case with abbreviations written
   out, so the API reference documents the script operations too.
5. **Scripts are checked before they run.** Everything that can be decided without loading an input is decided first,
   so a misspelled name is not reported after an hour of determinization. `mata script run` checks the whole script
   before its first form runs, unless it is given `--no-check`.
6. **Functions are ordinary values.** A function can be defined inside another function, passed as an argument,
   returned as a result and stored in a list; see [Functions](#functions).
7. **Loops and recursion are opt-in.** They are off unless the script turns them on, and an interpreter may leave them
   out; see [Loops and recursion](#loops-and-recursion).
8. **Evaluation is sequential.** Nothing is evaluated in parallel.
9. **A script names its language version.** A `mata-script` header declares the version a script is written for, so
   a later version can change the language without changing what an existing script means; see
   [Versions](#versions).

## Lexical structure

| Token           | Form                                                                   | Examples                     |
| --------------- | ---------------------------------------------------------------------- | ---------------------------- |
| Comment         | `;` to the end of the line; a `#!` first line is skipped too           | `; intersect the filters`    |
| Name            | Letters, digits and `- _ ? ! * + / < = > .`, not starting with a digit | `filters`, `list:take`       |
| Namespaced name | `namespace:name`                                                       | `nfa:intersection`           |
| Keyword         | `:name`                                                                | `:algorithm`                 |
| Input reference | `$` followed by a position (from 1) or a name                          | `$1`, `$specification`       |
| Integer         | Decimal digits                                                         | `0`, `42`                    |
| String          | Between `"`; the only escape is `""` for one `"`; may span lines       | `"a(b\|c)*"`, `"say ""hi"""` |
| Constant        | `true`, `false`, `none`                                                |                              |

Backslash has no meaning in a string, so regular expressions and `.mata` text can be written without doubling it. A
namespaced name always names an operation: a script cannot define one, so an operation added to a later version never
clashes with a name a script chose.

## Grammar

```ebnf
script      = [ shebang ] [ header ] { form } ;
header      = "(" "mata-script" string { ":requires" "(" { name } ")" | ":profile" name } ")" ;
form        = definition | declaration | directive | expression ;

definition  = "(" "define" name [ sort ] expression ")"
            | "(" "define" "(" name { parameter } ")" body ")" ;
body        = { definition } expression { expression } ;
parameter   = name | "(" name sort ")" ;
declaration = "(" "declare-input" name sort ")"
            | "(" "declare-inputs" name sort ")"
            | "(" "declare-param" name sort [ expression ] ")" ;
directive   = "(" "set-option" keyword expression ")" | "(" "set-info" keyword expression ")"
            | "(" "push" ")" | "(" "pop" ")" | "(" "reset" ")" | "(" "exit" ")" ;

expression  = constant | integer | string | name | input-reference
            | "(" expression { expression } { keyword expression } ")"
            | "(" "let" "(" { "(" name expression ")" } ")" body ")"
            | "(" "lambda" "(" { parameter } ")" body ")"
            | "(" "loop" "(" { "(" name expression ")" } ")" body ")"
            | "(" "recur" { expression } ")"
            | "(" "if" expression expression expression ")"
            | "(" "cond" { "(" expression expression ")" } "(" "else" expression ")" ")"
            | "(" "do" expression { expression } ")"
            | "(" ( "and" | "or" ) { expression } ")"
            | "(" "->" expression { step } ")"
            | "(" "as" expression sort ")"
            | "(" "time" expression expression { expression } ")"
            | "(" "witness" expression ")"
            | "(" ( "get-option" | "get-info" ) keyword ")"
            | "(" ( "nfa:literal" | "nft:literal" ) { keyword literal-data } ")" ;
step        = name | "(" expression { expression } { keyword expression } ")" ;
sort        = name | "(" name { sort } ")" ;
literal-data = name | integer | "(" { literal-data } ")" ;
```

The header, when present, is the first form. Declarations and directives are allowed only at the top level. A
definition is allowed at the top level and at the start of a body, where it is local to that body. Everything else is
an expression and can be nested; `get-option` and `get-info` are expressions, so `(print (get-option :alphabet))`
works. `loop` and `recur` need the option described in [Loops and recursion](#loops-and-recursion).

## Values and sorts

| Sort                    | Value                                                                     |
| ----------------------- | ------------------------------------------------------------------------- |
| `Nfa`                   | `mata::nfa::Nfa`                                                          |
| `Nft`                   | `mata::nft::Nft`                                                          |
| `Automaton`             | Either an `Nfa` or an `Nft`; which one is known only when the script runs |
| `IntermediateAutomaton` | `mata::IntermediateAut`: a parsed automaton that is not constructed yet   |
| `Alphabet`              | `mata::Alphabet`                                                          |
| `Symbol`                | `mata::Symbol`; `epsilon` and `dont-care` are predefined                  |
| `Word`                  | `mata::Word`                                                              |
| `Run`                   | `mata::nfa::Run`: a word and a path                                       |
| `Bool`, `Int`, `String` |                                                                           |
| `(List T)`              | A list of values of sort `T`                                              |
| `(Option T)`            | Either a `T` or `none`, for operations that return `std::optional`        |
| `Function`              | An operation, a defined function or a `lambda`                            |

### Checking

Checking is static where the sorts are known and dynamic where they are not.

- **Before the script runs:** the language version, syntax, unknown names, unknown keywords and hints, the number of
  arguments in a call of a named operation or function, a name that is reserved, and the declared inputs and
  parameters against the ones given.
- **Before the script runs, where sorts are known:** every operation in the `nfa:` and `nft:` namespaces has fixed
  sorts, so `(nfa:determinize t)` with `t` declared as `Nft` is rejected without running anything.
- **When the script runs:** the kind of a loaded file, operations in the `automaton:` namespace, function parameters
  without a sort, and every call of a function that arrived as a value.
- **As a report:** whether the script is guaranteed to terminate; see [Termination](#termination).

`(as value Sort)` narrows a value and fails at that point if the value is of another sort:

```scheme
(define a (as (automaton:load $1) Nfa)) ; error here if the file holds a transducer
```

## Definitions, functions and conditionals

```scheme
(define alphabet-size 4)                          ; a value
(define product Nfa (nfa:intersection lhs rhs))   ; a value with a checked sort

(define (image (transducer Nft) (language Nfa))   ; a function; parameter sorts are optional
  (nft:to-nfa (nft:apply transducer language)))

(define deterministic
  (if (nfa:deterministic? product) product (nfa:determinize product)))
```

- `define` binds a name once; binding it again in the same scope is an error. `push` opens a scope, `pop` closes it
  and drops its definitions and option changes, and `reset` returns to the initial state.
- The sort in `(define name Sort value)` narrows the value as `as` does: a value that cannot be of that sort is an
  error before the script runs, and a value whose sort is known only when the script runs is checked then.
- `let` binds names for the extent of its body, each binding seeing the previous ones.
- `if` and `cond` evaluate only the chosen branch. `and` and `or` stop at the first operand that decides the result.
  This matters because it decides which library calls are made.
- `do` evaluates its expressions in order and returns the last value.

### Functions

A function can call every function that is defined before it. A function is also an ordinary value: it can be defined
inside another function, passed as an argument, returned as a result, and stored in a list.

```scheme
(define (minimal-image (transducer Nft) (language Nfa))
  (define (normalize (automaton Nfa))             ; visible only inside minimal-image
    (-> automaton nfa:determinize nfa:minimize))
  (normalize (nft:to-nfa (nft:apply transducer language))))

(define (included-with (algorithm String))        ; returns a function
  (lambda (smaller bigger) (nfa:included? smaller bigger :algorithm algorithm)))

(define relations (list (included-with "naive") (included-with "antichains")))
(list:map (lambda (included?) (included? lhs rhs)) relations)
```

- **Body.** The body of a function, of a `lambda` and of a `let` is a sequence of definitions followed by
  expressions. The definitions are local to the body, and the value of the body is the value of its last expression.
- **Parameters** are positional. A parameter that takes a function can be given the sort `Function`.
- **Scope.** A function sees its parameters, its local definitions, and every name that was visible where the
  function was written. It keeps those names for as long as it exists: the function returned by `included-with` still
  knows `algorithm` after `included-with` has returned.
- **Order.** A name is visible after its definition, and not inside it. A function can therefore call the functions
  defined before it, but it cannot call itself by name, and two functions cannot call each other.
  [Loops and recursion](#loops-and-recursion) describes the option that lets a function call itself.
- **Own name.** Using the name of a function inside its own body is an error without that option, even when an outer
  scope defines the same name. Turning the option on therefore never changes what an accepted script means.
- **Hiding.** An inner scope may define a name that an outer scope already has. The inner one is used inside that
  scope.

### Pipelines

`->` passes a value through a sequence of steps. A step that is a name is called with the value; a step that is an
application gets the value as its first argument.

```scheme
(-> $1 nfa:load nfa:determinize nfa:minimize nfa:print-to-mata print)

(-> lhs (nfa:intersection rhs) nfa:trim (write "product.mata"))
```

The second pipeline is `(write (nfa:trim (nfa:intersection lhs rhs)) "product.mata")`.

### Lists

`(list a b c)` builds a list. The list functions are `list:length`, `list:nth` (from 0), `list:take`, `list:drop`,
`list:range`, `list:map`, `list:filter`, `list:reduce` and `list:fold`:

```scheme
(list:map nfa:determinize automata)
(list:filter (lambda (a) (not (nfa:language-empty? a))) automata)
(list:reduce nfa:intersection automata)                    ; error on an empty list
(list:fold nfa:union-nondeterministic empty-language automata) ; starts from empty-language
```

`list:reduce` and `list:fold` combine from left to right, one call per element. The function gets the value combined
so far as its first argument and the next element as its second.

`(list:range start end)` is the list of integers from `start` up to, but not including, `end`; it is empty when
`start` is not smaller than `end`. Repeating something a number of times that is known before the repetition starts
needs no loop. It is a `list:map` or a `list:fold` over a range:

```scheme
(list:range 0 3)                                  ; the list 0 1 2

; measure the same check five times: records "inclusion-0" to "inclusion-4"
(list:map (lambda (round) (time (concat "inclusion-" (to-string round)) (nfa:included? lhs rhs)))
          (list:range 0 5))

; the image of a language under four applications of a transducer
(list:fold (lambda (language round) (nft:to-nfa (nft:apply step language)))
           initial
           (list:range 0 4))
```

### Loops and recursion

Loops and functions that call themselves are part of the language, but they are off by default. A script turns them
on for the whole file:

```scheme
(set-option :loops-and-recursion true)
```

- The option must be set before the first form that is not `set-option` or `set-info`. It cannot be changed
  afterwards, and `pop` and `reset` leave it as it is.
- Without the option, a `loop`, a `recur`, and a function that uses its own name in its body are errors, reported
  before the script runs. `loop` and `recur` cannot be used as names in any script.
- An interpreter may leave the option unimplemented. It then rejects the `set-option` form before running anything.
  The first version of the Mata interpreter is planned without it.

Everything else on this page works without the option, including functions that call other functions and functions
used as values.

Loops and recursion are meant for a computation whose number of rounds depends on the automata computed so far, such
as a fixpoint. A number of rounds that is known in advance needs only [`list:range`](#lists).

`loop` binds names to initial values and evaluates its body. `(recur value …)` starts the body again with the names
bound to the new values. When the body finishes without a `recur`, the loop ends and the value of the body is the
value of the loop.

```scheme
(set-option :loops-and-recursion true)

(declare-input initial Nfa)
(declare-input step Nft)
(declare-input bad Nfa)

; the configurations reachable from `initial` under the transition relation `step`
(define reachable
  (loop ((reached initial))
    (define next
      (nfa:union-nondeterministic reached (nft:to-nfa (nft:apply step reached))))
    (if (nfa:included? next reached) reached (recur next))))

(check (nfa:intersection-empty? reachable bad))
```

- `recur` takes one value for each name of the loop, of the sort that the name started with.
- `recur` must be the last thing the body evaluates: the last expression of the body, or a branch of an `if` or a
  `cond` in that place, or the last expression of a `let` or a `do` in that place.
- `recur` restarts the innermost `loop` around it. It cannot be written inside a function that is defined in the body
  of the loop.

With the option, a function defined with `define` is also visible inside its own body, so it can call itself. The
loop above as a function:

```scheme
(define (saturate (reached Nfa))
  (define next
    (nfa:union-nondeterministic reached (nft:to-nfa (nft:apply step reached))))
  (if (nfa:included? next reached) reached (saturate next)))

(define reachable (saturate initial))
```

Two functions still cannot call each other by name, because the second one is not visible in the first.

(script-termination)=

### Termination

The checker reports, for each script, whether it is guaranteed to terminate, provided that the library operations it
calls do. This is a report, not an error: the script runs either way.

The guarantee is given when the script has no `loop` and no function that calls itself, and every function in it is
used in these ways only:

- it is bound to a name,
- it is called, or
- it is given to a `list:` function as the function to apply.

A script gets no guarantee when it passes a function to a defined function or to a `lambda`, returns a function from
one, or puts a function into a list. A function can then be passed to itself, as in
`((lambda (f) (f f)) (lambda (f) (f f)))`, which runs forever without any loop. The example in
[Functions](#functions) is such a script: `included-with` returns a function, and `relations` is a list of functions.

The checker is conservative, so a script without the guarantee may still terminate.

## Inputs

A script gets its automata from four sources, and its other values from parameters.

**Inputs given on the command line** are paths. `$1`, `$2`, … are the positional inputs, `inputs` is the list of all
of them, and `$name` is the input given under that name. All of them are strings; nothing is loaded until the script
asks:

```scheme
(define lhs (nfa:load $1))
(define transducer (nft:load $transducer))
(define unknown (automaton:load $2)) ; an Nfa or an Nft, whichever the file holds
```

**Declarations** are the short form:

```scheme
(declare-input lhs Nfa)
(declare-input rhs Nfa)
(declare-inputs rest (List Nfa))
```

- Each `declare-input` declares one automaton, `Nfa`, `Nft` or `Automaton`, and takes the input given under its name,
  or else the next positional input. A file with several automata gives one automaton to each `declare-input` in turn.
  `declare-inputs` takes all the positional inputs that are left, every automaton of every file, and must come last.
- Declarations must come before every other form except `set-option` and `set-info`, which run in order with them, so
  `(set-option :alphabet "bytes")` before a declaration applies to the automaton it loads.
- All declared inputs are loaded before the first other form runs. A file whose kind does not match the declared sort
  is an error at that point. Declaring an input as `Automaton` accepts either kind.
- When a script declares inputs, every input given on the command line must be taken by a declaration; an input left
  over is an error, so a mistyped command line is not ignored.
- The loading done by declarations is part of a [timing record](#timing) only under `:time-operations`, as `parsing`.

**Parameters** are values given on the command line, for a script that is run several times with different sizes,
seeds or labels:

```scheme
(declare-param count Int 3)        ; --param count=5, or 3 without one
(declare-param label String)       ; --param label=…, required
(echo (concat label ": " (to-string count)))
```

A parameter is `String`, `Int` or `Bool`; the value given on the command line is read as that sort, and an optional
default makes the parameter optional. Parameters are declarations, so they come first, with the inputs. The check
reports a value that is not of the sort, and a missing value of a parameter without a default, before any input is
loaded.

**Paths written in the script** are loaded with the same operations: `(nfa:load "automata/a.mata")`. A relative path
is resolved against the directory of the script, so a script and the automata beside it move together. A path given on
the command line is resolved against the current directory, as for any other command: when the script is in a file
with a directory part, a relative one is made absolute before the script sees it, so `$1` and `inputs` hold the
absolute path.

**Literals** make a script complete without any file:

```scheme
(nfa:create-from-regex "a(b|c)*")

(nfa:parse "@NFA-explicit
%Initial q0
%Final q1
q0 a q1")

(nfa:literal :initial (q0) :final (q1)
             :transitions ((q0 a q1) (q1 b q1) (q0 epsilon q1)))

(nft:literal :levels 2 :initial (q0) :final (q0)
             :states ((q0 0) (q1 1))
             :transitions ((q0 a q1) (q1 b q0)))
```

In `nfa:literal` and `nft:literal`, states and symbols are names, as in the [`.mata` format](automata-format); they
are not evaluated. `epsilon` in the symbol position is an ε-transition. `:states` of `nft:literal` gives the level of
each state that is not on level 0.

(script-loading)=

### What loading does

`nfa:load`, `nft:load` and `automaton:load` are the one place where a script operation is more than one library call.
Loading parses the file, mintermizes it if its alphabet is a bitvector alphabet, and constructs the automaton over the
[script alphabet](#alphabets). This is what `load_automaton()` in `tests-integration/src/utils/` does today. The
library function `parse_from_mata()` is not used, because it constructs over `IntAlphabet`, which accepts only numbers
as symbol names.

`nfa:parse`, `nft:parse` and `automaton:parse` do the same with `.mata` text given as a string instead of a file.

Given a list of paths, `load` returns a list. Inputs with a bitvector alphabet are then mintermized together, so that
they share their minterms; declarations load this way. Automata mintermized separately do not share minterms and
must not be combined. The library mintermizes NFAs and AFAs only, so a transducer with a bitvector alphabet is an
error.

The steps are available separately as `intermediate:parse-from-mata`, `intermediate:mintermize` and `nfa:construct` /
`nft:construct`.

(script-alphabets)=

## Alphabets

A script has one **script alphabet**. Every load, literal, and word made with `(word "a" "b")` or `(symbol "a")`
translates symbol names through it, so a name means the same symbol everywhere in the script.

| `(set-option :alphabet …)` | Script alphabet                                                                                                                                                                                                                  |
| -------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `"on-the-fly"` (default)   | An empty `OnTheFlyAlphabet`; a name gets the next free number when it is first seen                                                                                                                                              |
| `"bytes"`                  | An `OnTheFlyAlphabet` in which every byte is registered under its own value first, as in the website playground: a printable ASCII character other than the space and the backslash is named by itself, any other byte by `\xHH` |

`nfa:create-from-regex` produces automata whose symbols are byte values. Under `"on-the-fly"`, the name `a` in a
`.mata` file is the symbol 0 if it is seen first, which is not the byte `a`. A script that combines automata from
regular expressions with automata from files or literals must therefore select `"bytes"`. The option can be set only
before the alphabet is first used.

`(script-alphabet)` returns the script alphabet as a value, for operations that take one. `nfa:create-alphabet`
builds the alphabet of the symbols that occur in the given automata.

## Operations

### Names

The name of an operation is the name of the library function, changed by three rules:

1. `snake_case` becomes `kebab-case`: `project_out` is `nft:project-out`.
2. Abbreviations are written out: `lang` is `language`, `nondet` is `nondeterministic`, `det` is `deterministic`,
   `num` is `number`, `aut` is `automaton`, `eps` is `epsilon`, `symb` is `symbol`. `nfa`, `nft` and `dfa` stay.
3. An operation that returns `Bool` ends in `?` and drops a leading `is` or `are`: `is_lang_empty` is
   `nfa:language-empty?`, `are_equivalent` is `nfa:equivalent?`, `has_epsilon_cycle` is `nft:has-epsilon-cycle?`.

The namespace tells where the function lives:

| Namespace       | Library                                                                  |
| --------------- | ------------------------------------------------------------------------ |
| `nfa:`          | `mata::nfa`, `mata::nfa::builder`, and the member functions of `Nfa`     |
| `nft:`          | `mata::nft`, `mata::nft::builder`, and the member functions of `Nft`     |
| `automaton:`    | No library counterpart; see [below](#the-automaton-namespace)            |
| `strings:`      | `mata::applications::strings` and its `seg_nfa` and `replace` namespaces |
| `alphabet:`     | The member functions of `mata::Alphabet`                                 |
| `intermediate:` | `mata::parser`, `mata::IntermediateAut` and `mata::Mintermization`       |
| `list:`         | No library counterpart                                                   |

### Arguments

- **Required parameters** are positional, in the order of the library function. For a member function, the object
  comes first.
- **Optional parameters** are keyword arguments named after the parameter:
  `(nfa:concatenate lhs rhs :use-epsilon true)`. The value has the sort of the parameter, `Bool`, `Int` or `String`;
  a `Symbol` parameter takes the name of the symbol, as `(symbol "a")` does. The checker rejects a value of another
  sort before the script runs. A call without the keyword gets the default of the library.
- **Entries of the `ParameterMap`** are keyword arguments too: `(nfa:included? lhs rhs :algorithm "naive")`. They are
  called _hints_. Their values are strings, as in the library. Each operation lists the hints it takes and the values
  each accepts, and the checker rejects any other one before the script runs; a value written as an expression rather
  than a string literal is checked when the call is made.
- **Enumerations** are strings in kebab-case: `:jump-mode "append-dont-cares"` is `JumpMode::AppendDontCares`.
- **Not available:** callbacks, such as `macrostate_discover`, and output parameters, such as `prod_map`,
  `state_renaming` and `subset_map`. Counterexample output parameters are available through
  [`witness`](#witnesses).

A hint value is an ordinary expression, so one script can compare algorithms:

```scheme
(list:map (lambda (algorithm)
            (time algorithm (nfa:included? lhs rhs :algorithm algorithm)))
          (list "naive" "antichains"))
```

The default of a hint can be changed for the rest of the scope. A hint written in the application wins over the
changed default, which wins over the default of the library:

```scheme
(set-option :default (nfa:included? :algorithm "naive"))
```

### One call per application

Each application is one library call, with these exceptions:

- `load` and `parse`, described in [What loading does](#what-loading-does).
- A member function that changes its object, such as `Nfa::trim()` or `Nfa::make_complete()`, is applied to a copy
  when the operand is a named value, because values are immutable. An unnamed intermediate result is changed in place.
  Either way, the operation returns the changed automaton.
- The library offers some functions in several overloads that differ only in whether an operand is copied or moved,
  such as `union_nondet(Nfa&&, const Nfa&)` and `Nft::to_nfa_copy()` / `Nft::to_nfa_move()`. A script has one
  operation for all of them. A named operand is passed by reference and an unnamed intermediate result is moved, so
  the overload that runs can be read from the script.
- With more than two operands, `(nfa:intersection a b c)` is `(nfa:intersection (nfa:intersection a b) c)`. This holds
  for every operation whose library function takes two automata and returns one: `union-nondeterministic`,
  `intersection` and `concatenate` of both namespaces, and `nft:compose`.

### NFA operations

| Script                                                                                   | Library                                                                         |
| ---------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| `nfa:union-nondeterministic`, `nfa:union-deterministic-complete`                         | `union_nondet`, `union_det_complete`                                            |
| `nfa:intersection`, `nfa:product`, `nfa:language-difference`                             | `intersection`, `product`, `lang_difference`                                    |
| `nfa:concatenate`, `nfa:concatenate-nth-power`                                           | `concatenate`, `concatenate_nth_power`                                          |
| `nfa:complement`                                                                         | `complement`                                                                    |
| `nfa:left-quotient`, `nfa:right-quotient`                                                | `left_quotient`, `right_quotient`                                               |
| `nfa:determinize`, `nfa:as-dfa`, `nfa:minimize`, `nfa:reduce`                            | `determinize`, `as_dfa`, `minimize`, `reduce`                                   |
| `nfa:revert`, `nfa:remove-epsilon`, `nfa:trim`, `nfa:make-complete`                      | `revert`, `remove_epsilon`, `trim`, `Nfa::make_complete`                        |
| `nfa:decode-utf8`, `nfa:get-one-letter-automaton`                                        | `Nfa::decode_utf8`, `Nfa::get_one_letter_aut`                                   |
| `nfa:language-empty?`, `nfa:intersection-empty?`                                         | `Nfa::is_lang_empty`, `is_intersection_empty`                                   |
| `nfa:included?`, `nfa:equivalent?`, `nfa:universal?`                                     | `is_included`, `are_equivalent`, `Nfa::is_universal`                            |
| `nfa:in-language?`, `nfa:in-language-prefix?`                                            | `Nfa::is_in_lang`, `Nfa::is_in_lang_prefix`                                     |
| `nfa:deterministic?`, `nfa:complete?`, `nfa:minimal?`, `nfa:flat?`, `nfa:identical?`     | `Nfa::is_deterministic`, `is_complete`, `is_minimal`, `is_flat`, `is_identical` |
| `nfa:get-word`, `nfa:get-shortest-word`, `nfa:get-words`                                 | `Nfa::get_word`, `get_shortest_word`, `get_words`                               |
| `nfa:get-word-from-complement`, `nfa:get-word-from-language-difference`                  | `Nfa::get_word_from_complement`, `get_word_from_lang_difference`                |
| `nfa:number-of-states`, `nfa:number-of-transitions`                                      | `Nfa::num_of_states`, `Delta::num_of_transitions`                               |
| `nfa:create-from-regex`                                                                  | `builder::create_from_regex`, with the members of `Re2Options` as keywords      |
| `nfa:create-single-word-nfa`, `nfa:create-empty-string-nfa`, `nfa:create-sigma-star-nfa` | The `builder` functions of the same names                                       |
| `nfa:create-random-nfa-tabakov-vardi`                                                    | `builder::create_random_nfa_tabakov_vardi`                                      |
| `nfa:construct`                                                                          | `builder::construct`                                                            |
| `nfa:create-alphabet`, `nfa:encode-word`                                                 | `create_alphabet`, `encode_word`                                                |
| `nfa:print-to-mata`, `nfa:print-to-dot`                                                  | `Nfa::print_to_mata`, `Nfa::print_to_dot`; both return a string                 |
| `nfa:load`, `nfa:parse`, `nfa:literal`                                                   | No single library counterpart                                                   |

### NFT operations

| Script                                                                                   | Library                                                                              |
| ---------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| `nft:union-nondeterministic`, `nft:intersection`                                         | `union_nondet`, `intersection`                                                       |
| `nft:concatenate`, `nft:concatenate-nth-power`, `nft:complement`                         | `concatenate`, `concatenate_nth_power`, `complement`                                 |
| `nft:compose`                                                                            | `compose`                                                                            |
| `nft:apply`                                                                              | `Nft::apply`, to an `Nfa` or to a `Word`                                             |
| `nft:project-out`, `nft:project-to`, `nft:insert-level`, `nft:insert-levels`             | `project_out`, `project_to`, `insert_level`, `insert_levels`                         |
| `nft:invert-levels`, `nft:unwind-jumps`                                                  | `invert_levels`, `Nft::unwind_jumps`                                                 |
| `nft:determinize`, `nft:reduce`, `nft:revert`, `nft:remove-epsilon`, `nft:trim`          | `determinize`, `reduce`, `revert`, `remove_epsilon`, `Nft::trim`                     |
| `nft:language-empty?`, `nft:included?`, `nft:equivalent?`, `nft:universal?`              | `Nft::is_lang_empty`, `is_included`, `are_equivalent`, `Nft::is_universal`           |
| `nft:in-language?`, `nft:in-language-by-levels?`                                         | `Nft::is_in_lang`, `Nft::is_in_lang_by_levels`                                       |
| `nft:deterministic?`, `nft:complete?`, `nft:identical?`                                  | `Nft::is_deterministic`, `is_complete`, `is_identical`                               |
| `nft:contains-jump-transitions?`, `nft:has-epsilon-cycle?`                               | `Nft::contains_jump_transitions`, `has_epsilon_cycle`                                |
| `nft:to-nfa`, `nft:to-nfa-update`                                                        | `Nft::to_nfa_copy` / `to_nfa_move`, `Nft::to_nfa_update_copy` / `to_nfa_update_move` |
| `nft:from-nfa-with-levels-zero`, `nft:from-nfa-with-levels-advancing`                    | The `builder` functions of the same names                                            |
| `nft:create-single-word-nft`, `nft:create-empty-string-nft`, `nft:create-sigma-star-nft` | The `builder` functions of the same names                                            |
| `nft:construct`                                                                          | `builder::construct`                                                                 |
| `nft:number-of-states`, `nft:print-to-mata`, `nft:print-to-dot`                          | `Nft::num_of_states`, `Nft::print_to_mata`, `Nft::print_to_dot`                      |
| `nft:load`, `nft:parse`, `nft:literal`                                                   | No single library counterpart                                                        |

There is no `nft:minimize`: the library declares `nft::minimize` only under `MATA_NFT_NOT_IMPLEMENTED`.

### The `automaton:` namespace

For every operation that exists under the same name in both `nfa:` and `nft:`, there is an operation of that name in
`automaton:`, for example `automaton:intersection`, `automaton:determinize` and `automaton:included?`. It accepts an
`Automaton`, an `Nfa` or an `Nft`, and calls the `nfa:` or the `nft:` operation according to what the operands are
when the script runs. Operands of different kinds are an error; convert explicitly with
`nft:from-nfa-with-levels-zero` or `nft:to-nfa`.

`automaton:load` and `automaton:parse` accept either kind and return an `Automaton`.

### Other namespaces

| Script                                                                                                            | Library                                                                        |
| ----------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------ |
| `strings:get-shortest-words`, `strings:get-longest-words`                                                         | `get_shortest_words`, `get_longest_words`                                      |
| `strings:get-words-of-lengths`, `strings:get-word-lengths`                                                        | `get_words_of_lengths`, `get_word_lengths`                                     |
| `strings:get-accepted-symbols`, `strings:language-epsilon?`                                                       | `get_accepted_symbols`, `is_lang_eps`                                          |
| `strings:get-segments`                                                                                            | `seg_nfa::Segmentation` for the given epsilon symbols, then its `get_segments` |
| `strings:reluctant-nfa`, `strings:create-identity`                                                                | `replace::reluctant_nfa`, `replace::create_identity`                           |
| `strings:create-identity-with-single-symbol-replace`                                                              | The `replace` function of the same name                                        |
| `strings:replace-reluctant-regex`, `strings:replace-reluctant-literal`, `strings:replace-reluctant-single-symbol` | The `replace` functions of the same names                                      |
| `alphabet:get-alphabet-symbols`, `alphabet:translate-symbol`, `alphabet:reverse-translate-symbol`                 | `Alphabet::get_alphabet_symbols`, `translate_symb`, `reverse_translate_symbol` |
| `intermediate:parse-from-mata`                                                                                    | `parser::parse_mf` followed by `IntermediateAut::parse_from_mf`                |
| `intermediate:mintermize`                                                                                         | `Mintermization::mintermize`, on one NFA or on a list of NFAs                  |

Noodlification (`noodlify`, `noodlify_for_equation`, `noodlify_for_transducer`) is specific to one string solver and
is not part of the language.

### Core functions

`not`, `=`, `<`, `<=`, `+`, `-` on `Bool`, `Int`, `String` and `Word` as applicable; `(concat string…)`, which joins
strings; `(to-string value)`, which shows any value as one line, the way `print` would; `(string->int string)`; `none?`
and `get` on an `Option`; `list`; `(word "a" "b")` and `(symbol "a")`, which translate names through the script
alphabet; `(script-alphabet)`.

## Reporting results

| Form                        | Effect                                                                          | Value         |
| --------------------------- | ------------------------------------------------------------------------------- | ------------- |
| `(check condition)`         | Prints `true` or `false`                                                        | The `Bool`    |
| `(check "label" condition)` | Prints the record `label: true` or `label: false`                               | The `Bool`    |
| `(assert condition)`        | Reports a failure and its place if the condition is false; the script continues | The `Bool`    |
| `(print value)`             | Prints the value to standard output                                             | The value     |
| `(write value path)`        | Writes the value to a file                                                      | The value     |
| `(echo string)`             | Prints the string                                                               | The string    |
| `(get-witness)`             | See [Witnesses](#witnesses)                                                     | `Option Word` |

`print` and `write` return their argument, so they can stand in the middle of a pipeline. An automaton is printed in
the `.mata` format; use `nfa:print-to-dot` to print a DOT graph instead: `(print (nfa:print-to-dot a))`.

In an interactive session, the value of every top-level expression is shown, with an automaton summarised by its
kind and size rather than printed in full. When a script is run, the value of a top-level expression is dropped.

Results and timing records go to standard output, each written as soon as it is printed, so a run stopped by a time
limit keeps what it printed; errors and the [trace](#options) go to standard error.

(script-exit-status)=

### Exit status

| Status | Meaning                                                                                |
| ------ | -------------------------------------------------------------------------------------- |
| 0      | The script ran to its end and no assertion failed                                      |
| 1      | At least one `assert` failed                                                           |
| 2      | An error stopped the script: a failed check of the script, a file, a sort, the library |

`check` never changes the status. A one-off query that should fail a shell command uses `assert`. In an interactive
session, an error ends the form, not the session, and makes the session end with status `2`.

### Witnesses

Several decision procedures of the library can return a word that explains their answer through an output parameter:
`is_included`, `are_equivalent`, `is_universal`, `is_lang_empty` and `is_intersection_empty`. There are two ways to
get it.

Each of these procedures has one verdict that a word can explain: inclusion, equivalence and universality when they
do not hold, and the two emptiness checks when the language is not empty. `(witness application)` makes the call with
the output parameter and returns the stored `Word` for that verdict, and `none` for the other one. The call is made
once, so the verdict is not computed separately. The application must be a call of `nfa:included?`,
`nfa:equivalent?`, `nfa:universal?`, `nfa:language-empty?` or `nfa:intersection-empty?`; anything else is an error:

```scheme
(define counterexample (witness (nfa:included? smaller bigger)))
(assert (none? counterexample))
```

With `(set-option :produce-witnesses true)`, every such call passes the output parameter, and `(get-witness)` returns
the witness of the most recent decision operation, the `nfa:` and `nft:` operations whose names end in `?`, and `none`
when that operation stored none. This is the convenient form in an interactive session:

```scheme
(set-option :produce-witnesses true)
(check (nfa:included? smaller bigger))   ; false
(get-witness)                            ; a b b
```

Asking for a witness can make a decision procedure slower, so `:produce-witnesses` is off by default. Without it, a
call outside `witness` computes no witness, and `(get-witness)` returns `none` after it. A witness is a word only: the
states of an accepting run come from `nfa:get-accepting-run`.

## Timing

Nothing is measured unless the script asks. `(time label expression…)` evaluates the expressions, prints one record,
and returns the last value:

```scheme
(define product (time "intersection" (nfa:intersection lhs rhs)))
(check (time "emptiness" (nfa:language-empty? product)))
```

```text
intersection: 0.0481
emptiness: 0.0007
```

- A record is a line `label: seconds`, measured with a monotonic clock, with six fractional digits. This is the format
  the benchmark binaries in `tests-integration/src/` print today.
- Every record of a run has its own label: a reader of the records, such as `pycobench`, keys them by label, so a label
  printed twice is an error. A `time` form that is evaluated several times needs a label that differs each time, as
  `(time (concat "round-" (to-string i)) …)`.
- Loading is measured when it is written inside a `time` form: `(time "parsing" (nfa:load $1))`.
- `(set-option :time-operations true)` prints a record for every operation that is called, labelled by the name of
  the operation, without any `time` form in the script; a repeated operation gets `#2`, `#3`, … after its name. Set
  before the declarations, it also records their loading, as `parsing`, as the benchmark binaries do.

## Options

| Option                 | Values                              | Meaning                                                                                                               |
| ---------------------- | ----------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `:alphabet`            | `"on-the-fly"` (default), `"bytes"` | The [script alphabet](#alphabets)                                                                                     |
| `:default`             | An application with only hints      | Changes the default of a hint; see [Arguments](#arguments)                                                            |
| `:produce-witnesses`   | `false` (default), `true`           | See [Witnesses](#witnesses)                                                                                           |
| `:time-operations`     | `false` (default), `true`           | See [Timing](#timing)                                                                                                 |
| `:optimize`            | `false` (default), `true`           | See [Optimizer](#optimizer)                                                                                           |
| `:trace`               | `false` (default), `true`           | Prints every library call that is made, with its hints, to standard error                                             |
| `:loops-and-recursion` | `false` (default), `true`           | Allows `loop` and functions that call themselves, for the whole file; see [Loops and recursion](#loops-and-recursion) |

`set-info` attaches information that does not change evaluation; see [Script information](#script-information).

## Optimizer

By default a script runs exactly as written. `(set-option :optimize true)` allows the interpreter to replace the
calls a script asks for with cheaper ones.

- **What is preserved:** the language of every automaton, and every result that depends only on languages.
- **What is not preserved:** the structure of automata. The number of states, the numbering of states, and which
  witness is returned may differ.
- **What is never changed:** the operand of a form that shows structure, namely `print`, `write`, the
  `number-of-states` and `number-of-transitions` operations, and the predicates on structure such as
  `nfa:deterministic?`, is computed as written.
- **What is reported:** timing records and the trace describe the calls that were made, not the calls that were
  written.

The first rewrites planned are:

| Written                                        | Run                                    |
| ---------------------------------------------- | -------------------------------------- |
| `(nfa:language-empty? (nfa:intersection a b))` | `is_intersection_empty(a, b)`          |
| A definition whose value is never used         | Nothing                                |
| The same application of the same values twice  | One call                               |
| The last use of a named value                  | The operand is moved instead of copied |

A benchmark that measures one specific operation should leave the optimizer off.

## Portable core

The `.emp` programs were interpreted by several automata libraries, to compare them on the same inputs. A script whose
header says `:profile core`, as `(mata-script "0.1.0" :profile core)`, promises to stay within a subset that another
library can implement, and the checker rejects anything outside of it, before `check` reports and before `run` runs:

- **Forms:** `declare-input`, `declare-inputs`, `define` of a value, `let`, `->`, `time`, `check`, `assert`, `print`,
  `echo`, `set-option`, `set-info`; no defined functions, no `lambda`, no conditionals, and no loops or recursion:
  `:loops-and-recursion` cannot be turned on.
- **Sorts:** `Nfa`, `Alphabet`, `Bool`, `Int`, `String`, `(List T)`.
- **Operations:** `nfa:load`, `nfa:union-nondeterministic`, `nfa:intersection`, `nfa:complement`, `nfa:concatenate`,
  `nfa:determinize`, `nfa:minimize`, `nfa:language-empty?`, `nfa:included?`, `nfa:equivalent?`, `nfa:universal?`,
  `(script-alphabet)`, and the `list:` functions.

A script in the portable core is always guaranteed to [terminate](#termination).

An interpreter for another library must reject an operation it does not have. It may ignore a hint it does not know,
with a warning, because a hint only selects how a result is computed.

(script-information)=

## Script information

`set-info` attaches information to a script that does not change what it computes; `get-info` reads it back, and is
`none` for a key the script did not set. The keys are fixed, so that a tool reading a set of scripts finds the same
fact under the same key:

| Key            | Sort     | Meaning                                                                     |
| -------------- | -------- | --------------------------------------------------------------------------- |
| `:name`        | `String` | A short name, as `"email-filter"`                                           |
| `:description` | `String` | What the script computes                                                    |
| `:source`      | `String` | Where the script or its benchmark comes from, as `"CADE'23, b-regex"`       |
| `:license`     | `String` | The license of the script                                                   |
| `:authors`     | `String` | Who wrote it                                                                |
| `:expects`     | `Bool`   | The verdict the script is expected to print, as SMT-LIB's `:status`         |
| `:x-…`         | Any      | Free for any use, as `:x-timeout`; never given a meaning by a later version |

Any other key is an error, and so is a value of the wrong sort. The profile is not information: it changes what the
checker accepts, so it is part of the header.

(versions)=

## Versions

A script names the version of the language it is written for in a header, the first form of the file:

```scheme
#!/usr/bin/env -S mata script run
(mata-script "0.1.0"
  :requires (hints witnesses)   ; optional features the script needs
  :profile core)                ; optional: the portable core
```

- **The framing never changes.** In every version, a script is an optional `#!` line, whitespace and `;` comments,
  then `(mata-script "MAJOR.MINOR.PATCH" …)`, with whitespace and comments allowed between its tokens. An interpreter
  reads the version from this framing before it parses the rest, so a script for a version it does not run is
  rejected with the version it needs, as `the script needs Mata Script 0.2.0; this mata runs Mata Script 0.1.x`, never
  with a syntax error about something that version added. A header typed into a REPL session is checked the same way.
- **A major version may break anything:** syntax, the meaning of a form, a removed or renamed operation, the printed
  formats, the exit statuses. An interpreter runs one major version.
- **A minor version only adds,** from `1.0.0` on: an interpreter of `1.4` runs scripts for `1.0` to `1.4`, and rejects
  a script for `1.5`. Before `1.0.0`, every minor version may break, so an interpreter of `0.1` runs only `0.1` scripts.
- **A patch version** never changes what a script means; an interpreter of `0.1.0` runs a script that declares
  `0.1.7`.
- **A version selects the names a script sees.** Every operation records the version that added it, and a script
  sees only those of its declared version. Every namespaced name and every name of the language is reserved, whether
  the declared version has it yet or not, so adding an operation cannot change the meaning of a script that defined a
  name of its own.
- **`:requires`** lists optional parts of the language that the script needs: `hints`, `witnesses`, `transducers`,
  `loops-and-recursion`, `optimizer`, `trace`, `automaton`, `strings` and `intermediate`. An interpreter without one
  of them rejects the script before running it, naming the feature, rather than failing at the first form that needs
  it. `mata formats` lists the features and whether the build implements them.
- **`:profile`** selects a [profile](#portable-core); `core` is the only one, and any other name is an error.
- **A script without a header** is read as the version of the interpreter that runs it. That suits a one-line
  `--expression` and an interactive session, but a script kept in a file should name its version; `mata script check`
  warns about one that does not.
- **Deprecation.** A minor version may deprecate a name with a warning; the name is removed only in the next major
  version.

The language starts at `0.1.0`. The version describes the language only: the behaviour of the library, such as the
numbering of states or the default algorithm of an operation, and the options of the `mata` command have versions of
their own, which `mata version` prints beside it.

## Examples

A regression specification that needs no input file:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define language (nfa:create-from-regex "a(b|c)*"))
(define minimal (-> language nfa:determinize nfa:minimize))

(assert (nfa:deterministic? minimal))
(assert (nfa:equivalent? language minimal))
(assert (nfa:in-language? minimal (word "a" "b" "c")))
(assert (not (nfa:in-language? minimal (word "b"))))
```

The image of a language under a transducer, checked against a specification, with a function:

```scheme
(mata-script "0.1.0")
(declare-input transducer Nft)
(declare-input language Nfa)
(declare-input specification Nfa)

(define (image (transducer Nft) (language Nfa))
  (nft:to-nfa (nft:apply transducer language)))

(define counterexample
  (witness (nfa:included? (image transducer language) specification)))
(if (none? counterexample)
    (echo "the image satisfies the specification")
    (print (get counterexample)))
```

A script that works on an input of either kind:

```scheme
(mata-script "0.1.0" :requires (automaton))
(declare-input a Automaton)
(declare-input b Automaton)

(check (automaton:included? a b))
(check (automaton:included? b a))
```

## Open questions

- **Command-line interface.** Settled in [The `mata` command](cli): inputs are positional or named with `--input`,
  values come from `declare-param` and `--param`, options and hint defaults are set in the script only, and `check`
  writes its report to standard error. A script written inline in a shell command or in a `pycobench` job still needs
  care, because both can replace `$1` themselves before the interpreter sees it.
- **Loops and recursion** are designed as an option that the first interpreter leaves out. Whether two functions
  should be able to call each other by name is open.
- **Runaway scripts.** A script without the termination guarantee can run forever. Whether the interpreter should
  limit the depth of calls and the number of rounds of a loop is open.
- **Alternating automata** are not covered, as the library does not support them.
- **The exact set of rewrites** of the optimizer, and whether the list of forms that show structure is the right one.
