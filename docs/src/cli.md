# The `mata` command

**Status: implemented, with named gaps.** The `mata-cli` crate of the Rust workspace builds this command, a binary named
`mata` that runs [Mata Script](script-language) scripts and performs one-off automata operations without writing a
script. The command is implemented in Rust on top of the
[Rust bindings](https://mata.pages.fit.vutbr.cz/rust/mata/); see [Why Rust](#why-rust). This page stays the
specification: where the current interpreter does not reach it, the gap is named under
[implementation notes](#implementation-notes) and reported by the command itself, never approximated.

```console
$ mata script run email-filter.matasc filters/*.mata --input spec=spec.mata
inclusion-time: 0.012300
inclusion: true
```

## Command tree

```
mata <command> [arguments]
```

| Command                      | Effect                                                                |
| ---------------------------- | --------------------------------------------------------------------- |
| `mata script run SCRIPT …`   | Runs a `.matasc` file and reports its results                         |
| `mata script check SCRIPT …` | Checks a `.matasc` file without running the operations                |
| `mata script repl`           | Starts an interactive Mata Script session reading from standard input |
| `mata nfa <operation> …`     | Performs one NFA operation directly, with automata as files           |
| `mata nft <operation> …`     | Performs one NFT operation directly                                   |
| `mata convert …`             | Converts automata between the `.mata` and DOT formats                 |
| `mata formats`               | Lists the supported formats, language features and every operation    |
| `mata version`               | Prints the versions of the command, of Mata Script and of the library |
| `mata help [command]`        | Prints usage; `--help` works everywhere                               |

`mata script run` is the entry point for whole computations; `mata nfa`, `mata nft` and `mata convert` are for one
operation whose script would be a single form. They share one executable, one Rust implementation and the same
[bindings](https://mata.pages.fit.vutbr.cz/rust/mata/), so a script, an interactive session and a one-off query give the
same answer to the same library call. The subcommand is `script`, singular, because it acts on one Mata Script, as
`mata nfa` acts on one automaton.

Options and operands are separated by `--` where either side accepts file names; after `--`, every argument is an
operand. A path operand of `-` means standard input, except where a script file name is expected and standard input is
already used for a script from `-`, or the command writes its result to the path `-`.

## Scripts

### `mata script run`

```console
mata script run [options] [SCRIPT] [INPUT…]
```

- `SCRIPT` is a `.matasc` file, or `-` for a script read from standard input. It can also be given as
  `--script PATH`; that form is for shell aliases and generated command lines. `SCRIPT`, `--script` and `--expression`
  are mutually exclusive, and exactly one of them is required.
- A script file may start with a `#!` line, such as `#!/usr/bin/env -S mata script run`, so that it can be an executable
  file; the line is skipped.
- Every operand after `SCRIPT` is a positional input, in order: they become `$1`, `$2`, … and `inputs` in the script.
  An input is the path string as given, not a loaded automaton; a `declare-input` or `declare-inputs` form loads it, and
  `nfa:load` takes such a path directly. An input of `-` is standard input, unless the script itself was read from
  there.
- Each `--input NAME=PATH` names one input: `$name` is its path, and a `declare-input NAME` takes the input given under
  that name. A name can be given at most once, and `--input` can be repeated.
- Each `--param NAME=VALUE` gives the parameter that the script declares with `(declare-param NAME Sort [default])`.
  The value is read as the parameter's sort, `String`, `Int` or `Bool`, so `--param count=five` for an `Int` parameter
  is an error before the script runs, and so is a parameter without a default that is given no value.
- Positional inputs are consumed by `declare-input` first, then by `declare-inputs`; named inputs are always found by
  name. A file with several automata gives one automaton to each declaration in turn. When a script declares inputs,
  every input on the command line must be taken by a declaration: an extra positional input, an `--input` the script
  does not declare and a `--param` it does not declare are errors, reported before anything runs. A script without
  declarations reads its inputs as `$1`, `$2`, …, `inputs` or `$name`.
- A path given on the command line is relative to the current directory, as in every other command; when the script
  file has a directory part, a relative one is made absolute, so `$1` and `inputs` hold the absolute path. A path
  written in the script, as in `(nfa:load "local.mata")`, is relative to the directory of the script file; a script
  given as `-` or with `--expression` has no file, so its paths are relative to the current directory too.
- `--output PATH` redirects the script's standard output: `print`, `echo`, `check`, timing records and REPL values are
  written to `PATH` instead of standard output. `--output -` is standard output, the default. Errors always go to
  standard error. An output that is the file of an input is an error, before anything is written.

The whole script is checked, as [`mata script check`](#mata-script-check) does, before its first form runs, so a
misspelled operation in the last line stops the run before any output and before any automaton is loaded. Its declared
inputs are then loaded in order with the `set-option` and `set-info` forms between them, so `(set-option :alphabet
"bytes")` before the declarations applies to the automata they load.

`--no-check` skips that check: the forms run in order until one fails, and an error is reported when it is reached,
after the output of the forms before it. The header is still read, so a script for another language version or one
that `:requires` a feature this build lacks is still rejected before anything runs; the `:profile core` restrictions,
which only the check enforces, are not. The option is for scripts the checker rejects but that run correctly.

Options set at run time go through the script's `set-option` form, not through command-line options; an option cannot
be different from the one the script asks for, except that `--output` redirects only where the output goes. This keeps
one script runnable the same way everywhere and keeps a benchmark script's meaning in the script itself. A value that
should come from the command line is a parameter.

A script may be given a script string instead of a file with `--expression SCRIPT-TEXT`. It exists for short shell
commands; `$1` inside the script still refers to the first input, because the shell expands `$1` in a double-quoted
shell string, not in a script file. Use single quotes around `--expression` in a shell.

Example with declared inputs:

```console
$ cat check-inclusion.matasc
(mata-script "0.1.0")
(declare-input smaller Nfa)
(declare-input bigger Nfa)
(check "inclusion" (time "inclusion-time" (nfa:included? smaller bigger)))

$ mata script run check-inclusion.matasc small.mata big.mata
inclusion-time: 0.000108
inclusion: true
```

Example with an inline script:

```console
$ mata script run --expression '(print (nfa:number-of-states (nfa:load $1)))' a.mata
2
```

### `mata script check`

```console
mata script check [SCRIPT | --script PATH | --expression SCRIPT-TEXT]
```

Checks the script without loading any input or running any operation, in this order:

1. **Version:** the [`mata-script` header](script-language) names a language version this `mata` runs. The header is
   read before the rest of the script, so a script for a newer version is rejected with the version it needs, not with
   a syntax error about something added later.
2. **Syntax:** the script parses as Mata Script.
3. **Names and shapes:** every operation, special form, keyword and hint exists in the declared version; every
   fixed-arity call has the right number of arguments, a pipeline step counting the piped value as its first one; every
   declaration is well formed; no name of an operation or of the language is defined; `loop`, `recur` and self-calls
   are rejected without `:loops-and-recursion`.
4. **Sorts, where known:** an operation of the `nfa:` and `nft:` namespaces is checked against its fixed sorts; a sort
   annotation on `define` or `as` is checked, as are the values of hints and `set-info` keys.
5. **Termination report:** whether the script is guaranteed to terminate, as described in
   {ref}`Termination <script-termination>`.

`check` takes no inputs: it does not read an automaton and accepts no positional operand and no `--input`. The script
comes from a file, from standard input as `-`, from `--script PATH`, or from `--expression`.

The output is one line of the form `checked: <path>` on standard output, `checked: <stdin>` or `checked: <expression>`
for a script without a file, followed by report lines on standard error: `termination: guaranteed` or
`termination: not-guaranteed`, and one line for every warning: an unused declaration, as `unused declaration 'x'`, or a
script without a `mata-script` header. Anything that would stop the script before its first library call is an error
instead of a report and sets [exit status](#exit-status) `2`.

`check` does not run the script, so a misspelled input path or a malformed automaton is not found by it, because it
never reads one. It only checks what can be decided without evaluating the script.

### `mata script repl`

```console
mata script repl [--output PATH]
```

Reads Mata Script forms from standard input and prints the value of every top-level expression, one line per value, to
the output stream; an automaton is shown by its kind and size, as `#<Nfa with 3 states and 2 transitions>`, and `print`
shows it in full. Errors print a diagnostic on standard error and end the current form only, and the session goes on.
A form may go on over several lines, and it is evaluated once it is complete. `(exit)` ends the session. The session
keeps definitions and options until it ends. Every form that can appear at the top level of a script can be
typed. A session has no inputs to pass on its command line, so `$1`, `$name` and `inputs` are errors there,
`mata: no positional input 1`, and loading an automaton needs a path in the script, as `(nfa:load "a.mata")`.

Because each form is evaluated as soon as it is complete, the session can be driven by a pipe:

```console
$ printf '%s\n' \
    '(set-option :alphabet "bytes")' \
    '(define a (nfa:create-from-regex "a(b|c)*"))' \
    '(nfa:number-of-states a)' \
  | mata script repl
2
```

The session prints the prompt `mata> ` only when standard input is a terminal, so a non-interactive session reads
without printing prompts and is fit for tests and the website playground.

## Direct operations

The direct commands are for one operation whose script would be a single form.

```console
mata nfa <operation> [--hint NAME=VALUE]… [--output PATH] OPERAND…
mata nft <operation> [--hint NAME=VALUE]… [--output PATH] OPERAND…
```

For example:

```console
$ mata nfa intersection a.mata b.mata c.mata | mata nfa determinize -
@NFA-explicit
…
$ mata nfa included small.mata big.mata --hint algorithm=naive
true
```

- Operations mirror the script names exactly, but without the `nfa:` or `nft:` prefix: `mata nfa determinize`,
  `mata nfa intersection`, `mata nft compose`. A query may leave out its trailing `?`, which a shell would otherwise
  read as a pattern: `mata nfa included` is `nfa:included?`.
- Each operand is read as the sort its position takes. An automaton operand is a file in the
  [`.mata` format](automata-format), or `-` for standard input, which can be read once; a file with several automata
  gives one operand each. A `Word` operand is symbol names separated by spaces, as `mata nfa in-language a.mata 'a b'`,
  and an empty operand `''` is the empty word; `ε`, as printed for the empty word, is read as a symbol named `ε`.
  A `String` operand is the text itself, as `mata nfa create-from-regex 'a(b|c)*'`; an `Int` operand is a number.
- An operation whose operands a command line cannot give, such as a function or `nfa:literal`, is not a direct
  command; `mata nfa --help` lists exactly the operations that are, each with its operands and hints.
- A binary operation of the algebra, such as `intersection`, `union-nondeterministic`, `concatenate` or `compose`, takes
  more than two operands and folds them from the left, as in a script.
- `--hint NAME=VALUE` gives a hint or an optional parameter of the operation, as
  `(nfa:included? a b :algorithm "naive")` and `(nfa:concatenate a b :use-epsilon true)` do in a script. A keyword the
  operation does not take, or a value it does not accept, is an error that lists what it takes; nothing is silently
  ignored. Any other option is an error, as `mata: unknown option '--algorithm'`.
- All operands of one command are loaded together, so bitvector alphabets are mintermized together exactly as
  `declare-inputs` loads them. Automata that cannot be mintermized together are an error, never a silent mismatch.
- Automaton-returning operations write to standard output, or to the file given by `--output PATH`. `--output -` means
  standard output, the default.
- A decision operation prints `true` or `false` to the output stream, one line.
- A number operation prints the number, one line.
- A word operation prints one word as a sequence of symbol names separated by spaces, or `ε` for the empty word.

`mata nfa --help` and `mata nft --help` list every operation the installed `mata` command supports as a direct command;
`mata formats` prints every format, every language feature and every script operation one entry per line, prefixed
`format`, `feature` or `operation`, for programs that read it.

### Direct queries and exit status

A decision operation that returns a value uses the [query status](#exit-status): `0` for `true`, `1` for `false`. A
script's `check` never changes the status, because a script is a program, not a query; the direct commands are
queries, so they can be used directly in a shell `if`. An assertion in a script uses status `1` for failure, matching
the query convention.

## Conversion

```console
mata convert [--from mata|dot] [--to mata|dot] [--kind nfa|nft]
             [--output PATH] [INPUT…]
```

- `--from` defaults to the `.mata` format; `--to` defaults to DOT. `--from dot` converts DOT to `.mata`, so both
  directions are available.
- `--kind` selects what the input is parsed as. It defaults to detecting the kind from the first automaton's
  `%LevelsNum` heading: `@NFA-explicit` without levels is an NFA, `@NFT-explicit` or any section with `%LevelsNum` is
  an NFT; a DOT graph with state labels, `forcelabels=true` or `num_of_levels` is an NFT. All inputs of one invocation
  must have the same kind; mixed kinds are an error.
- Every input is converted in order to the output stream, or to one file per input when `--output PATH` names a
  directory; with several inputs and no `--output`, each converted automaton follows the previous one, separated by a
  blank line, as the `.mata` format allows several sections. Two inputs that would be written to the same file in the
  directory, such as `one/x.mata` and `two/x.mata`, are an error before anything is written.
- `--output -` writes to standard output, the default. `INPUT` of `-` reads standard input, but standard input can be
  used only once.
- `mata convert --from dot` is the only part that accepts DOT. The DOT it accepts is exactly what
  `print_to_dot` writes, extended only where another writer is documented here.

### DOT conventions

Mata's DOT output is deliberately small: a digraph, one node per state and one edge per transition. Input is parsed
under fixed conventions, so anything outside them is an error; unsupported constructs are never dropped.

| DOT construct                                        | Meaning                                                |
| ---------------------------------------------------- | ------------------------------------------------------ |
| `digraph finiteAutomaton { … }`                      | One automaton; the digraph name is ignored             |
| `node [shape=circle]`, `node [shape=none, label=""]` | Graph defaults; ignored                                |
| `s [shape=doublecircle]`                             | State `s` is final                                     |
| `s [shape=circle]`                                   | State `s` exists and is not final                      |
| `N [label="N:level"]`                                | State `N` is on `level`, so the graph is a transducer  |
| `iN -> N` from a `shape=none` node                   | State `N` is initial; the marker node is not a state   |
| `s -> t [label="a,b"]`, `[label="[a-c]"]`            | One transition under each listed symbol                |
| `s -> t` without `label`                             | A transition under the abstract symbol `0`             |
| `[tooltip="…"]` on an edge                           | Ignored; it repeats the full label of a shortened one  |
| `forcelabels=true`                                   | A transducer; its writer emits it for the state labels |
| `num_of_levels=N`                                    | A transducer with `N` levels                           |
| states `0`, `1`, `…`                                 | State numbers; state names must be numbers             |

An input holds exactly one graph, and every state from `0` to the highest appears in a statement, as the writer draws
every state. The library's DOT carries no level count, so `mata convert`, `nfa:print-to-dot` and `nft:print-to-dot`
add `num_of_levels=N` after the `digraph` line of a transducer; without it the level count is the highest state level
plus one.

The symbol label is the reverse of the {ref}`script alphabet <script-alphabets>`; `mata convert`, `nfa:print-to-dot` and
`nft:print-to-dot` all write symbol names, so their outputs agree:

- A plain name such as `a` or `q0` is looked up, and inserted under the next free symbol if new.
- `<eps>` is epsilon, `<dntcr>` is the don't-care symbol, and `<n>` is the symbol number `n`. A number `n` that is not
  a known name is symbol `n` too, as the library writes a symbol without a name. A new name never takes the number of
  a symbol the graph lists.
- A range `[a-c]` expands to every symbol it names; `a,b` expands to the two symbols `a` and `b`.
- An empty label is an error.
- `\\`, `\"`, `\,`, `\[` and `\<` are the characters `\`, `"`, `,`, `[` and `<` inside a name, as Graphviz shows
  them; any other escape is an error. The writers escape these characters in names and write a name of digits or with
  a control character as `<n>`, so every label reads back as the same symbols.

A DOT graph that uses anything else is rejected with the construct and its line: subgraphs, clusters, node attributes
other than `shape` and `label`, edge attributes other than `label` and `tooltip`, attributes on a marker edge, `strict`,
undirected `graph`, HTML labels, ports, `rank`, `constraint`, a node label that is not `N:level` for the node's own
number, a level not below `num_of_levels`, another graph attribute, a state name that is not a number, a gap in the
state numbers, a second graph, a graph without its closing `}`, a marker edge not of the form `iN -> N`, or an edge
label with no listed symbols. Nothing is skipped, guessed or approximated.

## Output conventions

- A stream of values: one line per value. An automaton is printed in the `.mata` format, so it can be piped into
  another `mata` command.
- Every value is written and flushed as it is printed, so a run that is stopped by a time limit keeps every record it
  printed before.
- A boolean is printed as `true` or `false`.
- A word is printed as symbol names separated by spaces, or `ε` for the empty word.
- A list is printed one element per line; no brackets or commas are printed, because a value is meant to be readable
  by another command.
- A record is exactly `label: value`, one line: a timing record of `time` or of `:time-operations` has the seconds
  with a period and six fractional digits, and a labelled `(check "label" condition)` has `true` or `false`. A label is
  printed once per run, because a reader of the records, such as `pycobench`, keys them by label; a second record with
  the same label is an error. `:time-operations` labels a record by the operation and numbers repetitions, as
  `nfa:trim`, `nfa:trim#2`.
- Errors are written to standard error as `mata: <message>`, one line. When an error has a place in the script, the
  message is followed by its line and column, as `mata: unknown name 'nfa:determinizee' at 3:1`; an error at run time
  without a place of its own gets the place of the form that was running. A failed `assert` writes
  `mata: assertion failed at <line>:<column>` and the script goes on.
- The trace of a run is not implemented: `(set-option :trace true)` is rejected with `option :trace is not
implemented`, so a run does not report its library calls yet.
- No progress output, banners or colours are printed, unless standard error is a terminal and the command is
  interactive; the content of the streams never changes, only the prompt.

## Exit status

| Status | Meaning                                                                                        |
| ------ | ---------------------------------------------------------------------------------------------- |
| 0      | Success: the script ran to its end with no failed assertion, or a direct query returned `true` |
| 1      | A script assertion failed, or a direct query returned `false`                                  |
| 2      | Usage, version, parse, sort, file, alphabet or library error                                   |

`mata help`, `mata formats`, and a script that only reports with `check` always end with status `0`, unless they fail
for an operational reason, which is status `2`.

The status is set once, by the command that ends the process. A REPL ends with `2` if any form of the session failed
with an error, else with `1` once an assertion in the session failed, else with `0`; the session continues after an
error, so a piped session reports every error, and its status shows that one happened. A script run by
`mata script run` sets the status described in {ref}`Scripts <script-exit-status>`.

## Versions

`mata version` prints three lines: the version of the command, the version of the Mata Script language it runs, and the
version of the `libmata` it is linked with.

```console
$ mata version
mata 0.1.0
Mata Script 0.1.0
libmata 1.32.45
```

A script declares the language version it is written for in its `mata-script` header; see
[the script language page](script-language). A script without a header is read as the version of the `mata` that runs
it, which suits a one-line `--expression` but not a script that is kept: `mata script check` warns about a file
without one.

## Why Rust

The command is implemented in Rust on top of the existing Rust bindings because:

- The bridge cost is irrelevant at the granularity of an operation. A call across the CXX bridge that only asks for
  the number of states of a 7-state automaton costs about `9` ns, against about `2` ns for a C++ call that is not
  inlined; determinizing that automaton takes about `24` µs either way, so the bridge is not measurable where the
  library works.
- The bindings already cover the operations a first Mata Script interpreter needs: construction from regular
  expressions and words, `.mata` parsing with a shared alphabet, the NFA algebra, determinization, minimization,
  reduction, decisions, witnesses, DOT output and a substantial part of the NFT API.
- A command-line program in Rust needs no new binding surface and no new dependency: the standard library is enough
  for the parser and the argument handling. A C++ implementation would have to add a lexer, a parser and argument
  parsing beside a build that has no command-line executable today, and would duplicate the safe API the Rust crate
  already offers.
- The same implementation serves the interactive session and the website playground, where safe memory handling and
  a library published as a crate matter more than the last nanosecond of call overhead.

The coverage that the current interpreter cannot reach from the existing crates is reported, not hidden. The
`mata-sys` bridge is the place to add a library call the script language needs; an operation the interpreter does not
have is rejected by its exact name, and never approximated or served by a placeholder. The operations that the script
language page specifies but this build does not have are listed in the
[implementation notes](#implementation-notes).

## Implementation notes

- The executable lives in `bindings/rust/mata-cli`, a binary crate named `mata` in the existing Rust workspace, so
  `cargo build --release --package mata-cli` builds it against the same Mata build as the bindings. It uses no crates
  outside the Rust workspace.
- The command tree is one module per subcommand; the script lexer, parser, checker and evaluator are one module each,
  used by all of `run`, `check` and `repl`.
- The evaluator keeps one alphabet for the whole run and maps every symbol name through it, as the script language
  requires; automata loaded together are parsed through the same alphabet, so bitvector alphabets are mintermized
  together.
- The DOT reader is a new parser in the CLI crate, because the library has none; it rejects everything outside
  [DOT conventions](#dot-conventions) and writes only the `.mata` format.
- `mata nfa`, `mata nft` and `mata convert` call the same evaluator functions as the script interpreter, so a direct
  command and a one-line script cannot disagree.
- Every operation of the catalogue records the language version that added it. A script sees the operations of the
  version its header declares, and the names of all of them are reserved, so an operation added later can never clash
  with a name a script defined.
- The interpreter implements 129 operations: the portable core of the language, the script forms that a direct command
  needs, and the NFA and NFT operations `bindings/rust/mata` covers. An operation outside that table is rejected by
  name and never approximated: a script reports `unknown name 'strings:get-segments'` and a direct command reports
  `unknown operation 'strings:get-segments'`, both exiting `2`. These 22 operations of the
  [script language](script-language) page are absent and need new `mata-sys` bridge functions:
  `automaton:determinize`, `automaton:included?`, `automaton:intersection`, `automaton:load`, `automaton:parse`,
  `intermediate:mintermize`, `intermediate:parse-from-mata`, `nfa:construct`, `nft:construct`,
  `strings:create-identity`, `strings:create-identity-with-single-symbol-replace`,
  `strings:get-accepted-symbols`, `strings:get-longest-words`, `strings:get-segments`,
  `strings:get-shortest-words`, `strings:get-word-lengths`, `strings:get-words-of-lengths`,
  `strings:language-epsilon?`, `strings:reluctant-nfa`, `strings:replace-reluctant-literal`,
  `strings:replace-reluctant-regex`, `strings:replace-reluctant-single-symbol`.
  `nft:minimize` is not one of them: the library declares it only under `MATA_NFT_NOT_IMPLEMENTED`.
- The hints are those the bindings expose: `:algorithm` of `nfa:complement` (`"classical"`, `"brzozowski"`),
  `nfa:minimize` (`"hopcroft"`, `"brzozowski"`) and `nfa:reduce` (`"simulation"`, `"residual"`, with `:type` `"after"`
  or `"with"` and `:direction` `"forward"` or `"backward"`), and `:algorithm` `"antichains"` or `"naive"` of
  `nfa:included?`, `nfa:equivalent?` and `nfa:universal?`. `(set-option :default …)` changes the default of any of
  them. The other operations take no hints yet.
- Optional parameters of the library functions are keyword arguments of their sort, named after the parameter:
  `(nfa:concatenate a b :use-epsilon true)`. A `Bool` is `true` or `false`, an `Int` a number, and a `Symbol` is
  given by its name, a `String`, as in `(symbol "a")`. A direct command takes them as `--hint NAME=VALUE`, as
  `--hint use-epsilon=true`, and `(set-option :default …)` changes their defaults too; a call without one gets the
  library default. `mata nfa --help` and `mata nft --help` list them with their sorts. The parameters this build
  takes are `:use-epsilon` of `nfa:concatenate` and of `nfa:in-language-prefix?`, `:final-condition` (`"and"` or
  `"or"`) and `:first-epsilon` of `nfa:product`, `:first-epsilon` of `nfa:left-quotient` and of
  `nfa:right-quotient`, `:abstract-symbol` of `nfa:get-one-letter-automaton`, `:seed` of
  `nfa:create-random-nfa-tabakov-vardi`, and `:match-prefix` and `:has-epsilon-cycles` of `nft:in-language?`. The
  last one defaults to `true`, which only allows a transducer that has such a cycle; `false` on one that has it is the
  error `the transducer has a cycle of epsilon transitions`. The language has no sort for a real number, so the
  transition ratio and the final state density of that builder are decimal strings, as in
  `(nfa:create-random-nfa-tabakov-vardi 4 2 "1" "0.5")`.
- Of the script options, `:alphabet` with `on-the-fly` or `bytes`, `:produce-witnesses`, `:time-operations` and
  `:default` work. `:optimize` and `:trace` are accepted only as `false`; `true` is rejected as
  `option :<name> is not implemented`, and `:loops-and-recursion` is always rejected, so `loop`, `recur` and self-calls
  are errors with the same reason. `mata formats` lists the features a header can require and whether this build has
  them.
