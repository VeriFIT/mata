# (Auto)Mata Script Language {#script-language}

**Status: specification, partly implemented.** The page records the agreed design of **Mata Script**, a small,
library-independent language for describing automata operations, with Mata as its first implementation. The `mata`
command runs scripts today; [The `mata` command](@ref cli) lists under its implementation notes which operations and
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

## A first example {#a-first-example}

The smallest scripts need no input file. This one builds an automaton from a regular expression and asks two
questions about it:

```scheme
(mata-script "0.1.0")                 ; the language version the script is written for
(set-option :alphabet "bytes")        ; symbols are bytes, as regular expressions read them

(define greeting (nfa:create-from-regex "hello|hi"))                  ; the language {hello, hi}
(check "accepts-hi" (nfa:in-language? greeting (word "h" "i")))       ; is "hi" in it?
(check "accepts-hey" (nfa:in-language? greeting (word "h" "e" "y")))  ; is "hey" in it?
(echo (nfa:print-to-regex (nfa:minimize greeting) :mode "readable"))  ; the minimal automaton as a regex
```

Saved as `greeting.matasc`, it runs with `mata script run greeting.matasc` and prints one record for each `check`
and the regular expression of the minimal automaton:

```text
accepts-hi: true
accepts-hey: false
h(i|ello)
```

The script below is the `email-filter` benchmark, which takes its automata from files: it intersects the first four
input automata and checks, once with each inclusion algorithm, whether the intersection is included in the fifth one.

```scheme
#!/usr/bin/env -S mata script run
(mata-script "0.1.0")
(set-info :name "email-filter")              ; information for tools; it does not change what runs
(declare-inputs automata (List Nfa))         ; every input file, loaded as one list of NFAs

; the intersection of the first four automata: ((a0 & a1) & a2) & a3
(define filters (list:reduce nfa:intersection (list:take automata 4)))

; a function: one timed check of whether the filters are included in the fifth automaton
(define (included-with algorithm)
  (time (concat algorithm "-time")           ; prints the record "<algorithm>-time: <seconds>"
    (nfa:included? filters (list:nth automata 4) :algorithm algorithm)))

(check "naive" (included-with "naive"))      ; prints "naive: true" or "naive: false"
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

## Design principles {#design-principles}

1. **A script is a sequence of forms, evaluated in order.** The shape is borrowed from SMT-LIB: parenthesised
   commands, `:keyword` attributes, options, a `check` command. The language is only inspired by SMT-LIB. It is not
   conformant, nothing is solved, and every form runs when it is reached.
2. **One application is one library call.** `(nfa:determinize a)` calls `mata::nfa::determinize` and nothing else.
   Arguments are evaluated left to right. No call is added, removed, merged or reordered unless the
   [optimizer](@ref optimizer) is turned on. The exceptions are listed in
   [One call per application](@ref one-call-per-application).
3. **Values are immutable.** An operation never changes its operands. A name always denotes the value it was bound
   to.
4. **Operation names are the names of the library.** A name is the C++ name in kebab-case with abbreviations written
   out, so the API reference documents the script operations too.
5. **Scripts are checked before they run.** Everything that can be decided without loading an input is decided first,
   so a misspelled name is not reported after an hour of determinization. `mata script run` checks the whole script
   before its first form runs, unless it is given `--no-check`.
6. **Functions are ordinary values.** A function can be defined inside another function, passed as an argument,
   returned as a result and stored in a list; see [Functions](@ref functions).
7. **Loops and recursion are opt-in.** They are off unless the script turns them on, and an interpreter may leave them
   out; see [Loops and recursion](@ref loops-and-recursion).
8. **Evaluation is sequential.** Nothing is evaluated in parallel.
9. **A script names its language version.** A `mata-script` header declares the version a script is written for, so
   a later version can change the language without changing what an existing script means; see
   [Versions](@ref script-versions).

## Lexical structure {#lexical-structure}

| Token           | Form                                                             | Examples                     |
| --------------- | ---------------------------------------------------------------- | ---------------------------- |
| Comment         | `;` to the end of the line; a `#!` first line is skipped too     | `; intersect the filters`    |
| Name            | Letters, digits and `- _ ? ! * + / < = > .`; no initial digit    | `filters`, `list:take`       |
| Namespaced name | `namespace:name`                                                 | `nfa:intersection`           |
| Keyword         | `:name`                                                          | `:algorithm`                 |
| Input reference | `$` followed by a position (from 1) or a name                    | `$1`, `$specification`       |
| Integer         | Decimal digits                                                   | `0`, `42`                    |
| String          | Between `"`; the only escape is `""` for one `"`; may span lines | `"a(b\|c)*"`, `"say ""hi"""` |
| Constant        | `true`, `false`, `none`                                          |                              |

Backslash has no meaning in a string, so regular expressions and `.mata` text can be written without doubling it. A
namespaced name always names an operation: a script cannot define one, so an operation added to a later version never
clashes with a name a script chose.

## Grammar {#grammar}

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
works. `loop` and `recur` need the option described in [Loops and recursion](@ref loops-and-recursion).

## Values and sorts {#values-and-sorts}

| Sort                    | Value                                                                                  |
| ----------------------- | -------------------------------------------------------------------------------------- |
| `Nfa`                   | `mata::nfa::Nfa`                                                                       |
| `Nft`                   | `mata::nft::Nft`                                                                       |
| `Automaton`             | Either an `Nfa` or an `Nft`; which one is known only when the script runs              |
| `IntermediateAutomaton` | `mata::IntermediateAut`: a parsed automaton that is not constructed yet                |
| `Alphabet`              | `mata::Alphabet`                                                                       |
| `Symbol`                | `mata::Symbol`; `epsilon` and `dont-care` are predefined                               |
| `Word`                  | `mata::Word`                                                                           |
| `Run`                   | `mata::nfa::Run`: a word and a path                                                    |
| `Bool`, `Int`, `String` |                                                                                        |
| `(List T)`              | A list of values of sort `T`                                                           |
| `(Option T)`            | Either a `T` or `none`, for operations that return `std::optional`                     |
| `Function`              | An operation, a defined function or a `lambda`                                         |
| `Term`                  | A node of the run's lazy formula; see [The `lazy:` namespace](@ref the-lazy-namespace) |

### Checking {#checking}

Checking is static where the sorts are known and dynamic where they are not.

- **Before the script runs:** the language version, syntax, unknown names, unknown keywords and hints, the number of
  arguments in a call of a named operation or function, a name that is reserved, and the declared inputs and
  parameters against the ones given.
- **Before the script runs, where sorts are known:** every operation in the `nfa:` and `nft:` namespaces has fixed
  sorts, so `(nfa:determinize t)` with `t` declared as `Nft` is rejected without running anything.
- **When the script runs:** the kind of a loaded file, operations in the `automaton:` namespace, function parameters
  without a sort, and every call of a function that arrived as a value.
- **As a report:** whether the script is guaranteed to terminate; see [Termination](@ref termination).

`(as value Sort)` narrows a value and fails at that point if the value is of another sort:

```scheme
(define a (as (automaton:load $1) Nfa)) ; error here if the file holds a transducer
(nft:determinize a)                     ; rejected before the script runs: `a` is an Nfa from here on
```

## Definitions, functions and conditionals {#definitions-functions-and-conditionals}

```scheme
(define alphabet-size 4)                          ; a value
(define product Nfa (nfa:intersection lhs rhs))   ; a value with a checked sort

(define (image (transducer Nft) (language Nfa))   ; a function; parameter sorts are optional
  (nft:to-nfa (nft:apply transducer language)))

(define deterministic                             ; a value chosen by a condition:
  (if (nfa:deterministic? product) product (nfa:determinize product)))   ; only one branch runs
```

- `define` binds a name once; binding it again in the same scope is an error. `push` opens a scope, `pop` closes it
  and drops its definitions and option changes, and `reset` returns to the initial state.
- The sort in `(define name Sort value)` narrows the value as `as` does: a value that cannot be of that sort is an
  error before the script runs, and a value whose sort is known only when the script runs is checked then.
- `let` binds names for the extent of its body, each binding seeing the previous ones.
- `if` and `cond` evaluate only the chosen branch. `and` and `or` stop at the first operand that decides the result.
  This matters because it decides which library calls are made.
- `do` evaluates its expressions in order and returns the last value.

A complete script that uses each of these forms:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define language (nfa:create-from-regex "(ab)*"))
(define size Int (nfa:number-of-states (nfa:minimize language)))   ; 2, checked to be an Int

; cond: the first condition that holds picks the branch, and else catches the rest
(echo (cond ((= size 0) "no states")
            ((< size 4) "small")                ; printed, as the minimal automaton has 2 states
            (else "large")))

; let: the names are visible only in its body, and each binding sees the ones before it
(check "abab-accepted"
  (let ((minimal (nfa:minimize language))
        (abab (word "a" "b" "a" "b")))
    (nfa:in-language? minimal abab)))           ; abab-accepted: true

; do: evaluates its expressions in order and returns the last value
(define states (do (echo "minimizing") (nfa:number-of-states (nfa:minimize language))))
(print states)                                  ; 2

; or: stops at the first true operand, so nfa:universal? runs only for a language that is not empty
(check "empty-or-universal" (or (nfa:language-empty? language) (nfa:universal? language)))   ; false
```

### Functions {#functions}

A function can call every function that is defined before it. A simple function is a named expression with
parameters:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

; one parameter, with its sort: the number of states of the minimal automaton of a language
(define (minimal-size (language Nfa))
  (nfa:number-of-states (nfa:minimize language)))

; two parameters, without sorts: whether two regular expressions denote the same language
(define (same-language? left right)
  (nfa:equivalent? (nfa:create-from-regex left) (nfa:create-from-regex right)))

(print (minimal-size (nfa:create-from-regex "a*b")))     ; 2
(check "star-of-star" (same-language? "(a*)*" "a*"))     ; star-of-star: true
(check "not-the-same" (same-language? "(ab)*" "a*b*"))   ; not-the-same: false
```

