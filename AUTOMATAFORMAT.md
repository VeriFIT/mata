# The Mata file format for automata (.mata format)

## Top-level file structure
* The format is **line**-based. Lines can be connected by `\`.
* Lines are parsed into tokens. Tokens are delimited by white spaces, the format is **white space sensitive** with exception of formulas in transitions where expressions could be written without white spaces separating tokens.
* A file contains **sections** starting with a line of the form `@<SECTION-TYPE>`, containing an automaton or, in theory, anything. We will start with one section per file, containing an automaton, but it is obviously extensible to more sections and more kinds of things.
* In automata sections, non-empty lines are **key-value lines** of the form `%<KEY> [VALUE]`, **transition lines**, or **comment lines** starting with `#`. Several key-value lines with the same key mean that the key is mapped to the set of values, an occurence without a value marks that the `KEY` is defined. 
* Besides white spaces and the end of line, the following are **characters with special meaning**: `&`,`|`,`!`,`@`,`(`,`)`,`%`,`"`,`\`,`#` `[`,`]`,`a`,`q`,`n`,`t`,`f`.
  * `&`,`|`,`!`,`(`,`)` are logical operators used in Boolean formulas.
  * `[`,`]`,`-`,`^` enclose character classes, such as `[abcd-h]`, `[^abcd-h]`.
  * `@` opens the line with a section name and is used in transducer alphabet tokens of the form `x@a1`, `y@[10-55]`,...
  * `%` opens a key-value line.
  * `"` strings containing white spaces and special characters can be written in between `"`. The special characters lose their special meaning. The characters `"` and `\` inside a string must be escaped, i.e. `\"` and `\\`. 
  * `\` for escaping characters. `\` is also used to concatenate lines.
  * `a`,`q`,`n`,`t`,`f` are token type specifiers (alphabet, state, node, attribute, formula). 
  * `#` starts a comment line.
* There are **words with a special meaning**: `\true`,`\false`, `\min`, `\max`. The first two represent true/false in Boolean formulas.
* The parser may recognise the special symbols as standalone tokens (does not apply to `q`,`a`,`n`,`t`,`f`,`#`) only if they appear in the special context where their special meaning applies, otherwise they should be treated as normal symbols.

We categorise automata according to their **transition type**, that is, the structure of their transitions, and by their **alphabet type**, i.e., how alphabet symbols on transitions are represented. It is expected that different parameters would be parsed into different data structures. These parameters should determine the name of the automata section.

## Alphabet type
We categorise automata by the **alphabet type**, i.e., the representation of symbols on transitions. We will consider propositional formulae (over bit vectors), explicit symbols, intervals of numbers or unicode or ascii, or user-defined alphabet.
* **Explicit**: Just plain symbols. May be given implicitly or enumerated, or one can have some predefined alphabet, numbers, ascii, utf.
* **Bitvector**: Propositional variables, the syntax is the same as for explicit symbols.
* **Character class**: Character classes, i.e. sets of symbols as used in normal regular expressions, are of the form `[bla]` or `[^bla]` where `bla` is a sequence of symbols and intervals of symbols such as `a-z`, for instance `abcd-hij-z` denotes all lower case alphabet symbols. The `^` in front complements the entire class. The character `-` in sincluded as `\-`, analogously `\` and `^`. The special keywords `\min` and `\max` denote the first repsective the last letter the alphabet.
## Automata by their transition type

### NFA (Nondeterministic finite automata)
* The transitions are triples consisting of a source state, transition formula, and target state. Note that a single explicit symbol or a single interval is also a transition formula. 
* Both symbol literals and states are determined positionally. Markers or enumeration can also be used.
* Initial (final) states are defined by key-value line `%Initial"` (`%Final`) followed by an enumeration of states.

### AFA (Alternating finite automata)
* Transition lines are of the form `"<State> <Formula>"` representing the source state and the transition formula - a Boolean formula over symbol literals, states, and nodes.
* Several lines starting with the same state mean the disjunciton of the formulae. No line for the state means false. 
* Initial (final) states are defined by key-value line `%Initial <Formula>"` (`"%Final <Formula>"`) where the Boolean formula is given over states.

