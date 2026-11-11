# Automata script language

**Status: design only.** Nothing described on this page is implemented yet. The page records the agreed design of a
small language for scripting Mata, so that the interpreter and the command-line interface can be built against it.
The language has no final name and no file extension yet; see [Open questions](#open-questions).

A script loads automata, applies a sequence of automata operations to them, and reports the outcome: a verdict, a
witness, a printed automaton, or timing records. It replaces two things that exist today:

- the `.emp` programs of [VeriFIT/nfa-program-parser](https://github.com/VeriFIT/nfa-program-parser/), which can only
  express a straight-line sequence drawn from a small fixed set of operations, and
- the hand-written benchmark binaries in `tests-integration/src/`, one C++ file per sequence of operations.

The intended uses are benchmark programs, one-off queries from a shell, regression specifications that assert expected
results, and interactive sessions, including the playground on the website.

## A first example

The script below is the `email-filter` benchmark: it intersects the first four input automata and checks whether the
intersection is included in the fifth one.

```scheme
(declare-inputs automata (List Nfa))

(define filters (list:reduce nfa:intersection (list:take automata 4)))
(check (time "inclusion" (nfa:included? filters (list:nth automata 4))))
```

Run on five `.mata` files, it prints one timing record and the verdict, for example:

```text
inclusion: 0.0123
true
```

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
   so a misspelled name is not reported after an hour of determinization.
6. **Functions are ordinary values.** A function can be defined inside another function, passed as an argument,
   returned as a result and stored in a list; see [Functions](#functions).
7. **Evaluation is sequential.** Nothing is evaluated in parallel.

## Lexical structure

| Token           | Form                                                                   | Examples                     |
| --------------- | ---------------------------------------------------------------------- | ---------------------------- |
| Comment         | `;` to the end of the line                                             | `; intersect the filters`    |
| Name            | Letters, digits and `- _ ? ! * + / < = > .`, not starting with a digit | `filters`, `list:take`       |
| Namespaced name | `namespace:name`                                                       | `nfa:intersection`           |
| Keyword         | `:name`                                                                | `:algorithm`                 |
| Input reference | `$` followed by a position (from 1) or a name                          | `$1`, `$specification`       |
| Integer         | Decimal digits                                                         | `0`, `42`                    |
| String          | Between `"`; the only escape is `""` for one `"`; may span lines       | `"a(b\|c)*"`, `"say ""hi"""` |
| Constant        | `true`, `false`, `none`                                                |                              |

Backslash has no meaning in a string, so regular expressions and `.mata` text can be written without doubling it.

## Grammar

```ebnf
script      = { form } ;
form        = definition | declaration | directive | expression ;

definition  = "(" "define" name [ sort ] expression ")"
            | "(" "define" "(" name { parameter } ")" body ")" ;
body        = { definition } expression { expression } ;
parameter   = name | "(" name sort ")" ;
declaration = "(" "declare-input" name sort ")"
            | "(" "declare-inputs" name sort ")" ;
directive   = "(" "set-option" keyword expression ")" | "(" "get-option" keyword ")"
            | "(" "set-info" keyword expression ")"   | "(" "get-info" keyword ")"
            | "(" "push" ")" | "(" "pop" ")" | "(" "reset" ")" | "(" "exit" ")" ;

expression  = constant | integer | string | name | input-reference
            | "(" expression { expression } { keyword expression } ")"
            | "(" "let" "(" { "(" name expression ")" } ")" body ")"
            | "(" "lambda" "(" { parameter } ")" body ")"
            | "(" "if" expression expression expression ")"
            | "(" "cond" { "(" expression expression ")" } "(" "else" expression ")" ")"
            | "(" "do" expression { expression } ")"
            | "(" ( "and" | "or" ) { expression } ")"
            | "(" "->" expression { step } ")"
            | "(" "as" expression sort ")"
            | "(" "time" expression expression { expression } ")"
            | "(" "witness" expression ")"
            | "(" ( "nfa:literal" | "nft:literal" ) { keyword literal-data } ")" ;
step        = name | "(" expression { expression } { keyword expression } ")" ;
sort        = name | "(" name { sort } ")" ;
literal-data = name | integer | "(" { literal-data } ")" ;
```

Declarations and directives are allowed only at the top level. A definition is allowed at the top level and at the
start of a body, where it is local to that body. Everything else is an expression and can be nested.

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

- **Before the script runs:** syntax, unknown names, unknown keywords, the number of arguments in a call of a named
  operation or function, and the number of declared inputs against the inputs given.
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
- `let` binds names for the extent of its body, each binding seeing the previous ones.
- `if` and `cond` evaluate only the chosen branch. `and` and `or` stop at the first operand that decides the result.
  This matters because it decides which library calls are made.
- `do` evaluates its expressions in order and returns the last value.

### Functions

A function is an ordinary value. It can be defined inside another function, passed as an argument, returned as a
result, and stored in a list.

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
- **Order.** A name is visible after its definition, and not inside it. A function therefore cannot call itself by
  name, and two functions cannot call each other.
- **Hiding.** An inner scope may define a name that an outer scope already has. The inner one is used inside that
  scope.

#### Termination

The language has no loop and no recursion by name. A script can still run forever, because a function can be passed
to itself, as in `((lambda (f) (f f)) (lambda (f) (f f)))`.

The checker therefore reports, for each script, whether it is guaranteed to terminate, provided that the library
operations it calls do. The guarantee is given when every function in the script is used in these ways only:

- it is bound to a name,
- it is called, or
- it is given to a `list:` function as the function to apply.

A script gets no guarantee when it passes a function to a defined function or to a `lambda`, returns a function from
one, or puts a function into a list. The example above is such a script: `included-with` returns a function, and
`relations` is a list of functions.

This is a report, not an error: the script runs either way. The checker is conservative, so a script without the
guarantee may still terminate.

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
`list:map`, `list:filter`, `list:reduce` and `list:fold`:

```scheme
(list:map nfa:determinize automata)
(list:filter (lambda (a) (not (nfa:language-empty? a))) automata)
(list:reduce nfa:intersection automata)                    ; error on an empty list
(list:fold nfa:union-nondeterministic empty-language automata) ; starts from empty-language
```

`list:reduce` and `list:fold` combine from left to right, one call per element.

## Inputs

A script gets its automata from four sources.

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

- Each `declare-input` takes the input given under its name, or else the next positional input. `declare-inputs`
  takes all the positional inputs that are left and must come last.
- Declarations must come before every other form except `set-option` and `set-info`.
- All declared inputs are loaded together, when the first other form is reached. A file whose kind does not match the
  declared sort is an error at that point. Declaring an input as `Automaton` accepts either kind.
- The loading done by declarations is never part of a [timing record](#timing).

**Paths written in the script** are loaded with the same operations: `(nfa:load "automata/a.mata")`. A relative path
is resolved against the directory of the script.

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
  `(nfa:concatenate lhs rhs :use-epsilon true)`.
- **Entries of the `ParameterMap`** are keyword arguments too: `(nfa:included? lhs rhs :algorithm "naive")`. They are
  called _hints_. Their values are strings, as in the library.
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
  for every operation whose library function takes two automata and returns one.

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

`not`, `=`, `<`, `<=`, `+`, `-` on `Bool`, `Int`, `String` and `Word` as applicable; `none?` and `get` on an
`Option`; `list`; `(word "a" "b")` and `(symbol "a")`, which translate names through the script alphabet;
`(script-alphabet)`.

## Reporting results

| Form                 | Effect                                                            | Value        |
| -------------------- | ----------------------------------------------------------------- | ------------ |
| `(check condition)`  | Prints `true` or `false`                                          | The `Bool`   |
| `(assert condition)` | Reports a failure if the condition is false; the script continues | The `Bool`   |
| `(print value)`      | Prints the value to standard output                               | The value    |
| `(write value path)` | Writes the value to a file                                        | The value    |
| `(echo string)`      | Prints the string                                                 | The string   |
| `(get-witness)`      | See [Witnesses](#witnesses)                                       | `Option Run` |

`print` and `write` return their argument, so they can stand in the middle of a pipeline. An automaton is printed in
the `.mata` format; use `nfa:print-to-dot` to print a DOT graph instead: `(print (nfa:print-to-dot a))`.

In an interactive session, the value of every top-level expression is shown, with an automaton summarised by its
kind and size rather than printed in full. When a script is run, the value of a top-level expression is dropped.

Results and timing records go to standard output; errors and the [trace](#options) go to standard error.

### Exit status

| Status | Meaning                                                                                |
| ------ | -------------------------------------------------------------------------------------- |
| 0      | The script ran to its end and no assertion failed                                      |
| 1      | At least one `assert` failed                                                           |
| 2      | An error stopped the script: a failed check of the script, a file, a sort, the library |

`check` never changes the status. A one-off query that should fail a shell command uses `assert`. In an interactive
session, an error ends the form, not the session.

### Witnesses

Several decision procedures of the library can return a word that explains their answer through an output parameter:
`is_included`, `are_equivalent`, `is_universal`, `is_lang_empty` and `is_intersection_empty`. There are two ways to
get it.

Each of these procedures has one verdict that a word can explain: inclusion, equivalence and universality when they
do not hold, and the two emptiness checks when the language is not empty. `(witness application)` makes the call with
the output parameter and returns the stored `Run` for that verdict, and `none` for the other one. The call is made
once, so the verdict is not computed separately:

```scheme
(define counterexample (witness (nfa:included? smaller bigger)))
(assert (none? counterexample))
```

With `(set-option :produce-witnesses true)`, every such call passes the output parameter, and `(get-witness)` returns
the witness of the most recent one. This is the convenient form in an interactive session:

```scheme
(set-option :produce-witnesses true)
(check (nfa:included? smaller bigger))   ; false
(get-witness)                            ; (run :word ("a" "b" "b") :path (0 3 4 7))
```

Asking for a witness can make a decision procedure slower, so `:produce-witnesses` is off by default.

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

- A record is a line `label: seconds`, measured with a monotonic clock. This is the format the benchmark binaries in
  `tests-integration/src/` print today.
- The label is a string expression. A `time` form that is evaluated several times prints several records.
- Loading is measured only when it is written inside a `time` form: `(time "parsing" (nfa:load $1))`. The loading done
  by declarations cannot be inside one, so the usual benchmark script, declarations followed by timed operations, does
  not measure loading.
- `(set-option :time-operations true)` prints a record for every operation that is called, labelled by the name of
  the operation, without any `time` form in the script.

## Options

| Option               | Values                              | Meaning                                                                   |
| -------------------- | ----------------------------------- | ------------------------------------------------------------------------- |
| `:alphabet`          | `"on-the-fly"` (default), `"bytes"` | The [script alphabet](#alphabets)                                         |
| `:default`           | An application with only hints      | Changes the default of a hint; see [Arguments](#arguments)                |
| `:produce-witnesses` | `false` (default), `true`           | See [Witnesses](#witnesses)                                               |
| `:time-operations`   | `false` (default), `true`           | See [Timing](#timing)                                                     |
| `:optimize`          | `false` (default), `true`           | See [Optimizer](#optimizer)                                               |
| `:trace`             | `false` (default), `true`           | Prints every library call that is made, with its hints, to standard error |

`set-info` attaches information that does not change evaluation, such as `(set-info :profile "core")`.

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

The `.emp` programs were interpreted by several automata libraries, to compare them on the same inputs. A script that
starts with `(set-info :profile "core")` promises to stay within a subset that another library can implement, and the
checker rejects anything outside of it:

- **Forms:** `declare-input`, `declare-inputs`, `define` of a value, `let`, `->`, `time`, `check`, `assert`, `print`,
  `echo`, `set-option`, `set-info`; no defined functions, no `lambda` and no conditionals.
- **Sorts:** `Nfa`, `Alphabet`, `Bool`, `Int`, `String`, `(List T)`.
- **Operations:** `nfa:load`, `nfa:union-nondeterministic`, `nfa:intersection`, `nfa:complement`, `nfa:concatenate`,
  `nfa:determinize`, `nfa:minimize`, `nfa:language-empty?`, `nfa:included?`, `nfa:equivalent?`, `nfa:universal?`,
  `(script-alphabet)`, and the `list:` functions.

A script in the portable core is always guaranteed to [terminate](#termination).

An interpreter for another library must reject an operation it does not have. It may ignore a hint it does not know,
with a warning, because a hint only selects how a result is computed.

## Examples

A regression specification that needs no input file:

```scheme
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
(declare-input a Automaton)
(declare-input b Automaton)

(check (automaton:included? a b))
(check (automaton:included? b a))
```

## Open questions

- **Name and file extension** of the language.
- **Command-line interface.** How a script and its inputs are given, how an input is named, how an option or the
  default of a hint is set from the command line, and how the results of checking, including the termination report,
  are shown. A script written inline in a shell command or in a `pycobench` job needs care, because both can replace
  `$1` themselves before the interpreter sees it.
- **Loops and recursion by name** are left out. A loop would serve a computation whose number of rounds depends on the
  automata computed so far, such as a fixpoint. `loop` and `recur` are reserved so that one can be added without
  breaking scripts.
- **Runaway scripts.** A script without the termination guarantee can run forever. Whether the interpreter should
  limit the depth of calls is open.
- **Alternating automata** are not covered, as the library does not support them.
- **The exact set of rewrites** of the optimizer, and whether the list of forms that show structure is the right one.