A function is also an ordinary value: it can be defined inside another function, passed as an argument, returned as
a result, and stored in a list. Given two automata `lhs` and `rhs`:

```scheme
(define (minimal-image (transducer Nft) (language Nfa))
  (define (normalize (automaton Nfa))             ; visible only inside minimal-image
    (-> automaton nfa:determinize nfa:minimize))
  (normalize (nft:to-nfa (nft:apply transducer language))))   ; the minimal automaton of the image

(define (included-with (algorithm String))        ; returns a function of two automata,
  (lambda (smaller bigger) (nfa:included? smaller bigger :algorithm algorithm)))   ; which keeps `algorithm`

; a list of two functions, and one verdict for each of them: the list true true when lhs is included in rhs
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
  [Loops and recursion](@ref loops-and-recursion) describes the option that lets a function call itself.
- **Own name.** Using the name of a function inside its own body is an error without that option, even when an outer
  scope defines the same name. Turning the option on therefore never changes what an accepted script means.
- **Hiding.** An inner scope may define a name that an outer scope already has. The inner one is used inside that
  scope.

### Pipelines {#pipelines}

`->` passes a value through a sequence of steps. A step that is a name is called with the value; a step that is an
application gets the value as its first argument.

```scheme
; load the file of the first input, determinize and minimize it, and print it in the .mata format
(-> $1 nfa:load nfa:determinize nfa:minimize nfa:print-to-mata print)

; intersect lhs with rhs, trim the product, and write it to a file
(-> lhs (nfa:intersection rhs) nfa:trim (write "product.mata"))
```

The second pipeline is `(write (nfa:trim (nfa:intersection lhs rhs)) "product.mata")`.

A step may also take keyword arguments, which follow the value:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

(-> (nfa:create-from-regex "(a|b)*abb")      ; the words over a and b that end in abb
    nfa:determinize                          ; a deterministic automaton of them
    nfa:minimize                             ; the minimal one
    (nfa:print-to-regex :mode "readable")    ; is (nfa:print-to-regex minimal :mode "readable")
    echo)                                    ; prints (b*a)+bb
```

### Lists {#lists}

`(list a b c)` builds a list. The list functions are `list:length`, `list:nth` (from 0), `list:take`, `list:drop`,
`list:range`, `list:map`, `list:filter`, `list:reduce` and `list:fold`:

```scheme
(list:map nfa:determinize automata)                                ; determinize each automaton
(list:filter (lambda (a) (not (nfa:language-empty? a))) automata)  ; keep those with a non-empty language
(list:reduce nfa:intersection automata)                    ; intersect all of them; error on an empty list
(list:fold nfa:union-nondeterministic empty-language automata) ; unite all of them, starting from empty-language
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

A complete script that applies each list function to automata built from a list of regular expressions:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define patterns (list "a*" "(ab)*" "a|b" "b+"))
(define languages (list:map nfa:create-from-regex patterns))   ; one automaton for each pattern
(print (list:length languages))                                ; 4
(echo (to-string (list:take (list:drop patterns 1) 2)))        ; ((ab)* a|b): the second and third

; the number of states of each minimal automaton
(define sizes
  (list:map (lambda (language) (nfa:number-of-states (nfa:minimize language))) languages))
(echo (to-string sizes))                                       ; (1 2 2 2)
(print (list:fold + 0 sizes))                                  ; 7: their sum

; the languages that contain the word a
(define with-a (list:filter (lambda (language) (nfa:in-language? language (word "a"))) languages))
(print (list:length with-a))                                   ; 2: those of a* and a|b

; the union of all of them
(define all (list:reduce nfa:union-nondeterministic languages))
(check "union-has-bbb" (nfa:in-language? all (word "b" "b" "b")))   ; union-has-bbb: true
```

### Loops and recursion {#loops-and-recursion}

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
  The `mata` interpreter implements it.

Everything else on this page works without the option, including functions that call other functions and functions
used as values.

Loops and recursion are meant for a computation whose number of rounds depends on the automata computed so far, such
as a fixpoint. A number of rounds that is known in advance needs only [`list:range`](@ref lists).

`loop` binds names to initial values and evaluates its body. `(recur value …)` starts the body again with the names
bound to the new values. When the body finishes without a `recur`, the loop ends and the value of the body is the
value of the loop. A loop that counts its rounds:

```scheme
(set-option :loops-and-recursion true)
(set-option :alphabet "bytes")

; the smallest n for which a^n, the n-th power of the language {a}, contains the word aaaa
(define a (nfa:create-from-regex "a"))
(echo (loop ((power a) (n 1))                       ; the first round: power is a and n is 1
  (if (nfa:in-language? power (word "a" "a" "a" "a"))
      (concat "a^" (to-string n) " contains aaaa")  ; no recur: the loop ends with this value
      (recur (nfa:concatenate power a) (+ n 1)))))  ; the next round, with power . a and n + 1
```

It prints `a^4 contains aaaa`. A loop that computes a fixpoint over the input automata:

```scheme
(set-option :loops-and-recursion true)

(declare-input initial Nfa)
(declare-input step Nft)
(declare-input bad Nfa)

; the configurations reachable from `initial` under the transition relation `step`
(define reachable
  (loop ((reached initial))
    (define next                                    ; those reached so far, and their successors
      (nfa:union-nondeterministic reached (nft:to-nfa (nft:apply step reached))))
    (if (nfa:included? next reached) reached (recur next))))   ; stop when no new one is reached

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

A function that calls itself, with a parameter that counts down:

```scheme
(set-option :loops-and-recursion true)
(set-option :alphabet "bytes")

(define (power (language Nfa) (n Int))   ; the language concatenated with itself n times, for n >= 1
  (if (= n 1) language (nfa:concatenate language (power language (- n 1)))))

(check "power-3" (nfa:equivalent? (power (nfa:create-from-regex "ab") 3)
                                  (nfa:create-from-regex "ababab")))    ; power-3: true
```

Two functions still cannot call each other by name, because the second one is not visible in the first.

### Termination {#termination}

The checker reports, for each script, whether it is guaranteed to terminate, provided that the library operations it
calls do. This is a report, not an error: the script runs either way.

The guarantee is given when the script has no `loop` and no function that calls itself, and every function in it is
used in these ways only:

- it is bound to a name,
- it is called, or
- it is given to a `list:` function as the function to apply.

A script gets no guarantee when it passes a function to a defined function or to a `lambda`, returns a function from
one, or puts a function into a list. A function can then be passed to itself, as in
`((lambda (f) (f f)) (lambda (f) (f f)))`, which runs forever without any loop. The script in
[Functions](@ref functions) is such a script: `included-with` returns a function, and `relations` lists them.

The checker is conservative, so a script without the guarantee may still terminate.

## Inputs {#inputs}

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
  `(set-option :alphabet "bytes")` before a declaration applies to the automaton it loads, and a `set-info` value
  that writes a file before the first declaration can create an input it loads.
- All declared inputs are loaded before the first other form runs. A file whose kind does not match the declared sort
  is an error at that point. Declaring an input as `Automaton` accepts either kind.
- When a script declares inputs, every input given on the command line must be taken by a declaration; an input left
  over is an error, so a mistyped command line is not ignored.
- The loading done by declarations is part of a [timing record](@ref timing) only under
  `:time-operations`, as `parsing`.

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

A script that takes inputs and parameters, saved as `compare.matasc`:

```scheme
; time the inclusion of two automata several times with one algorithm
(mata-script "0.1.0")
(declare-param algorithm String "antichains")  ; --param algorithm=naive, or "antichains" without one
(declare-param rounds Int 3)                   ; --param rounds=…, or 3 without one
(declare-input smaller Nfa)                    ; --input smaller=…, or else the first positional input
(declare-input bigger Nfa)                     ; --input bigger=…, or else the next positional input

(define (measure round)                        ; one timed check, labelled naive-0, naive-1, …
  (time (concat algorithm "-" (to-string round))
    (nfa:included? smaller bigger :algorithm algorithm)))
(define verdicts (list:map measure (list:range 0 rounds)))
(check "included" (list:nth verdicts 0))
```

```console
$ mata script run --param algorithm=naive --param rounds=2 compare.matasc small.mata big.mata
naive-0: 0.000049
naive-1: 0.000006
included: true
```

`mata script run --input bigger=big.mata compare.matasc small.mata` gives `bigger` by name and `smaller` by
position. A third positional input is an error, as no declaration takes it.

**Paths written in the script** are loaded with the same operations: `(nfa:load "automata/a.mata")`. A relative path
is resolved against the directory of the script, so a script and the automata beside it move together. A path given on
the command line is resolved against the current directory, as for any other command: when the script is in a file
with a directory part, a relative one is made absolute before the script sees it, so `$1` and `inputs` hold the
absolute path.

**Literals** make a script complete without any file:

```scheme
; from an RE2 regular expression; its symbols are bytes
(nfa:create-from-regex "a(b|c)*")

