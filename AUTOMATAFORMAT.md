
# The file format for automata
* Based partially on Ondrej Lengal's [`.vtf' format](https://discord.com/channels/@me/864885374375821312/980792642927460372), a format of Pavol Vargovcik, and a confused discussion.

## Top-level file structure
* The format is **line**-based. Lines can be connected by `\`.
* Lines are parsed into tokens. Tokens are delimited by white spaces, the format is **white space sensitive**.
* A file contains **sections** starting with a line of the form `@<SECTION-TYPE>`, containing an automaton or, in theory, anything. We will start with one section per file, containing an automaton, but it is obviously extensible to more sections and more kinds of things.
* In automata sections, non-empty lines are **key-value lines** of the form `%<KEY> [VALUE]`, **transition lines**, or **comment lines** starting with `#`. Several key-value lines with the same key mean that the key is mapped to the set of values, an occurence without a value marks that the `KEY` is defined. 
* Besides white spaces and the end of line, the following are **characters with special meaning**: `&`,`|`,`!`,`@`,`(`,`)`,`%`,`"`,`\`,`#` `[`,`]`,`a`,`q`,`n`,`t`,`{`,`}`.
  * `&`,`|`,`!`,`(`,`)` occurring as tokens in transition lines are logical operators.
  * `[`,`]`,`-` within intervals of symbols of the form `[bla-bli]`.
  * `{`,`}` are enclosing attributes, if we use them this way. Remains to be decided. (?)
  * `@` opens the line with a section name and is used in transducer alphabet tokens of the form `x@a1`, `y@[10-55]`,...
  * `%` opens a key-value line, possibly also key-value appendix of a transition. Remains to be decided. (?)
  * `"` strings containing white spaces and special characters can be written in between `"`. The special characters lose their special meaning. The characters `"` and `\` inside a string must be escaped, i.e. `\"` and `\\`. 
  * `\` for escaping characters `\` is also used to concatenate lines.
  * `a`,`q`,`n`,`t` are token type specifiers (alphabet, state, node, attribute). 
  * `#` starts a comment line.
* There are two **words with special meaning**: `true`,`false`.
* The parser may recognise the special symbols as standalone tookens (does not apply to `q`,`a`,`n`,`t`,`#`) only if they appear in the special context where their special meaning applies, otehrwise they should be treated as normal symbols.

We categorise automata by **transition type**, that is, the structure of their transitions, and by **alphabet type**, i.e., how alphabet symbols on transitions are represented. It is expected that different parameters would be parsed into different data structures. These parameters should determine the name of the automata section.

## Automata by transition type: 
We use general form of AFA, by Pavol V., and simple NFA.

### AFA 
* Each state has a transition, a booelan formula over symbol literals, states, and nodes.
* Several lines starting with the same state mean the disjunciton of the formulae. No line for the state means false. 
* Since in the general formulae, the type of a token cannot be determined by its position, we require one of the three schemes for distinguishing token types.
  * By **enumeration**.
  * By **type-markers**. The literal is preceeded by a single character that specifies the type, we use `q` for states, `a` for symbols, and `n` for shared nodes. The marker *is* a part of the thing's name. 
  * **automatic**. If all the other types are specified by enumeration or by markers, the third type may be marked as automatic, the parser will assign it to everything which is not of the two previous types. 
* The typing scheme is specified as a key value line with the key  `<Thing>-<Scheme>` where  `<Thing>` may be `Alphabet`, `States`, or `Nodes`. `<Scheme>` may be `markerd`, `enum`, or `auto`.  The scheme `enum` is followed by an enumeration.  For instance, we can have `%Alphabet-enum a b c karel`, `%Alphabet-auto`, `%Alphabet-markers`.
* Markers is the implicit option for all types of tokens, used if nothing is specified.
* `true` means true and `false` means false.

### NFA 
* The transitions are triples consisting of a source state, transition formula, and target state. Note that a single explicit symbol or a single interval is also a transition formula. 
* Both symbol literals and states are determined positionally, corresponding to `auto` in AFA. Markers or enumeration can also be used.

## Alphabet type
Next, we categorise automtata by the **alphabet type**, representation of symbols on transitions. We will consider propositional formulae (over bit vectors), explicit symbols, unicode (?), asci (?), numbers (?), intervals of numbers or unicode or asci.
* **Explicit**: Just plain symbols. May be given implicitly or enumerated, or one can have some predefined alphabet, numbers, asci, utf.
* **Bitvector**: Propositional variables, syntax is the same as explicit symbols.
* **Interval**: Intervals of symbols are of the form [a-z]. We might also want complements, [^a-z], but I am not sure that we want to work with complements. We might also want to give abstract intervals of the form [min,a], [a,max], [min,max], or use inf and -inf instead of min and max. This would be particularly needed if we do not have complements and do not know the alphabet range.

We support specifying **epsilons**, by a key-value line `%Epsilon <formula>` where formula has the same syntax and meaning as the formulae used in transitions (depends on the alphabet type and typing scheme).

## AFA and NFA Examples
```
@AFA-bits
q1 q2 & (a1 | q1 | n1)
n1 q3 | q4
q1 a2 & q5
```
```
@AFA-explicit
%States-enum q r s t "(r,s)"
%Alphabet-auto
q symbol | other_symbol & ("(r,s)" | r | s)
```
```
@AFA-intervals
%States-auto
%Alphabet-utf
q [a-z] | [2-7] | [\u{1c}-\u{5c}] & ("(r,s)" | r | s)
```
```
@NFA-bits
q (x &  y) | z r
s x & !y t
```
```
@NFA-intervals
%Alphabet-utf
q [a-z]
```
## Transducer
Transducer has named tracks, and it has the key-value line starting with `%Tracks`. We use `x@<lit>` do say that the <lit> belongs to the track `x`. We may also specify their names by type-identifier or enumeration. Example:
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
 
## Special formulas over symbols and transducer track names.
The user may parse symbol token strings in a specific way. They may be SMT formulas or some special king of fomulas, such as `a"x=y"` specifying that the two transducer tracks read the same symbol... 

``` 
@AFA-intervals
%Tracks-enum x y z
%Alphabet-enum a b c d e f h
q (a@x | b@z) & f"z=x" ("(r,s)" | (r & s))
```
The example above does not work in the current version where the special formulae and symbols are the same thing.
 
## Aliases 
It could be good to allow a akey-value line `%Alias bla bli`, which specifies an alias. The parser will replace occurrences of the token `bla` with the string `bli`.

## Attributes
We want to assign attributes to states, symbols, nodes, transducer tracks, transitions. 
1. An attribute inside a transition is a token such as state, node, symbol. Its identifier is `t`. We can also give attributes to states, symbols, nodes by writing key-value lines of the form `%Attribute <state> <attribute>`. 