The transition and alphabet type is encoded in the name of a section, e.g., `@NFA-explicit` denotes that a nondeterministic finite automaton over an explicit alphabet is given.
## Distinguishing between states, symbols, and nodes
* When the general formulas are used in transition (as in case of AFA), the type of a token cannot be determined by its position. We therefore require one of the three schemes for distinguishing token types:
  * By **enumeration**.
  * By **type-markers** (default). The literal is preceeded by a single character that specifies the type, we use `q` for states, `a` for symbols, and `n` for shared nodes. The marker *is* a part of the thing's name. 
  * **Automatic**. If all the other types are specified by enumeration or by markers, the third type may be marked as automatic, the parser will assign it to everything which is not of the two previous types. 
* The typing scheme is specified as a key value line with the key  `<Thing>-<Scheme>` where  `<Thing>` may be `Alphabet`, `States`, or `Nodes`. `<Scheme>` may be `marked`, `enum`, or `auto`.  The scheme `enum` is followed by an enumeration. For instance, we can have `%Alphabet-enum a b c karel`, `%Alphabet-auto`, `%Alphabet-marked`.
* Moreverover, in case of alphabet one can also use `utf-8`, `chars`, or `numbers` as typing scheme to denote that an alphabet is all utf-8 symbols, all characters, or all numbers respectively.
* Shared nodes are used to share same formula or part of formula among more transitions.

We support specifying **epsilons**, by a key-value line `%Epsilon <formula>` where formula has the same syntax and meaning as the formulae used in transitions (depends on the alphabet type and typing scheme). The formula may be just a single letter which is supposed to mean epsilon, it may be a disjunciton of letters, when we need several different epsilons, or a more complex formula describing boolean vectors that are supposed to mean epsilons.  Use cases for multiple epsilons appear for instance in string solving.

## AFA and NFA Examples
```
@AFA-bits
%Initial q1
%Final !q1 & !q2
q1 q2 & (a1 | q1 | n1)
n1 q3 | q4
q1 a2 & q5
```
```
@AFA-explicit
%States-enum q r s t "(r,s)"
%Alphabet-auto
%Initial (q & r) | (s & t)
%Final !q & !r & !s & !t
q symbol | other_symbol & ("(r,s)" | r | s)
```
```
@AFA-intervals
%States-auto
%Alphabet-utf
%Initial (q & r) | ("(r,s)" & t)
%Final !q & !r
q [a-z] | [2-7] | [\u{1c}-\u{5c}] & ("(r,s)" | r | s)
```
```
@NFA-bits
%Alphabet-auto
%Initial q1 q2
%Final q3 q4
q1 (x &  y) | z q4
q2 x & !y q3
```
```
@NFA-intervals
%Alphabet-utf
%States-auto
%Initial q
%Final s
q [a-z] s
```
## Transducer
A transducer has named tracks, and it has a key-value line starting with `%Tracks`. We use `<lit>@x` to say that the `<lit>` belongs to the track `x`. We may also specify their names by a type-identifier or enumeration. An example:
```
@AFA-intervals
%States-auto
%Tracks-auto
%Alphabet-utf
q [a-z]@x | [2-7]@y | [\u{1c}-\u{5c}]@z & ("(r,s)" | (r & s))
```
``` 
@AFA-intervals
%Tracks-auto
%Alphabet-bits
q a1@x | a2@z & ("(r,s)" | r | s)
```
 
## Special formulas and relations over symbols and transducer track names.
It may be useful to specify relation between symbol variables or transducer tracks. These may be SMT formulas or something special, such as `a"x=y"` specifying that the two transducer tracks read the same symbol. We reserve the type letter `f` for this. We this fuzzy for now, to be concretized when it is actually needed. 

``` 
@AFA-intervals
%Tracks-enum x y z
%Alphabet-enum a b c d e f h
q (a@x | b@z) & f"z=x" ("(r,s)" | (r & s))
```
The example above does not work in the current version where the special formulae and symbols are the same thing.
 
## Aliases 
It could be good to allow a key-value line `%Alias bla bli`, which specifies an alias. The parser will replace occurrences of the token `bla` with the string `bli`.

## Attributes
We want to assign attributes to states, symbols, nodes, transducer tracks, transitions. The attributes are meant as something not relevant for the semantics of the automaton, for instance some attributes used when siplaying the automaton, such as colors. 
1. An attribute inside a transition is a token such as a state, node, symbol. Its identifier is `t`. We can also give attributes to states, symbols, nodes by writing key-value lines of the form `%Attribute <state> <attribute>`.