; from .mata text; a string may span several lines
(nfa:parse "@NFA-explicit
%Initial q0
%Final q1
q0 a q1")

; from states and transitions written in the script; epsilon is an ε-transition
(nfa:literal :initial (q0) :final (q1)
             :transitions ((q0 a q1) (q1 b q1) (q0 epsilon q1)))

; a transducer of two levels: q0 is on level 0 and q1 on level 1, so it maps a to b, letter by letter
(nft:literal :levels 2 :initial (q0) :final (q0)
             :states ((q0 0) (q1 1))
             :transitions ((q0 a q1) (q1 b q0)))
```

In `nfa:literal` and `nft:literal`, states and symbols are names, as in the [`.mata` format](@ref automata-format); they
are not evaluated. `epsilon` in the symbol position is an ε-transition. `:states` of `nft:literal` gives the level of
each state that is not on level 0.

### What loading does {#what-loading-does}

`nfa:load`, `nft:load` and `automaton:load` are the one place where a script operation is more than one library call.
Loading parses the file, mintermizes it if its alphabet is a bitvector alphabet, and constructs the automaton over the
[script alphabet](@ref alphabets). This is what `load_automaton()` in `tests-integration/src/utils/` does today. The
library function `parse_from_mata()` is not used, because it constructs over `IntAlphabet`, which accepts only numbers
as symbol names.

`nfa:parse`, `nft:parse` and `automaton:parse` do the same with `.mata` text given as a string instead of a file.

Given a list of paths, `load` returns a list. Inputs with a bitvector alphabet are then mintermized together, so that
they share their minterms; declarations load this way. Automata mintermized separately do not share minterms and
must not be combined. The library mintermizes NFAs and AFAs only, so a transducer with a bitvector alphabet is an
error.

The steps are available separately as `intermediate:parse-from-mata`, `intermediate:mintermize` and `nfa:construct` /
`nft:construct`.

```scheme
(mata-script "0.1.0" :requires (intermediate))
(define text "@NFA-explicit
%Initial q0
%Final q1
q0 a q1
q1 b q1")

(define parsed (intermediate:parse-from-mata text))   ; one intermediate automaton for each section
(define nfa (nfa:construct (list:nth parsed 0)))      ; constructed over the script alphabet
(check "same-as-parse" (nfa:equivalent? nfa (nfa:parse text)))   ; same-as-parse: true
```

## Alphabets {#alphabets}

A script has one **script alphabet**. Every load, literal, and word made with `(word "a" "b")` or `(symbol "a")`
translates symbol names through it, so a name means the same symbol everywhere in the script.

| `(set-option :alphabet …)` | Script alphabet                                                                     |
| -------------------------- | ----------------------------------------------------------------------------------- |
| `"on-the-fly"` (default)   | An empty `OnTheFlyAlphabet`; a name gets the next free number when it is first seen |
| `"bytes"`                  | An `OnTheFlyAlphabet` with the byte registration and naming rules below             |

With `"bytes"`, every byte is registered under its own value first, as in the website playground: a printable ASCII
character other than the space and the backslash is named by itself, and any other byte by `\xHH`.

`nfa:create-from-regex` produces automata whose symbols are byte values. Under `"on-the-fly"`, the name `a` in a
`.mata` file is the symbol 0 if it is seen first, which is not the byte `a`. A script that combines automata from
regular expressions with automata from files or literals must therefore select `"bytes"`. The option can be set only
before the alphabet is first used.

`(script-alphabet)` returns the script alphabet as a value, for operations that take one. `nfa:create-alphabet`
builds the alphabet of the symbols that occur in the given automata.

Under the default alphabet, a name gets its symbol when it is first seen:

```scheme
(mata-script "0.1.0")                       ; no :alphabet option: "on-the-fly"
(define ab (nfa:literal :initial (q0) :final (q2) :transitions ((q0 a q1) (q1 b q2))))
(check "accepts-ab" (nfa:in-language? ab (word "a" "b")))              ; accepts-ab: true
(echo (to-string (alphabet:get-alphabet-symbols (script-alphabet))))   ; (a b): the names seen so far
```

A literal and a regular expression that spell the same words denote the same language only under `"bytes"`:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")              ; without this line, same-language prints false

(define from-regex (nfa:create-from-regex "ab"))                  ; reads the bytes a and b
(define from-literal                                              ; reads the names a and b, translated
  (nfa:literal :initial (q0) :final (q2)                          ; through the script alphabet
               :transitions ((q0 a q1) (q1 b q2))))
(check "same-language" (nfa:equivalent? from-regex from-literal))  ; same-language: true
```

Under `"on-the-fly"`, the names `a` and `b` of the literal are the symbols 0 and 1, which are not the bytes that the
regular expression reads.

## Operations {#operations}

### Names {#names}

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
| `automaton:`    | No library counterpart; see [below](@ref the-automaton-namespace)        |
| `strings:`      | `mata::applications::strings` and its `seg_nfa` and `replace` namespaces |
| `alphabet:`     | The member functions of `mata::Alphabet`                                 |
| `intermediate:` | `mata::parser`, `mata::IntermediateAut` and `mata::Mintermization`       |
| `list:`         | No library counterpart                                                   |
| `lazy:`         | `mata::lazy` of the Rust bindings; see [below](@ref the-lazy-namespace)  |

### Arguments {#arguments}

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
  [`witness`](@ref witnesses).

A hint value is an ordinary expression, so one script can compare algorithms:

```scheme
; one timed inclusion check per algorithm; the label of each record is the algorithm
(list:map (lambda (algorithm)
            (time algorithm (nfa:included? lhs rhs :algorithm algorithm)))
          (list "naive" "antichains"))
```

The default of a hint can be changed for the rest of the scope. A hint written in the application wins over the
changed default, which wins over the default of the library:

```scheme
(set-option :default (nfa:included? :algorithm "naive"))   ; nfa:included? without :algorithm is now naive
```

A complete script with an optional parameter, hints, and a changed default inside a scope. The trace shows the hints
of each call on standard error:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")
(define lhs (nfa:create-from-regex "(ab)*"))
(define rhs (nfa:create-from-regex "(a|b)*"))

; an optional parameter of the library function: join the two automata with an ε-transition
(define joined (nfa:concatenate lhs rhs :use-epsilon true))
(check "joined-accepts-abba" (nfa:in-language? joined (word "a" "b" "b" "a")))   ; true

; a hint, an entry of the ParameterMap: reduce the automaton by simulation
(print (nfa:number-of-states (nfa:reduce joined :algorithm "simulation")))       ; 3

(push)                                                      ; a scope for the changed default
(set-option :default (nfa:included? :algorithm "naive"))
(set-option :trace true)
(check "included-naive" (nfa:included? lhs rhs))            ; trace: nfa:included? :algorithm "naive"
(check "included-antichains"
  (nfa:included? lhs rhs :algorithm "antichains"))          ; the hint in the application wins
(pop)                                                       ; drops the default and the trace
(check "included-default" (nfa:included? lhs rhs))          ; the library default again, untraced
```

### One call per application {#one-call-per-application}

Each application is one library call, with these exceptions:

- `load` and `parse`, described in [What loading does](@ref what-loading-does).
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
- A `lazy:` builder makes no call at all; the query that checks its term is the call. See
  [The `lazy:` namespace](@ref the-lazy-namespace).

### NFA operations {#nfa-operations}

| Script                                  | Library                                                                  |
| --------------------------------------- | ------------------------------------------------------------------------ |
| `nfa:union-nondeterministic`            | `union_nondet`                                                           |
| `nfa:union-deterministic-complete`      | `union_det_complete`                                                     |
| `nfa:intersection`                      | `intersection`                                                           |
| `nfa:product`                           | `product`                                                                |
| `nfa:language-difference`               | `lang_difference`                                                        |
| `nfa:concatenate`                       | `concatenate`                                                            |
| `nfa:concatenate-nth-power`             | `concatenate_nth_power`                                                  |
| `nfa:complement`                        | `complement`                                                             |
| `nfa:left-quotient`                     | `left_quotient`                                                          |
| `nfa:right-quotient`                    | `right_quotient`                                                         |
| `nfa:determinize`                       | `determinize`                                                            |
| `nfa:as-dfa`                            | `as_dfa`                                                                 |
| `nfa:minimize`                          | `minimize`                                                               |
| `nfa:reduce`                            | `reduce`                                                                 |
| `nfa:revert`                            | `revert`                                                                 |
| `nfa:remove-epsilon`                    | `remove_epsilon`                                                         |
| `nfa:trim`                              | `trim`                                                                   |
| `nfa:make-complete`                     | `Nfa::make_complete`                                                     |
| `nfa:decode-utf8`                       | `Nfa::decode_utf8`                                                       |
| `nfa:get-one-letter-automaton`          | `Nfa::get_one_letter_aut`                                                |
| `nfa:collapse-by-length`                | `collapse_by_length`                                                     |
| `nfa:language-empty?`                   | `Nfa::is_lang_empty`                                                     |
| `nfa:intersection-empty?`               | `is_intersection_empty`                                                  |
| `nfa:included?`                         | `is_included`                                                            |
| `nfa:equivalent?`                       | `are_equivalent`                                                         |
| `nfa:universal?`                        | `Nfa::is_universal`                                                      |
| `nfa:in-language?`                      | `Nfa::is_in_lang`                                                        |
| `nfa:in-language-prefix?`               | `Nfa::is_in_lang_prefix`                                                 |
| `nfa:deterministic?`                    | `Nfa::is_deterministic`                                                  |
| `nfa:complete?`                         | `is_complete`                                                            |
| `nfa:minimal?`                          | `is_minimal`                                                             |
| `nfa:flat?`                             | `is_flat`                                                                |
| `nfa:identical?`                        | `is_identical`                                                           |
| `nfa:get-word`                          | `Nfa::get_word`                                                          |
| `nfa:get-shortest-word`                 | `get_shortest_word`                                                      |
| `nfa:get-words`                         | `get_words`                                                              |
| `nfa:get-word-from-complement`          | `Nfa::get_word_from_complement`                                          |
| `nfa:get-word-from-language-difference` | `get_word_from_lang_difference`                                          |
| `nfa:number-of-states`                  | `Nfa::num_of_states`                                                     |
| `nfa:number-of-transitions`             | `Delta::num_of_transitions`                                              |
| `nfa:create-from-regex`                 | `builder::create_from_regex`; `Re2Options` members are keywords          |
| `nfa:create-single-word-nfa`            | `builder::create_single_word_nfa`                                        |
| `nfa:create-empty-string-nfa`           | `builder::create_empty_string_nfa`                                       |
| `nfa:create-sigma-star-nfa`             | `builder::create_sigma_star_nfa`                                         |
| `nfa:create-random-nfa-tabakov-vardi`   | `builder::create_random_nfa_tabakov_vardi`                               |
| `nfa:construct`                         | `builder::construct`                                                     |
| `nfa:create-alphabet`                   | `create_alphabet`                                                        |
| `nfa:encode-word`                       | `encode_word`                                                            |
| `nfa:print-to-mata`                     | `Nfa::print_to_mata`; returns a string                                   |
| `nfa:print-to-dot`                      | `Nfa::print_to_dot`; returns a string                                    |
| `nfa:print-to-regex`                    | `Nfa::print_to_regex`; returns a string; `:mode` is `fast` or `readable` |
| `nfa:load`, `nfa:parse`, `nfa:literal`  | No single library counterpart                                            |

The operations on one language, with the results as comments:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define even-as (nfa:create-from-regex "(b*ab*a)*b*"))   ; words over a and b with an even number of a
(define rest (nfa:complement even-as))                   ; all other words over all 256 bytes
(check "a-is-in-rest" (nfa:in-language? rest (word "a")))           ; true
(check "aa-is-in-rest" (nfa:in-language? rest (word "a" "a")))      ; false
(check "disjoint" (nfa:intersection-empty? even-as rest))           ; true
(echo (to-string (nfa:get-shortest-word rest)))   ; \x00: the complement is over the script alphabet
```

Several languages combined, compared and searched for words:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define identifiers (nfa:create-from-regex "[a-z][a-z0-9]*"))
(define keywords (nfa:create-from-regex "if|in|let"))
(define names (nfa:language-difference identifiers keywords))   ; the identifiers that are no keyword

(check "keywords-are-identifiers" (nfa:included? keywords identifiers))    ; true
(check "let-is-a-name" (nfa:in-language? names (word "l" "e" "t")))        ; false
(check "lets-is-a-name" (nfa:in-language? names (word "l" "e" "t" "s")))   ; true

; a word of the identifiers that is not a name, and a name
(echo (to-string (nfa:get-word-from-language-difference identifiers names)))   ; l e t
(echo (to-string (nfa:get-word names)))                                        ; a

; the names of two letters that start with i: the 36 pairs minus if and in
(print (list:length (nfa:get-words (nfa:intersection names (nfa:create-from-regex "i[a-z0-9]")) 2)))   ; 34

; the minimal automaton of the keywords and of their reversal, as regular expressions
(echo (nfa:print-to-regex (nfa:minimize keywords) :mode "readable"))              ; let|i[fn]
(check "tel" (nfa:in-language? (nfa:revert keywords) (word "t" "e" "l")))         ; true
```

### NFT operations {#nft-operations}

| Script                                 | Library                                           |
| -------------------------------------- | ------------------------------------------------- |
| `nft:union-nondeterministic`           | `union_nondet`                                    |
| `nft:intersection`                     | `intersection`                                    |
| `nft:concatenate`                      | `concatenate`                                     |
| `nft:concatenate-nth-power`            | `concatenate_nth_power`                           |
| `nft:complement`                       | `complement`                                      |
| `nft:compose`                          | `compose`                                         |
| `nft:apply`                            | `Nft::apply`, to an `Nfa` or to a `Word`          |
| `nft:project-out`                      | `project_out`                                     |
| `nft:project-to`                       | `project_to`                                      |
| `nft:insert-level`                     | `insert_level`                                    |
| `nft:insert-levels`                    | `insert_levels`                                   |
| `nft:invert-levels`                    | `invert_levels`                                   |
| `nft:unwind-jumps`                     | `Nft::unwind_jumps`                               |
| `nft:determinize`                      | `determinize`                                     |
| `nft:reduce`                           | `reduce`                                          |
| `nft:revert`                           | `revert`                                          |
| `nft:remove-epsilon`                   | `remove_epsilon`                                  |
| `nft:trim`                             | `Nft::trim`                                       |
| `nft:language-empty?`                  | `Nft::is_lang_empty`                              |
| `nft:included?`                        | `is_included`                                     |
| `nft:equivalent?`                      | `are_equivalent`                                  |
| `nft:universal?`                       | `Nft::is_universal`                               |
| `nft:in-language?`                     | `Nft::is_in_lang`                                 |
| `nft:in-language-by-levels?`           | `Nft::is_in_lang_by_levels`                       |
| `nft:deterministic?`                   | `Nft::is_deterministic`                           |
| `nft:complete?`                        | `is_complete`                                     |
| `nft:identical?`                       | `is_identical`                                    |
| `nft:contains-jump-transitions?`       | `Nft::contains_jump_transitions`                  |
| `nft:has-epsilon-cycle?`               | `has_epsilon_cycle`                               |
| `nft:to-nfa`                           | `Nft::to_nfa_copy` or `to_nfa_move`               |
| `nft:to-nfa-update`                    | `Nft::to_nfa_update_copy` or `to_nfa_update_move` |
| `nft:from-nfa-with-levels-zero`        | `builder::from_nfa_with_levels_zero`              |
| `nft:from-nfa-with-levels-advancing`   | `builder::from_nfa_with_levels_advancing`         |
| `nft:create-single-word-nft`           | `builder::create_single_word_nft`                 |
| `nft:create-empty-string-nft`          | `builder::create_empty_string_nft`                |
| `nft:create-sigma-star-nft`            | `builder::create_sigma_star_nft`                  |
| `nft:construct`                        | `builder::construct`                              |
| `nft:number-of-states`                 | `Nft::num_of_states`                              |
| `nft:print-to-mata`                    | `Nft::print_to_mata`                              |
| `nft:print-to-dot`                     | `Nft::print_to_dot`                               |
| `nft:load`, `nft:parse`, `nft:literal` | No single library counterpart                     |

There is no `nft:minimize`: the library declares `nft::minimize` only under `MATA_NFT_NOT_IMPLEMENTED`.

A transducer reads a pair of words, one on each level. `nft:apply` gives the image of a word or a language, and
`nft:to-nfa` makes the image, a transducer of one level, an `Nfa`:

```scheme
(mata-script "0.1.0" :requires (transducers))
(set-option :alphabet "bytes")

; replace every a by b, and keep b: level-0 states read the input, level-1 states write the output
(define a-to-b
  (nft:literal :levels 2 :initial (q) :final (q)
               :states ((on-a 1) (on-b 1))
               :transitions ((q a on-a) (on-a b q)
                             (q b on-b) (on-b b q))))

(define image (nft:to-nfa (nft:apply a-to-b (nfa:create-from-regex "a*ba"))))
(echo (nfa:print-to-regex (nfa:minimize image) :mode "readable"))       ; bb+
(check "ab-to-bb" (nft:in-language-by-levels? a-to-b (list (word "a" "b") (word "b" "b"))))   ; true
(check "interleaved" (nft:in-language? a-to-b (word "a" "b" "b" "b")))  ; the same pair, level by level
```

Transducers compose, invert and project:

```scheme
(mata-script "0.1.0" :requires (transducers))
(set-option :alphabet "bytes")

; a Caesar shift on a, b and c: a to b, b to c, c to a
(define shift
  (nft:literal :levels 2 :initial (q) :final (q)
               :states ((read-a 1) (read-b 1) (read-c 1))
               :transitions ((q a read-a) (read-a b q)
                             (q b read-b) (read-b c q)
                             (q c read-c) (read-c a q))))

(define shift-twice (nft:compose shift shift))
(check "inverse-is-shift-twice" (nft:equivalent? (nft:invert-levels shift) shift-twice))   ; true

; three shifts give every word back
(define words (nfa:create-from-regex "[abc]*"))
(check "identity" (nfa:equivalent? (nft:to-nfa (nft:apply (nft:compose shift shift shift) words)) words))

; the domain is level 0 kept, the codomain level 0 projected out; both are every word over a, b and c
(define domain (nft:to-nfa (nft:project-to shift (list 0))))
(define codomain (nft:to-nfa (nft:project-out shift (list 0))))
(echo (nfa:print-to-regex (nfa:minimize domain) :mode "readable"))       ; [a-c]*
(echo (nfa:print-to-regex (nfa:minimize codomain) :mode "readable"))     ; [a-c]*

(echo (nfa:print-to-regex (nft:to-nfa (nft:apply shift-twice (word "c" "a" "b")))))   ; bca
```

### The `automaton:` namespace {#the-automaton-namespace}

For every operation that exists under the same name in both `nfa:` and `nft:`, there is an operation of that name in
`automaton:`, for example `automaton:intersection`, `automaton:determinize` and `automaton:included?`. It accepts an
`Automaton`, an `Nfa` or an `Nft`, and calls the `nfa:` or the `nft:` operation according to what the operands are
when the script runs. Operands of different kinds are an error; convert explicitly with
`nft:from-nfa-with-levels-zero` or `nft:to-nfa`.

`automaton:load` and `automaton:parse` accept either kind and return an `Automaton`.

```scheme
(mata-script "0.1.0" :requires (automaton))
(declare-input given Automaton)                        ; a file with an NFA or with a transducer

(define smaller (automaton:determinize (automaton:trim given)))   ; nfa: or nft: operations, by the kind
(echo (concat "states: " (to-string (automaton:number-of-states given))
              " -> " (to-string (automaton:number-of-states smaller))))
(check "same-language" (automaton:equivalent? given smaller))     ; true for either kind
```

### The `lazy:` namespace {#the-lazy-namespace}

The `lazy:` operations decide properties of Boolean and relational combinations of automata and transducers without
building the combination: products pair states and complements build subsets on the fly, and a query explores only
the macrostates it needs. They run `mata::lazy`, the `mata-lazy` crate of the Rust bindings, which has no C++
counterpart. A script that uses them says so with `:requires (lazy)`.

Builders combine values of the sort `Term` and do no automata work. A query checks one term, which is one call of the
library:

```scheme
(mata-script "0.1.0" :requires (lazy))
(set-option :alphabet "bytes")
(define a (lazy:leaf (nfa:create-from-regex "a(a|b)*")))         ; words that start with a
(define b (lazy:leaf (nfa:create-from-regex "(a|b)*b")))         ; words that end in b
(check (lazy:included? a b))                                     ; false
(print (lazy:witness (lazy:intersection a (lazy:complement b))))  ; a: starts with a, does not end in b
```

| Builder                                           | `mata::lazy`                | Arities                     |
| ------------------------------------------------- | --------------------------- | --------------------------- |
| `(lazy:leaf automaton :alphabet "x y")`           | `leaf`, `leaf_over`         | `Nfa` → 1, `Nft` → levels   |
| `(lazy:union t u …)`, `(lazy:intersection t u …)` | `union`, `intersection`     | n, n → n, folded from left  |
| `(lazy:complement t)`                             | `complement`                | n → n                       |
| `(lazy:identity t)`                               | `identity`                  | 1 → 2                       |
| `(lazy:prefixes t)`                               | `prefixes`                  | n → n                       |
| `(lazy:pad-closure t (symbol "x") …)`             | `pad_closure`               | n → n, one symbol per tape  |
| `(lazy:pad-closure t)`                            | `pad_closure`               | n → n, repeat the last step |
| `(lazy:project t (list 2 0))`                     | `project`                   | n → the tapes, in its order |
| `(lazy:compose r s)`                              | `compose`                   | 2, 2 → 2                    |
| `(lazy:post-image l r)`, `(lazy:pre-image l r)`   | `post_image`, `pre_image`   | 1, 2 → 1                    |
| `(lazy:explicit t :max-states n)`                 | `explicit`, `explicit_with` | n → n                       |

| Query                    | `mata::lazy`                 | Value                                                          |
| ------------------------ | ---------------------------- | -------------------------------------------------------------- |
| `(lazy:empty? t)`        | `check(t)`                   | `Bool`: whether `t` is empty; false if a bound is inconclusive |
| `(lazy:empty-within? t)` | `check(t)`                   | `Bool`: no witness within `:max-length`                        |
| `(lazy:included? t u)`   | `check(t ∩ ¬u)`              | `Bool`                                                         |
| `(lazy:equivalent? t u)` | `check((t ∩ ¬u) ∪ (u ∩ ¬t))` | `Bool`                                                         |
| `(lazy:witness t)`       | `check(t)`                   | A tuple of `t`, `none` if `t` is empty                         |
| `(lazy:materialize t)`   | `materialize(t)` as an `Nft` | An `Nfa` for arity 1, else an `Nft`                            |
| `(lazy:materialize t)`   | `materialize(t)` as an `Nft` | An `Nfa` for arity 1, else an `Nft`                            |

- **Lock step.** A term of arity n denotes a relation over n-tuples of words, read in lock step: one step reads one
  symbol on every tape, and `epsilon` pads a shorter word. A language has arity 1, and a leaf of an `Nft` has one tape
  per level. This is exact for length-preserving relations, as in regular model checking. Mata's `nft:compose` and
  `nft:apply` align padding asynchronously, so on a transducer that pads they can give another relation than
  `lazy:compose` and the images.
- **Arities** are checked when the builder runs: `(lazy:intersection language relation)` stops the script with
  `intersection expects arity 1, found 2`. The checker knows only that both operands are terms.
- **Alphabets.** The complement is taken over the tuples of concrete symbols of the tapes' alphabets, so it holds no
  padding. A tape's alphabet holds the symbols that the leaves read on the tapes that must read the same symbols (both
  operands of an intersection, both tapes of an identity, …), the symbols of `:alphabet` of their `lazy:leaf`, and,
  for the term a query checks, the symbols of the query's `:alphabet`. `:alphabet` is symbol names separated by
  spaces, translated through the script alphabet. The same complement can therefore mean different things in
  different queries: `(lazy:complement (lazy:leaf (nfa:create-from-regex "(a|b)*")))` is empty, and not empty with
  `:alphabet "a b c"`.
- **One formula per run.** Every term of a run is a node of one formula, which only grows while the run lasts, so
  terms built for different queries combine. A value is imported as a leaf once for each `:alphabet` it is given, and
  `=` is true of two terms that are the same node, such as two leaves of one value.
- **Explicit nodes.** `lazy:explicit` builds its term as an automaton when a query starts and reads it like a leaf;
  later queries of the run reuse it. With `:max-states n` (an `Int`), a term that needs more than `n` states stays
  lazy.
- **Padding closures.** `lazy:prefixes` prefixes all tapes together, padding included. `lazy:pad-closure` accepts every
  word that can be padded into its operand. The symbols supply that padding, one per tape; `dont-care` accepts any one
  symbol of the tape's alphabet. With no symbols, the last step repeats forever (the empty word has no padding). For
  example, `(lazy:pad-closure (lazy:leaf ab0) (symbol "0"))` turns the language `ab0` into `ab0?`.
- **Limits.** A query takes `:time-limit` in milliseconds and `:max-states`, the number of macrostates it may hold,
  both `Int`s. A query that passes a limit is an error that stops the script, naming the limit:
  `the query holds more than 2 macrostates`.
- **Bounded search.** `:max-length n`, an `Int`, searches only words of at most `n` lock-step steps, padding included.
  Use `lazy:empty-within?` to ask whether no witness exists within that bound. `lazy:empty?` is about the full relation:
  it returns `false` both when it finds a witness and when a length bound leaves the search inconclusive. Likewise, a
  bounded `lazy:witness` returning `none` does not by itself prove that a relation is empty. Search for a shortest
  witness with `:shortest true`; without it the default depth-first search returns the first one it finds.
- **Witnesses.** `lazy:empty?`, `lazy:included?` and `lazy:equivalent?` are [witness](@ref witnesses) operations: the
  witness is a tuple of the checked term, so a tuple of `t` not in `u` for `lazy:included?`. A tuple of a language is
  a `Word`, and of a relation a list of one `Word` per tape.
- **Timing and tracing.** `:time-operations` and `:trace` report the queries, which are the calls; builders make none.
  `:optimize` neither remembers nor rewrites `lazy:` applications.

A complete script on a relation: one token passed between processes, letter by letter, as in regular model checking:

```scheme
(mata-script "0.1.0" :requires (lazy transducers))
(set-option :alphabet "bytes")

; one token t among processes n; a step passes the token to the right neighbour
(define step
  (nft:literal :levels 2 :initial (p) :final (q)
               :states ((p-n 1) (p-t 1) (give 1) (take 1) (q-n 1) (q-t 1))
               :transitions ((p n p-n) (p-n n p) (p t p-t) (p-t t p)    ; copy before the move
                             (p t give) (give n r) (r n take) (take t q) ; tn becomes nt
                             (q n q-n) (q-n n q) (q t q-t) (q-t t q))))  ; copy after the move
(define relation (lazy:leaf step))                                 ; arity 2: one tape per level
(define one-token (lazy:leaf (nfa:create-from-regex "n*tn*")))     ; arity 1

; the successors of one-token configurations still hold one token
(define successors (lazy:post-image one-token relation))
(check "keeps-one-token" (lazy:included? successors one-token :alphabet "n t"))   ; true

; a shortest configuration from which one step puts the token at the end
(print (lazy:witness (lazy:pre-image (lazy:leaf (nfa:create-from-regex "n*t")) relation)
                     :shortest true))                              ; t n

; a shortest pair of configurations two steps apart: one word per tape
(print (lazy:witness (lazy:compose relation relation) :shortest true))   ; t n n, then n n t

; no step leaves a configuration as it is: the relation meets the identity nowhere
(check "every-step-moves"
  (lazy:empty? (lazy:intersection relation (lazy:identity one-token)) :alphabet "n t"))   ; true

; the same kind of query, with its counters: post.explored, post.macrostates, …
(check "token-leaves-the-start"
  (lazy:stats "post"
    (lazy:empty? (lazy:intersection successors (lazy:leaf (nfa:create-from-regex "tn*"))))))

; the successors built as an automaton
(echo (nfa:print-to-regex (lazy:materialize successors) :mode "readable"))   ; n+tn*
```

#### Plans and policies {#plans-and-policies}

Plans change how a query evaluates a formula, never which relation it denotes. They can therefore change its cost or
which witness it returns; a query limit can still turn an answer into an error. Annotations live for the rest of the
run and do not add formula nodes:

```scheme
(define ab (lazy:intersection (lazy:leaf a) (lazy:leaf b)))
(define abc (lazy:intersection ab (lazy:leaf c)))
(lazy:annotate ab :eval "eager" :max-states 1000)
(lazy:annotate abc :order "swapped")
(check (lazy:empty? abc))                              ; annotates this and later queries
(check (lazy:empty? abc :policy "lazy"))              ; overrides them for this query only
```

- `:eval` is `"keep"`, `"lazy"`, or `"eager"`; `"keep"` leaves the formula's choice unchanged. `:max-states` is the
  explicit node's budget, at which it stays lazy, not the query's failure budget. It implies `"eager"` when `:eval` is
  omitted and cannot be combined with `"keep"` or `"lazy"`.
- `:order` is `"auto"` (the engine's choice), `"as-written"`, or `"swapped"`. Ordering applies to intersections and
  synchronized products, including `lazy:compose` and the images, not unions or leaves. `lazy:annotate` returns its
  term, without evaluating it.
- `(lazy:view t)` returns a `String` of JSON describing each term under `t`: its numeric id, kind, arity, children,
  fan-out, and leaf sizes and determinism. `(lazy:plan)` returns the current annotations as JSON in the format that
  `mata::lazy::Plan::to_json` emits.
- Every query accepts `:plan` with such a JSON string, so an external tool can derive a plan from `lazy:view` and
  return it to the script. Alternatively, `:policy` takes `"lazy"`, `"eager"`, `"heuristic"`, or `"portfolio"`. The
  heuristic eagerly builds selected small inner terms and keeps the root lazy; `:heuristic-max-states` sets its
  per-term bound (4096 by default). `"portfolio"` runs the three others at once, one thread each, takes the first
  answer, and stops the rest; the `plan` record of `lazy:stats` then tells which plan answered. Where threads cannot
  be started, as in the playground, the plans run one after another. `lazy:materialize` rejects it. `:plan` and
  `:policy` are mutually exclusive and override the run's annotations for that query.

`(lazy:stats label check)` evaluates `check`, an application of `lazy:empty?`, `lazy:empty-within?`, `lazy:witness`,
`lazy:included?` or `lazy:equivalent?`, returns its value, and prints one record per counter of that query,
`label.counter: number`, in the form of [timing](@ref timing) records:

| Counter                | Meaning                                                                   |
| ---------------------- | ------------------------------------------------------------------------- |
| `explored`             | Macrostates of the checked term whose successors were computed            |
| `pruned`               | Macrostates of the checked term skipped because a found one subsumes them |
| `macrostates`          | Macrostates created in all nodes                                          |
| `dropped`              | Cached successors dropped to stay within the cache budget                 |
| `explicit`             | Explicit nodes built as automata, not those past their `:max-states`      |
| `explicit-states`      | States of those automata                                                  |
| `explicit-transitions` | Transitions of those automata                                             |
| `plan`                 | Portfolios only: the plan that answered (0 lazy, 1 eager, 2 heuristic)    |

Not part of the language yet: synchronized products and composition on chosen tapes (`sync_product`, `compose_on`),
`lazy:universal?`, `lazy:in-language?`, the remaining options of `mata::lazy` (`:cache-budget`, `:antichain`,
`:subsumption`), and a direct command `mata lazy`.

### The `ws1s:` namespace {#the-ws1s-namespace}

`ws1s:valid?` decides whether a WS1S formula holds under every assignment, and `ws1s:satisfiable?` whether some
assignment satisfies it. Both parse the formula with `mata::logic::ws1s::Ws1s` and decide it lazily:

```scheme
(check "a tautology"                                     ; true for every x, y and z
  (ws1s:valid? "(x = y + 1 & y = z + 1) => x = z + 2"))  ; true
(check "has a model"                                     ; some position is 3
  (ws1s:satisfiable? "ex1 x: x = 3"))                    ; true
```

Quantifiers over positions and over finite sets:

```scheme
(mata-script "0.1.0")

; every position has a later one
(check "no-last-position" (ws1s:valid? "all1 x: ex1 y: x < y"))                       ; true

; no finite set holds 0 and is closed under the successor
(check "no-infinite-set"
  (ws1s:satisfiable? "ex2 X: 0 in X & all1 x: x in X => x + 1 in X"))                 ; false

; the subset relation is transitive; X, Y and Z are free, so this holds for all sets
(check "subset-is-transitive" (ws1s:valid? "X sub Y & Y sub Z => X sub Z"))           ; true

; a set that holds 0 and 4, steps by 2 below 4, and does not hold 1: for example {0, 2, 4}
(check "even-positions"
  (ws1s:satisfiable? "0 in X & 4 in X & (all1 x: x in X & x < 4 => x + 2 in X) & ~(1 in X)"))   ; true
```

The formula uses the MONA-like syntax of `mata::logic::ws1s`: first-order names start with a lowercase letter and
second-order ones with an uppercase letter. Both decisions accept `:time-limit`, `:max-states`, `:max-length`, and
`:shortest`, with the meanings above.

### Other namespaces {#other-namespaces}

| Script                                               | Library                                                   |
| ---------------------------------------------------- | --------------------------------------------------------- |
| `strings:get-shortest-words`                         | `get_shortest_words`                                      |
| `strings:get-longest-words`                          | `get_longest_words`                                       |
| `strings:get-words-of-lengths`                       | `get_words_of_lengths`                                    |
| `strings:get-word-lengths`                           | `get_word_lengths`                                        |
| `strings:get-accepted-symbols`                       | `get_accepted_symbols`                                    |
| `strings:language-epsilon?`                          | `is_lang_eps`                                             |
| `strings:get-segments`                               | `seg_nfa::Segmentation`, then `get_segments`              |
| `strings:reluctant-nfa`                              | `replace::reluctant_nfa`                                  |
| `strings:create-identity`                            | `replace::create_identity`                                |
| `strings:create-identity-with-single-symbol-replace` | `replace::create_identity_with_single_symbol_replace`     |
| `strings:replace-reluctant-regex`                    | `replace::replace_reluctant_regex`                        |
| `strings:replace-reluctant-literal`                  | `replace::replace_reluctant_literal`                      |
| `strings:replace-reluctant-single-symbol`            | `replace::replace_reluctant_single_symbol`                |
| `alphabet:get-alphabet-symbols`                      | `Alphabet::get_alphabet_symbols`                          |
| `alphabet:translate-symbol`                          | `translate_symb`                                          |
| `alphabet:reverse-translate-symbol`                  | `reverse_translate_symbol`                                |
| `intermediate:parse-from-mata`                       | `parser::parse_mf`, then `IntermediateAut::parse_from_mf` |
| `intermediate:mintermize`                            | `Mintermization::mintermize`, on one NFA or a list        |

`strings:get-segments` builds `seg_nfa::Segmentation` for the given epsilon symbols.

Noodlification (`noodlify`, `noodlify_for_equation`, `noodlify_for_transducer`) is specific to one string solver and
is not part of the language.

The string operations on a finite language, and a replacement transducer:

```scheme
(mata-script "0.1.0" :requires (strings transducers))
(set-option :alphabet "bytes")

(define language (nfa:create-from-regex "ab|ba|abc"))
(list:map (lambda (w) (echo (to-string w))) (strings:get-shortest-words language))   ; a b, then b a
(echo (to-string (strings:get-longest-words language)))                             ; (a b c): one word
(check "has-empty-word" (strings:language-epsilon? language))                       ; false

; replace cat by dog in every word: every occurrence, or only the first one
(define (replace mode)
  (strings:replace-reluctant-literal (word "c" "a" "t") (word "d" "o" "g") (script-alphabet)
                                     :replace-mode mode))
(define input (nfa:create-from-regex "cat(s|cat)"))
(define (rewrite transducer)
  (nfa:print-to-regex (nfa:minimize (nft:to-nfa (nft:apply transducer input))) :mode "readable"))
(echo (rewrite (replace "all")))       ; dog(s|dog)
(echo (rewrite (replace "single")))    ; dog(s|cat)
```

### Core functions {#core-functions}

`not`, `=`, `<`, `<=`, `+`, `-` on `Bool`, `Int`, `String` and `Word` as applicable; `(concat string…)`, which joins
strings; `(to-string value)`, which shows any value as one line, the way `print` would, with a list on one line as
`(1 (2 3) a)`, a line break inside a string as `\n`, and `…` for a list nested deeper than an interpreter chooses to
walk; `(string->int string)`; `none?`
and `get` on an `Option`; `list`; `(word "a" "b")` and `(symbol "a")`, which translate names through the script
alphabet; `(script-alphabet)`. `=` is also true of two lists of the same length whose elements are equal in this
sense, pairwise, so `(= (list 1 (list 2)) (list 1 (list 2)))` is `true`; an interpreter limits how deep a pair it
compares may nest.

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define n (+ 2 3))
(check "arithmetic" (= (- n 1) 4))                         ; true
(check "less" (< 2 n))                                     ; true
(check "strings" (= (concat "ab" "c") "abc"))              ; true
(print (string->int "42"))                                 ; 42
(echo (to-string (list 1 (list 2 3) "a")))                 ; (1 (2 3) a)
(check "lists" (= (list 1 (list 2)) (list 1 (list 2))))    ; true
(check "words" (= (word "a" "b") (word "a" "b")))          ; true

; an Option: nfa:get-word returns none for an empty language
(define found (nfa:get-word (nfa:create-from-regex "x+")))
(if (none? found) (echo "empty") (echo (to-string (get found))))              ; x
(define nothing (nfa:get-word (nfa:intersection (nfa:create-from-regex "a")
                                                (nfa:create-from-regex "b"))))
(echo (to-string nothing))                                                    ; none
```

## Reporting results {#reporting-results}

| Form                        | Effect                                                        | Value         |
| --------------------------- | ------------------------------------------------------------- | ------------- |
| `(check condition)`         | Prints `true` or `false`                                      | The `Bool`    |
| `(check "label" condition)` | Prints the record `label: true` or `label: false`             | The `Bool`    |
| `(assert condition)`        | Reports a false condition and its place; the script continues | The `Bool`    |
| `(print value)`             | Prints the value to standard output                           | The value     |
| `(write value path)`        | Writes the value to a file                                    | The value     |
| `(echo string)`             | Prints the string                                             | The string    |
| `(get-witness)`             | See [Witnesses](@ref witnesses)                               | `Option Word` |

`print` and `write` return their argument, so they can stand in the middle of a pipeline. An automaton is printed in
the `.mata` format; use `nfa:print-to-dot` to print a DOT graph instead, or `nfa:print-to-regex` to print an RE2
regular expression: `(print (nfa:print-to-regex a))`. `:mode "readable"` asks the regex writer for a shorter, polished
expression; the default `"fast"` is the machine pipeline output. When the script alphabet is non-empty the regex uses
its symbol names; every used symbol's name, or its number if it is unnamed, must be one character, and otherwise the
operation reports an error. Without an alphabet the writer uses Unicode scalar values, since a regex has one character
per symbol. The Latin1 `nfa:create-from-regex` reader reads back symbols through `0xFF` exactly. Larger scalar values
require a UTF-8 reader and decoding of its UTF-8 automaton.

In an interactive session, the value of every top-level expression is shown, with an automaton summarised by its
kind and size rather than printed in full. When a script is run, the value of a top-level expression is dropped.

Results and timing records go to standard output, each written as soon as it is printed, so a run stopped by a time
limit keeps what it printed; errors and the [trace](@ref options) go to standard error.

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")
(define language (nfa:create-from-regex "a+b"))

(check (nfa:universal? language))                             ; false, without a label
(check "has-ab" (nfa:in-language? language (word "a" "b")))   ; has-ab: true
(echo "the minimal automaton:")
(define minimal (print (nfa:minimize language)))              ; prints it in the .mata format, and names it
(write minimal "a-plus-b.mata")                               ; the same text, in a file beside the script
(assert (nfa:in-language? minimal (word "b")))                ; fails: reported on standard error
(echo "still running after a failed assertion")               ; printed; the script then exits with 1
```

### Exit status {#script-exit-status}

| Status | Meaning                                                                                |
| ------ | -------------------------------------------------------------------------------------- |
| 0      | The script ran to its end and no assertion failed                                      |
| 1      | At least one `assert` failed                                                           |
| 2      | An error stopped the script: a failed check of the script, a file, a sort, the library |

`check` never changes the status. A one-off query that should fail a shell command uses `assert`. In an interactive
session, an error ends the form, not the session, and makes the session end with status `2`.

```console
$ mata script run --expression '(set-option :alphabet "bytes") (assert (nfa:universal? (nfa:create-from-regex "a*")))'
mata: assertion failed at 1:32
$ echo $?
1
```

### Witnesses {#witnesses}

Several decision procedures of the library can return a word that explains their answer through an output parameter:
`is_included`, `are_equivalent`, `is_universal`, `is_lang_empty` and `is_intersection_empty`. There are two ways to
get it.

Each of these procedures has one verdict that a word can explain: inclusion, equivalence and universality when they
do not hold, and the two emptiness checks when the language is not empty. `(witness application)` makes the call with
the output parameter and returns the stored `Word` for that verdict, and `none` for the other one. The call is made
once, so the verdict is not computed separately. The application must be a call of `nfa:included?`,
`nfa:equivalent?`, `nfa:universal?`, `nfa:language-empty?`, `nfa:intersection-empty?`, or of the
[`lazy:`](@ref the-lazy-namespace) decisions `lazy:empty?`, `lazy:included?` and `lazy:equivalent?`; anything else is an
error:

```scheme
(define counterexample (witness (nfa:included? smaller bigger)))   ; a word of smaller not in bigger, or none
(assert (none? counterexample))                                    ; fails when there is such a word
```

With `(set-option :produce-witnesses true)`, every such call passes the output parameter, and `(get-witness)` returns
the witness of the most recent decision operation, the `nfa:`, `nft:` and `lazy:` operations whose names end in `?`,
and `none` when that operation stored none. This is the convenient form in an interactive session:

```scheme
(set-option :produce-witnesses true)
(check (nfa:included? smaller bigger))   ; false
(get-witness)                            ; a b b: the word that shows it
```

Asking for a witness can make a decision procedure slower, so `:produce-witnesses` is off by default. Without it, a
call outside `witness` computes no witness, and `(get-witness)` returns `none` after it. A witness is a word only: the
states of an accepting run come from `nfa:get-accepting-run`.

A complete script with both forms:

```scheme
(mata-script "0.1.0" :requires (witnesses))
(set-option :alphabet "bytes")
(define smaller (nfa:create-from-regex "a(b|c)*"))
(define bigger (nfa:create-from-regex "a(b*|c*)"))

; witness: one call, which returns the word for the verdict that a word can explain
(define counterexample (witness (nfa:included? smaller bigger)))
(if (none? counterexample)
    (echo "included")
    (echo (concat "not included, for example " (to-string (get counterexample)))))   ; a c b
(echo (to-string (witness (nfa:included? bigger smaller))))      ; none: bigger is included
(echo (to-string (witness (nfa:universal? bigger))))             ; ε: the empty word is not in bigger
(echo (to-string (witness (nfa:language-empty? bigger))))        ; a: a word of bigger

; :produce-witnesses: every decision stores its witness, and get-witness reads the last one
(set-option :produce-witnesses true)
(check (nfa:equivalent? smaller bigger))                        ; false
(echo (to-string (get-witness)))                                ; a c b
```

## Timing {#timing}

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

A script that measures each step of a determinization by hand:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

; the words whose seventh letter from the end is a: their DFA needs 2^7 states
(define language (nfa:create-from-regex "(a|b)*a(a|b){6}"))
(define dfa (time "determinize" (nfa:determinize language)))           ; determinize: 0.000094
(echo (concat "states: " (to-string (nfa:number-of-states dfa))))      ; states: 128
(define minimal (time "minimize" (nfa:minimize dfa)))                  ; minimize: 0.000081

; one record for several expressions; time returns the value of the last one
(time "both-checks"
  (check "included" (nfa:included? language dfa))                     ; included: true
  (check "equivalent" (nfa:equivalent? dfa minimal)))                  ; equivalent: true

; three records with labels of their own: inclusion-0, inclusion-1 and inclusion-2
(list:map (lambda (round) (time (concat "inclusion-" (to-string round)) (nfa:included? language dfa)))
          (list:range 0 3))
```

The same measurement without any `time` form, as a benchmark binary does it:

```scheme
(mata-script "0.1.0")
(set-option :time-operations true)                  ; before the declarations, so loading is measured too
(declare-input lhs Nfa)                             ; parsing: …
(declare-input rhs Nfa)                             ; parsing#2: …
(check (nfa:included? lhs rhs))                     ; nfa:included?: …, then the verdict
```

## Options {#options}

| Option                 | Values                              | Meaning                                                 |
| ---------------------- | ----------------------------------- | ------------------------------------------------------- |
| `:alphabet`            | `"on-the-fly"` (default), `"bytes"` | The [script alphabet](@ref alphabets)                   |
| `:default`             | Hint-only application               | Changes a hint default; see [Arguments](@ref arguments) |
| `:produce-witnesses`   | `false` (default), `true`           | See [Witnesses](@ref witnesses)                         |
| `:time-operations`     | `false` (default), `true`           | See [Timing](@ref timing)                               |
| `:optimize`            | `false` (default), `true`           | See [Optimizer](@ref optimizer)                         |
| `:trace`               | `false` (default), `true`           | Prints each library call and hints to standard error    |
| `:loops-and-recursion` | `false` (default), `true`           | Enables [loops and recursion](@ref loops-and-recursion) |

`set-info` attaches information that does not change evaluation; see [Script information](@ref script-information).

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")            ; before the first use of the alphabet
(set-option :produce-witnesses true)      ; every decision stores its witness
(set-option :time-operations true)        ; a timing record for every library call

(check (nfa:universal? (nfa:create-from-regex "[a-z]*")))   ; nfa:create-from-regex: …, nfa:universal?: …, false
(echo (to-string (get-witness)))                            ; \x00: a byte that is not a letter
```

## Optimizer {#optimizer}

By default a script runs exactly as written. `(set-option :optimize true)` allows the interpreter to replace the
calls a script asks for with cheaper ones.

- **What is preserved:** the language of every automaton, and every result that depends only on languages.
- **What is not preserved:** the structure of automata. The number of states, the numbering of states, and which
  witness is returned may differ.
- **What is never changed:** the operand of a form that shows structure, namely `print`, `write`, the
  `number-of-states` and `number-of-transitions` operations, and the predicates on structure such as
  `nfa:deterministic?`, is computed as written.
- **What is reported:** timing records and the trace describe the calls that were made, not the calls that were
  written. A decision run as a lazy query is reported as `lazy:empty?`.

The rewrites are:

| Written                                                                    | Run                                    |
| -------------------------------------------------------------------------- | -------------------------------------- |
| `(nfa:language-empty? (nfa:intersection a b))`                             | `is_intersection_empty(a, b)`          |
| A decision over operations, such as `(nfa:included? a (nfa:complement b))` | One lazy query                         |
| A definition whose value is never used                                     | Nothing                                |
| The same application of the same values twice                              | One call                               |
| The last use of a named value                                              | The operand is moved instead of copied |

A **decision over operations** is `nfa:language-empty?`, `nfa:universal?`, `nfa:included?` or `nfa:equivalent?`
without keywords, whose operands apply `nfa:union-nondeterministic`, `nfa:intersection`, `nfa:language-difference` or
`nfa:complement` without keywords. These operations become a term of `mata::lazy` instead of being built, and the
query explores only as much of it as the answer needs. `nfa:determinize`, `nfa:minimize`, `nfa:reduce`,
`nfa:remove-epsilon` and `nfa:trim` keep the language and are left out. The other operands are evaluated as written.
The witness of the query is the witness of the decision. A complement, and `nfa:universal?`, are taken over the
script alphabet. A lazy query cannot do that when an automaton reads a symbol outside of the script alphabet (under
`"on-the-fly"`, the byte symbols of `nfa:create-from-regex`), so then the operations are applied as written. The
intersection emptiness of two operands without such operations stays the first rewrite, which the library decides as
fast.

A benchmark that measures one specific operation should leave the optimizer off.

A script whose decisions the optimizer rewrites. The `:trace` lines on standard error show the calls that were made:

```scheme
(mata-script "0.1.0" :requires (optimizer trace))
(set-option :alphabet "bytes")
(set-option :optimize true)
(set-option :trace true)

; the words whose eleventh letter from the end is a, and those whose eleventh letter from the end is b
(define lhs (nfa:create-from-regex "(a|b)*a(a|b){10}"))
(define rhs (nfa:create-from-regex "(a|b)*b(a|b){10}"))
(define unused (nfa:determinize lhs))      ; never read, so never computed

(check "disjoint" (nfa:language-empty? (nfa:intersection lhs rhs)))   ; one call of nfa:intersection-empty?
(check "included" (nfa:included? lhs (nfa:complement rhs)))           ; one lazy:empty?, no complement built
(check "again" (nfa:language-empty? (nfa:intersection lhs rhs)))      ; the same call: no call at all
```

It prints `true` three times. Without `:optimize`, the trace shows `nfa:determinize`, two `nfa:intersection` and
`nfa:language-empty?` pairs, `nfa:complement` and `nfa:included?`, and the complement builds a deterministic automaton
of 2049 states.

## Portable core {#portable-core}

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

A script in the portable core is always guaranteed to [terminate](@ref termination).

An interpreter for another library must reject an operation it does not have. It may ignore a hint it does not know,
with a warning, because a hint only selects how a result is computed.

```scheme
(mata-script "0.1.0" :profile core)          ; mata script check rejects anything outside of the core
(declare-input lhs Nfa)
(declare-input rhs Nfa)

(define union (nfa:union-nondeterministic lhs rhs))
(define minimal (time "minimize" (-> union nfa:determinize nfa:minimize)))   ; -> and time are in the core
(check "lhs-in-union" (nfa:included? lhs minimal))
(check "same-language" (nfa:equivalent? union minimal))
```

`mata script check` accepts the script and reports `termination: guaranteed`. Adding `(define (f x) x)` makes it
report `a portable core script cannot define a function`.

(script-information)=

## Script information {#script-information}

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

```scheme
#!/usr/bin/env -S mata script run
(mata-script "0.1.0")
(set-info :name "regex-equivalence")
(set-info :description "Checks that two spellings of one regular expression agree")
(set-info :expects true)                 ; the verdict a test runner should see
(set-info :x-timeout 60)                 ; free for a tool of one's own
(set-option :alphabet "bytes")

(check (nfa:equivalent? (nfa:create-from-regex "(a|b)*") (nfa:create-from-regex "(a*b*)*")))   ; true
(echo (concat "ran " (get (get-info :name))))   ; ran regex-equivalence: get-info returns an Option
(echo (to-string (get-info :license)))          ; none: the script did not set it
```

(versions)=

## Versions {#script-versions}

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
  `loops-and-recursion`, `optimizer`, `trace`, `automaton`, `strings`, `intermediate` and `lazy`. An interpreter
  without one of them rejects the script before running it, naming the feature, rather than failing at the first form
  that needs it. `mata formats` lists the features and whether the build implements them.
- **`:profile`** selects a [profile](@ref portable-core); `core` is the only one, and any other name is an error.
- **A script without a header** is read as the version of the interpreter that runs it. That suits a one-line
  `--expression` and an interactive session, but a script kept in a file should name its version; `mata script check`
  warns about one that does not.
- **Deprecation.** A minor version may deprecate a name with a warning; the name is removed only in the next major
  version.

The language starts at `0.1.0`. The version describes the language only: the behaviour of the library, such as the
numbering of states or the default algorithm of an operation, and the options of the `mata` command have versions of
their own, which `mata version` prints beside it.

## Examples {#examples}

A regression specification that needs no input file. It prints nothing and exits with `0` while every assertion
holds:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define language (nfa:create-from-regex "a(b|c)*"))
(define minimal (-> language nfa:determinize nfa:minimize))

(assert (nfa:deterministic? minimal))                     ; minimization keeps it deterministic
(assert (nfa:equivalent? language minimal))               ; and keeps the language
(assert (nfa:in-language? minimal (word "a" "b" "c")))    ; a word that must be accepted
(assert (not (nfa:in-language? minimal (word "b"))))      ; and one that must not
```

A table of algebraic laws of regular expressions, each checked with its own label:

```scheme
(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define (same? left right)
  (nfa:equivalent? (nfa:create-from-regex left) (nfa:create-from-regex right)))

; each law: a label and two expressions that must denote the same language
(define laws
  (list (list "star-of-star"   "(a*)*"   "a*")
        (list "union-of-stars" "(a|b)*"  "(a*b*)*")
        (list "shift"          "(ab)*a"  "a(ba)*")
        (list "distribute"     "a(b|c)"  "ab|ac")))

; check prints star-of-star: true, …; assert makes a broken law fail the run
(list:map (lambda (law)
            (assert (check (list:nth law 0) (same? (list:nth law 1) (list:nth law 2)))))
          laws)

; a law that does not hold, with the word that shows it
(define not-a-law (witness (nfa:equivalent? (nfa:create-from-regex "(ab)*")
                                            (nfa:create-from-regex "a*b*"))))
(echo (concat "(ab)* and a*b* differ on " (to-string (get not-a-law))))   ; a b a b
```

The image of a language under a transducer, checked against a specification, with a function:

```scheme
(mata-script "0.1.0")
(declare-input transducer Nft)          ; three files, in this order
(declare-input language Nfa)
(declare-input specification Nfa)

(define (image (transducer Nft) (language Nfa))
  (nft:to-nfa (nft:apply transducer language)))

; a word of the image outside of the specification, or none
(define counterexample
  (witness (nfa:included? (image transducer language) specification)))
(if (none? counterexample)
    (echo "the image satisfies the specification")
    (print (get counterexample)))
```

A script that works on an input of either kind:

```scheme
(mata-script "0.1.0" :requires (automaton))
(declare-input a Automaton)             ; two NFAs, or two transducers
(declare-input b Automaton)

(check (automaton:included? a b))       ; both checks are nfa:included? or both nft:included?
(check (automaton:included? b a))
```

A benchmark over random automata, with its sizes as parameters. Each seed gives one pair of automata, and each pair
is checked with both inclusion algorithms:

```scheme
(mata-script "0.1.0")
(declare-param states Int 20)          ; --param states=60 for larger automata
(declare-param symbols Int 2)
(declare-param seeds Int 3)

; Tabakov and Vardi's model: 1.5 transitions per state and symbol, half of the states final
(define (random-nfa seed)
  (nfa:create-random-nfa-tabakov-vardi states symbols "1.5" "0.5" :seed seed))

; the two verdicts for one seed, with records seed-0-naive, seed-0-antichains, …
(define (measure seed)
  (let ((lhs (random-nfa seed))
        (rhs (random-nfa (+ seed 1000)))
        (label (concat "seed-" (to-string seed))))
    (list:map (lambda (algorithm)
                (time (concat label "-" algorithm)
                  (nfa:included? lhs rhs :algorithm algorithm)))
              (list "naive" "antichains"))))

; both algorithms must give the same verdict for every seed
(define verdicts (list:map measure (list:range 0 seeds)))
(check "algorithms-agree"
  (list:fold (lambda (agree pair) (and agree (= (list:nth pair 0) (list:nth pair 1))))
             true
             verdicts))
```

```console
$ mata script run --param states=60 --param seeds=2 random-inclusion.matasc
seed-0-naive: 0.129378
seed-0-antichains: 0.037182
seed-1-naive: 0.072660
seed-1-antichains: 0.000045
algorithms-agree: true
```

Regular model checking of a token-passing protocol, with every automaton written in the script. A configuration is a
word with one letter per process: `t` holds the token, `n` does not. The loop computes the configurations reachable
from `tnnnn` and checks that none of them has zero or two tokens:

```scheme
(mata-script "0.1.0" :requires (transducers loops-and-recursion))
(set-option :loops-and-recursion true)
(set-option :alphabet "bytes")

; one step: the token moves one process to the right; every other letter is copied
(define step
  (nft:literal :levels 2 :initial (p) :final (q)
               :states ((p-n 1) (p-t 1) (give 1) (take 1) (q-n 1) (q-t 1))
               :transitions ((p n p-n) (p-n n p) (p t p-t) (p-t t p)    ; copy before the move
                             (p t give) (give n r) (r n take) (take t q) ; tn becomes nt
                             (q n q-n) (q-n n q) (q t q-t) (q-t t q))))  ; copy after the move

(define initial (nfa:create-from-regex "tnnnn"))
(define bad (nfa:complement (nfa:create-from-regex "n*tn*")))   ; not exactly one token
(define (post (configurations Nfa))                              ; the successors of configurations
  (nft:to-nfa (nft:apply step configurations)))

; add successors until nothing new is reached; minimizing keeps the automaton small
(define reachable
  (loop ((reached initial) (round 0))
    (define next (nfa:minimize (nfa:union-nondeterministic reached (post reached))))
    (if (nfa:included? next reached)
        (do (echo (concat "fixpoint after " (to-string round) " rounds")) reached)
        (recur next (+ round 1)))))

(check "safe" (nfa:intersection-empty? reachable bad))                                ; safe: true
(check "token-reaches-the-end" (nfa:in-language? reachable (word "n" "n" "n" "n" "t")))   ; true
(echo (nfa:print-to-regex reachable :mode "readable"))
```

```text
fixpoint after 4 rounds
safe: true
token-reaches-the-end: true
nnn(tn|nt)|(nnt|(tn|nt)n)nn
```

A protocol with a bug gives the other verdict: with `(give t r)` in place of `(give n r)`, a step copies the token
instead of passing it, and `safe` prints `false`. The initial configurations are fixed here because the fixpoint of
`tn*`, a token at the start of a line of any length, is not reached by adding one step at a time: each round reaches
one more position. `examples/rmc/armc.matasc` handles such families with an abstraction.

## Open questions {#open-questions}

- **Command-line interface.** Settled in [The `mata` command](@ref cli): inputs are positional or named with `--input`,
  values come from `declare-param` and `--param`, options and hint defaults are set in the script only, and `check`
  writes its report to standard error. A script written inline in a shell command or in a `pycobench` job still needs
  care, because both can replace `$1` themselves before the interpreter sees it.
- **Loops and recursion** are an option that an interpreter may leave out. Whether two functions should be able to
  call each other by name is open.
- **Runaway scripts.** A script without the termination guarantee can run forever: an interpreter of the language
  limits the depth of calls to an error, as `mata` does, but does not limit the number of rounds of a loop. A
  script that runs forever has to be stopped from the outside.
- **Alternating automata** are not covered, as the library does not support them.
- **The exact set of rewrites** of the optimizer, and whether the list of forms that show structure is the right one.
