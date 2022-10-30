# The .fa format for automata
This is a draft for the `.fa` (finite automata) format for automata. It started as a modification of the [.vtf format](https://github.com/ondrik/automata-benchmarks/tree/master/vtf) of MATA.
The suffix a and name `.fa` is so far preliminary, as the entire content of this file, and serves to facilitate the discussion. 
The .fa format deviates from the .vtf format is concerned with only a subset of what is considered in .vtf.

An example of the general structure of a `.fa` file follows:
```
@<SECTION-TYPE-1>
<body line 1>
# a comment
<body line 2>
%KeyA valueA_1
<body line 3>   # the third line of body
%KeyA valueA_2
%KeyB valueB_1
<body line 4>
%KeyC

@<SECTION-TYPE-2>
...
```
### Explanation:
* The format is **line**-based, i.e., one line is the basic building block. **Lines can be concatenated with \\.**
* White spaces are used as delimeters.
* A file can contain zero, one, or more **sections**, each of them defining an automaton or operations over an automaton, or basicaly anything. **Let's first concentrate on one section per file that contain an automaton ?**
* `@`-starting lines denotes beginning of **sections**, `<SECTION-TYPE-x>` denotes the type of the section (e.g. the type of the automaton within).
* `%`-starting lines denote **meta** information, provided in the form of a dictionary mapping keys (e.g. `KeyX`) to sets of values.  If one key is defined several times, the resulting set of values is the union of all the partial definitions.  This is used e.g. for defining alphabet or final states of an automaton.  For instance:
  * `KeyA` is mapped to { `valueA_1`, `valueA_2` },
  * `KeyB` is mapped to { `valueB_1` },
  * `KeyC` is mapped to { } (i.e., only the information that `KeyC` is defined is preserved),
  * `KeyD` is undefined.
* `#` until the end of a line denotes a comment. **This could also be a line starting with %comment or %# or something like that, so we do not kill the '#'.**
* The rest of the lines (`<body line x>` in the example) define the **body** of the file, e.g., transitions of an automaton or code for an interpreter.
* tokens are strings or they may appear in between ""


## Alternating automata
* This is the main thing. 
* The section names will serve mainly as identifiers of the type of the automaton (NFA,AFA ...) and the type of the data structure used to represent transitions (explicit, interval, bdd, ...)
* The most general form is the automaton in the DAG format of Pavol: every state has transition formula over states, nodes, and symbol predicates.
* The parser needs a way of distinguishing these three types of things (states, nodes, symbol predicates). 
* The body are in the form ```node_or_state transition_right-hand side```
* Several lines with the same node_or_state are understood as a non-determinism, disjunction


<!---
# From the vtf. We can modify this after we know what we want.

## EBNF-like grammar (from vtf. , might eventually be updated)
```
print char   = ? see https://en.wikipedia.org/wiki/ASCII#Printable_characters ? ;
line char    = print char | "\t" ;
space tab    = " " | "\t" ;
white space  = space tab , { space tab } ;
eol          = [ "#" , { line char } ] , "\n" ;
special char = '"' | "(" | ")" | "#" | "%" | "@" | "\\" ;
string char  = print char - ( space tab | special char ) ;
string       = string char , { string char } ;
token        = string
             | '"', { ( line char - '"' ) | ( "\\" , '"' ) } , '"'
             | "(" | ")" ;
token list   = [ white space ] , [ token , { white space , token } , [ white space ] ] ;
meta line    = "%" , string , token list ;
line meat    = token list | meta line ;
line         = line meat ,  eol ;
section      = "@" , string , eol , { line } ;
file         = { section } ;
```

## Examples

Examples of files in the `.vtf` format follow:

### Finite automata
[link](nfa-example.vtf)
```
# Example of the MATA format for storing or exchanging automata
#
# comments start with '#'

@NFA       # denotes the type of the automaton, it should determine the data structure into which it would ultimatelly be parsed into
           # @type preamble starts a
           # section that will (in this case) define one automaton; the section
           # ends either with an end-of-file (or with another @type preamble if we decide to support more sections)
           
# now, we follow with the definition of components of an automaton
%Name nfa1                        # name of the automaton (optional, can be used to refer to the automaton)
 %Alphabet a b c d                # alphabet (optional) (a whitespace before % is OK)
%Initial q1 q2                    # initial states (required); a definition spans until the end of line
%Initial q3                       # a key can be repeated, the result should be the same as if in a single line
%Initial "a state"                # when in ", names can have whitespaces (and also " if escaped with backslash '\')

%Initial "\"we're here,\" he said"# a state with the name |"we're here," he said| ('|' are not part of the name)
                                  # names cannot span multiple lines
%Final q2                         # final states (required)
q1 a q1                           # transitions occur when there is no keyword
q1 a q2                           # the format is <source> <symbol> <target> 
"q1" b "a state"                  # note that "q1" and q1 are the same
"\"we're here,\" he said" c q1
q1 () q2                          # () is used for epsilon transitions

```
### Tree automata
[link](nta-example.vtf)
```
# Example of tree automata in the MATA format
@NTA               # nondeterministic tree automaton
%Root q2           # root states (required)
q1 a (q1 q2)       # the format of transitions is <parent> <symbol> (<child_1> ... <child_n>)
"q1" b "q1"        # is equivalent to q1 b (q1)
q2 c               # is equivalent to q2 c ()

```
### Finite automata with transitions in BDDs
[link](nfa-bdd-example.vtf)
```
# Example of finite automata with transitions in a BDD in the MATA format
@NFA-BDD          # NFAs with transitions in BDD
%Symbol-Vars 8    # number of Boolean variables in the alphabet (required)
%Initial q1 q2
%Final q2

q1 000x11x1 q2    # the format is <source> <symbol> <target> 
q1 01101111 q3    # 'x' in the binary vector denote don't care values
q3 xxxxxxxx q1    # the length needs to match the value in '%Symbol-Vars'

```
### Finite automata with everything in BDDs
[link](nfa-bdd-full-example.vtf)
```
# Example of finite automata where both states and transitions are in a BDD in the MATA format
@NFA-BDD-FULL     # NFAs with states and transitions in BDD
%State-Vars 3     # number of Boolean variables in states (required)
%Symbol-Vars 8    # number of Boolean variables in the alphabet (required)
%Initial 111 1x1
%Final 00x

111 000x11x1 0x0  # the format of transitions is <source> <symbol> <target> 
xxx xx11xx00 11x  # 'x' in the binary _vectors denote don't care values

```
### A sequence of operations
[link](code.vtf)
```
# Example of how to define a sequence of operations in the MATA format

@NFA
%Name nfa1
%Initial q1
%Final q2
q1 a q2

@NFA
%Name nfa2
%Initial r1
%Final r2
r1 a r2

@CODE                  # some code comes here
NFA nfa3 = (minus (union nfa1 nfa2) (intersect nfa1 nfa2))
bool empty = (isempty nfa3)
(print "NFA3:\n")
(print NFA3)
(print "is empty:")
(print empty)
(return empty)

```

### Symbolic finite automata
[link](sfa-example.vtf)
```
# Example of a symbolic finite automaton (in the sense of Margus & Loris) in the MATA format [TENTATIVE PROPOSAL, NOT FIXED!!!]
@SFA               # symbolic finite automaton
%Name sfa1         # identifier (optional)
%Initial q1        # initial states (required)
%Final q2          # final states (required)
# TODO: maybe specify theories?

q1 "(even x)" q1   # the format is <source> <formula> <target>
"q1" "(odd x)" q1  # 'x' in the formula denotes the read symbol
q2 "(= x 3)" q3    # (actually, any name can be used, as long as there is
q1 "(forall ((x Int)) (= cur x))" q3 # at most one free variable in the formula)

```

### Finite transducers
[link](nft-example.vtf)
```
# Example of a finite transducer in the MATA format
@NFT               # nondeterministic finite transducer
%Name  trans       # name (optional)
%Initial q1        # initial states (required)
%Final q2          # final states (required)
%Alphabet a b c    # alphabet (optional)

q1 (a) (b) q2      # the format is <source> (<input symbol 1> ... <input symbol n>) (<output symbol 1> ... <output symbol m>) <target>
q1 () (a b c) "q1"
q2 (a b) () q3

```

### Symbolic finite transducers
[link](sft-example.vtf)
```
# Example of a symbolic finite transducer in the MATA format
@SFT               # symbolic finite transducer
%Name  trans       # name (optional)
%Initial q1        # initial states (required)
%Final q2          # final states (required)
# TODO: restrict the theories?

q1 ("(= x 3)") ("(+ x 3)" "0") q2       # the format is <source> (<input predicate 1> ... <input predicate n>)
                                        # (<output function 1> ... <output function m>) <target>
q1 ("(even x)" "(odd y)") ("y" "x") q2  # here, we use a transition over two
                                        # symbols; note that the free variables
                                        # used in the predicates are used in
                                        # the output functions to refer to the
                                        # position of the symbols
q1 ("(= x x)") ("x") q3                 # this is how to specify the 'true'
                                        # predicate and also bind the symbol to a variable
q1 () ("1") q3                          # epsilon transitions allowed too
q1 ("(in x (list 1 2 3)") ("x") q3      # the input symbol is one of {1,2,3}, the output is the same

```

### Probabilistic automata
[link](dpa-example.vtf)
```
# Example of a deterministic probabilistic automaton in the MATA format [TENTATIVE PROPOSAL, NOT FIXED!!!]
@DPA                     # deterministic probabilistic automaton
%Name dpa1               # identifier (optional)
%Initial q1:0.5 q2:0.5   # initial states + probabilities (required) 
%Final q2:0.3 q3:0.7     # final states + probabilities (required)

q1 a:0.4 q1   # the format is <source> <symbol>:<prob> <target>
q1 b:0.6 q1   # the probabilities of outgoing transitions + acceptance should add up to 1
q2 a:0.7 q3
q3 b:0.3 q3

```

### Relations over states
[link](state-rel-example.vtf)
```
# Example of a relation on automaton states in the MATA format
@NFA
%Name aut1
%Initial q1 q2
%Final q3

q1 a q3
q3 a q3
q2 a q4


@STATE-REL
%Name "Simulation for aut1"      # identifier (optional)
%For-Automaton aut1              # denotes on the states of which automaton the relation is
%Type direct-sim                 # type of the relation (e.g. "direct-sim" for direct simulation)

q1 q3             # denotes sim(q1, q3)
q2 q1             # denotes sim(q2, q1)
q2 q3
q4 q3

q1 q1
q2 q2
q3 q3
q4 q4

```
--->
