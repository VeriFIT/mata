# The `mata` command

**Status: implemented, with named gaps.** The `mata-cli` crate of the Rust workspace builds this command, a binary named
`mata` that runs [Mata Script](script-language) scripts and performs one-off automata operations without writing a
script. The command is implemented in Rust on top of the
[Rust bindings](https://mata.pages.fit.vutbr.cz/rust/mata/); see [Why Rust](#why-rust). This page stays the
specification: where the current interpreter does not reach it, the gap is named under
[implementation notes](#implementation-notes) and reported by the command itself, never approximated.

```console
$ mata scripts run email-filter.matasc filters/*.mata --input spec=spec.mata
inclusion: 0.0123
true
```

## Command tree

```
mata [global options] <command> [arguments]
```

| Command                       | Effect                                                                |
| ----------------------------- | --------------------------------------------------------------------- |
| `mata scripts run SCRIPT …`   | Runs a `.matasc` file and reports its results                         |
| `mata scripts check SCRIPT …` | Checks a `.matasc` file without running the operations                |
| `mata scripts repl`           | Starts an interactive Mata Script session reading from standard input |
| `mata nfa <operation> …`      | Performs one NFA operation directly, with automata as files           |
| `mata nft <operation> …`      | Performs one NFT operation directly                                   |
| `mata convert …`              | Converts automata between the `.mata` and DOT formats                 |
| `mata formats`                | Lists the supported formats and every direct operation                |
| `mata version`                | Prints the version of the command                                     |
| `mata help [command]`         | Prints usage; `--help` works everywhere                               |

`mata scripts run` is the entry point for whole computations; `mata nfa`, `mata nft` and `mata convert` are for one
operation whose script would be a single form. They share one executable, one Rust implementation and the same
[bindings](https://mata.pages.fit.vutbr.cz/rust/mata/), so a script, an interactive session and a one-off query give the
same answer to the same library call.

Options and operands are separated by `--` where either side accepts file names; after `--`, every argument is an
operand. A path operand of `-` means standard input, except where a script file name is expected and standard input is
already used for a script from `-`, or the command writes its result to the path `-`.

## Scripts

### `mata scripts run`

```console
mata scripts run [options] [SCRIPT] [INPUT…]
```

- `SCRIPT` is a `.matasc` file, or `-` for a script read from standard input. It can also be given as
  `--script PATH`; that form is for shell aliases and generated command lines. `SCRIPT` and `--script` are mutually
  exclusive, and exactly one of them is required.
- Every operand after `SCRIPT` is a positional input, in order: they become `$1`, `$2`, … and `inputs` in the script.
  An input is the path string as given, not a loaded automaton; a `declare-input` or `declare-inputs` form loads it, and
  `nfa:load` takes such a path directly.
- Each `--input NAME=PATH` names one input: `$name` is its path, and a `declare-input NAME` takes the input given under
  that name. The name must be a valid Mata Script name, and a name can be given at most once. `--input` can be
  repeated.
- Positional inputs are consumed by `declare-input` first, then by `declare-inputs`; named inputs are always found by
  name. A positional input is used by at most one declaration.
- Every relative path is resolved against the directory of the script file, whether the path is written in the script
  or given on the command line as an input operand; only an absolute path is used as given. A script given as `-` or
  with `--expression` has no file, so it resolves relative paths against the current directory.
- `--output PATH` redirects the script's standard output: `print`, `echo`, `check`, timing records and REPL values are
  written to `PATH` instead of standard output. `--output -` is standard output, the default. Errors always go to
  standard error.

Options set at run time go through the script's `set-option` form, not through command-line options; an option cannot
be different from the one the script asks for, except that `--output` redirects only where the output goes. This keeps
one script runnable the same way everywhere and keeps a benchmark script's meaning in the script itself.

A script may be given a script string instead of a file with `--expression SCRIPT-TEXT`, which is mutually exclusive
with `SCRIPT` and `--script`. It is read exactly like a file whose directory is the current directory. It exists for
short shell commands; `$1` inside the script still refers to the first input, because the shell expands `$1` in a
double-quoted shell string, not in a script file. Use single quotes around `--expression` in a shell.

Example with named inputs:

```console
$ cat check-inclusion.matasc
(declare-input smaller Nfa)
(declare-input bigger Nfa)
(check (time "inclusion" (nfa:included? smaller bigger)))

$ mata scripts run check-inclusion.matasc small.mata big.mata
inclusion: 0.0123
true
```

Example with an inline script:

```console
$ mata scripts run --expression '(print (nfa:number-of-states (nfa:load $1)))' a.mata
2
```

### `mata scripts check`

```console
mata scripts check [SCRIPT | --script PATH | --expression SCRIPT-TEXT]
```

Checks the script without loading any input or running any operation, in this order:

1. **Syntax:** the script parses as Mata Script.
2. **Names and shapes:** every operation, special form and keyword exists; every fixed-arity call has the right number
   of arguments; every declaration is well formed; `define` does not shadow a name in the same scope; `loop`, `recur`
   and self-calls are rejected without `:loops-and-recursion`.
3. **Sorts, where known:** an operation of the `nfa:` and `nft:` namespaces is checked against its fixed sorts; a sort
   annotation on `define` or `as` is checked.
4. **Termination report:** whether the script is guaranteed to terminate, as described in
   {ref}`Termination <script-termination>`.

`check` takes no inputs: it does not read an automaton and accepts no positional operand, no `--input` and no `-`. The
script comes from a file, from `--script PATH`, or from `--expression`.

The output is one line of the form `checked: <path>` on standard output, `checked: <expression>` for an
`--expression` script, followed by report lines on standard error: `termination: guaranteed` or
`termination: not-guaranteed`, and one line for every portability warning or unused declaration, as
`unused declaration 'x'`. Anything that would stop the script before its first library call is an error instead of a
report and sets [exit status](#exit-status) `2`.

`check` does not run the script, so a misspelled input path or a malformed automaton is not found by it, because it
never reads one. It only checks what can be decided without evaluating the script.

### `mata scripts repl`

```console
mata scripts repl [--output PATH]
```

Reads Mata Script forms from standard input and prints the value of every top-level expression, one line per value, to
the output stream. Errors print a diagnostic on standard error and end the current form only, and the session goes on;
a failed assertion makes the session end with status `1`, while an error in a form leaves the status at `0`. The session keeps definitions and options until it ends.
Every form that can appear at the top level of a script can be typed. A session has no inputs to pass on its command
line, so `$1`, `$name` and `inputs` are errors there, `mata: no positional input 1`, and loading an automaton needs a
path in the script, as `(nfa:load "a.mata")`.

Because each form is evaluated as soon as it is complete, the session can be driven by a pipe:

```console
$ printf '%s\n' \
    '(set-option :alphabet "bytes")' \
    '(define a (nfa:create-from-regex "a(b|c)*"))' \
    '(nfa:number-of-states a)' \
  | mata scripts repl
2
```

The session prints the prompt `mata> ` only when standard input is a terminal, so a non-interactive session reads
without printing prompts and is fit for tests and the website playground.

## Direct operations

The direct commands are for one operation whose script would be a single form. An operand is a file in the
[`.mata` format](automata-format), or `-` for standard input. The result is an automaton in the `.mata` format by
default, unless the operation returns something else.

```console
mata nfa <operation> [options] AUTOMATON…
mata nft <operation> [options] AUTOMATON…
```

For example:

```console
$ mata nfa intersection a.mata b.mata | mata nfa determinize -
@NFA-explicit
…
```

- Operations mirror the script names exactly, but without the `nfa:` or `nft:` prefix: `mata nfa determinize`,
  `mata nfa intersection`, `mata nft compose`.
- Automaton-returning operations write to standard output, or to the file given by `--output PATH`. `--output -` means
  standard output, the default.
- A decision operation prints `true` or `false` to the output stream, one line.
- A number operation prints the number, one line.
- A word operation prints one word as a sequence of symbol names separated by spaces, or `ε` for the empty word.
- Options of an operation are not implemented: no operation takes one yet, and an option is an error rather than a
  silent no-op, so `mata nfa determinize --algorithm antichains a.mata` reports `mata: unknown option '--algorithm'`
  and exits `2`. A script rejects keyword arguments the same way, with `operation nfa:intersection takes no keyword
arguments here`, so every argument of an operation is positional today.
- All operands of one command are loaded together, so bitvector alphabets are mintermized together exactly as
  `declare-inputs` loads them. Automata that cannot be mintermized together are an error, never a silent mismatch.
- The operand order follows the script: for a binary operation `mata nfa op a.mata b.mata` is `(nfa:op a b)`. For
  operations with more than two automata, the left-to-right grouping is the same as in the script.

`mata nfa --help` and `mata nft --help` list every operation the installed `mata` command supports, each with its
operand count; `mata formats` prints the same list one entry per line, prefixed `format` or `operation`, for programs
that read it.

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
  an NFT; a DOT graph with state labels is an NFT. All inputs of one invocation must have the same kind; mixed kinds
  are an error.
- Every input is converted in order to the output stream, or to one file per input when `--output PATH` names a
  directory; with several inputs and no `--output`, each converted automaton follows the previous one, separated by a
  blank line, as the `.mata` format allows several sections.
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
| `forcelabels=true`                                   | Ignored; the transducer writer emits it for the labels |
| states `0`, `1`, `…`                                 | State numbers; state names must be numbers             |

The symbol label is the reverse of the {ref}`script alphabet <script-alphabets>`:

- A plain name such as `a` or `q0` is looked up, and inserted under the next free symbol if new.
- `<eps>` is epsilon, `<dntcr>` is the don't-care symbol, and `<n>` is the symbol number `n`.
- A range `[a-c]` expands to every symbol it names; `a,b` expands to the two symbols `a` and `b`.
- An empty label is an error.
- Escaped characters are not decoded; `\\` and `\"` in a DOT label are the two characters `\` and `"`, not `a`.

A DOT graph that uses anything else is rejected with the construct and its line: subgraphs, clusters, node or edge
attributes other than `shape`, `label` and `tooltip`, `strict`, undirected `graph`, HTML labels, ports, `rank`,
`constraint`, a node label that is not `N:level` for the node's own number, another graph attribute, a state name that
is not a number, a marker edge not of the form `iN -> N`, or an edge label with no listed symbols. Nothing is
skipped, guessed or approximated.

## Output conventions

- A stream of values: one line per value. An automaton is printed in the `.mata` format, so it can be piped into
  another `mata` command.
- A boolean is printed as `true` or `false`.
- A word is printed as symbol names separated by spaces, or `ε` for the empty word.
- A list is printed one element per line; no brackets or commas are printed, because a value is meant to be readable
  by another command.
- Every timing record is exactly `label: seconds`, with a period and six fractional digits, on the output stream.
- Errors are written to standard error as `mata: <message>`, one line. When an error has a place in the script, the
  message is followed by the form's line and column, as `mata: unknown name 'nfa:determinizee' at 3:1`.
- The trace of a run is not implemented: `(set-option :trace true)` is rejected with `option :trace is not
implemented`, so a run does not report its library calls yet.
- No progress output, banners or colours are printed, unless standard error is a terminal and the command is
  interactive; the content of the streams never changes, only the prompt.

## Exit status

| Status | Meaning                                                                                        |
| ------ | ---------------------------------------------------------------------------------------------- |
| 0      | Success: the script ran to its end with no failed assertion, or a direct query returned `true` |
| 1      | A script assertion failed, or a direct query returned `false`                                  |
| 2      | Usage, parse, sort, file, alphabet or library error                                            |

`mata help`, `mata formats`, and a script that only reports with `check` always end with status `0`, unless they fail
for an operational reason, which is status `2`.

The status is set once, by the command that ends the process. A REPL ends with `0`, or with `1` once an assertion in
the session failed; an error in one form of a session is reported and leaves the status alone, because the session
continues. A script run by `mata scripts run` sets the status described in
{ref}`Scripts <script-exit-status>`.

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
- The interpreter implements 97 operations: the portable core of the language, the script forms that a direct command
  needs, and the NFA and NFT operations `bindings/rust/mata` covers. An operation outside that table is rejected by
  name and never approximated: a script reports `unknown name 'nft:complement'` and a direct command reports
  `unknown operation 'nft:complement'`, both exiting `2`. These 51 operations of the
  [script language](script-language) page are absent and need new `mata-sys` bridge functions:
  `automaton:determinize`, `automaton:included?`, `automaton:intersection`, `automaton:load`, `automaton:parse`,
  `intermediate:mintermize`, `intermediate:parse-from-mata`, `nfa:as-dfa`, `nfa:concatenate-nth-power`,
  `nfa:construct`, `nfa:create-random-nfa-tabakov-vardi`, `nfa:decode-utf8`, `nfa:flat?`,
  `nfa:get-one-letter-automaton`, `nfa:get-word-from-complement`, `nfa:get-word-from-language-difference`,
  `nfa:in-language-prefix?`, `nfa:left-quotient`, `nfa:product`, `nfa:right-quotient`,
  `nfa:union-deterministic-complete`, `nft:complement`, `nft:complete?`, `nft:concatenate-nth-power`,
  `nft:construct`, `nft:contains-jump-transitions?`, `nft:create-sigma-star-nft`,
  `nft:from-nfa-with-levels-advancing`, `nft:has-epsilon-cycle?`, `nft:identical?`, `nft:in-language?`,
  `nft:insert-level`, `nft:insert-levels`, `nft:reduce`, `nft:revert`, `nft:to-nfa-update`, `nft:universal?`,
  `nft:unwind-jumps`, `strings:create-identity`, `strings:create-identity-with-single-symbol-replace`,
  `strings:get-accepted-symbols`, `strings:get-longest-words`, `strings:get-segments`,
  `strings:get-shortest-words`, `strings:get-word-lengths`, `strings:get-words-of-lengths`,
  `strings:language-epsilon?`, `strings:reluctant-nfa`, `strings:replace-reluctant-literal`,
  `strings:replace-reluctant-regex`, `strings:replace-reluctant-single-symbol`.
- Of the script options, `:alphabet` with `on-the-fly` or `bytes`, `:produce-witnesses` and `:time-operations` work.
  `:optimize` and `:trace` are accepted only as `false`; `true` is rejected as `option :<name> is not implemented`,
  and `:default` and `:loops-and-recursion` are always rejected, so `loop`, `recur` and self-calls are errors with the
  same reason.
- An operation's keyword arguments and the command-line options of an operation are not implemented; see
  [Direct operations](#direct-operations).
