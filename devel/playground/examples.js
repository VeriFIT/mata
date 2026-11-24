// Examples of the playground. An operation example sets automata A and B, the operation, and the word; a script
// example sets the script of the Script mode and the values of its declarations.

const EPSILON_NFA = `@NFA-explicit
%Alphabet-auto
%Initial s
%Final f
s a p
s <eps> r
p b f
r c f
r <eps> f
`;

const NFA = `@NFA-explicit
%Alphabet-auto
%Initial p
%Final r
p a p
p b p
p a r
r b r
`;

export const EXAMPLES = [
    {
        title: "Words ending in abb, minimized",
        aFormat: "regex",
        aText: "(a|b)*abb",
        operation: "minimize",
    },
    {
        title: "Determinizing “the fourth symbol from the end is a”",
        aFormat: "regex",
        aText: "(a|b)*a(a|b){3}",
        operation: "determinize",
    },
    {
        title: "Binary numbers divisible by three",
        aFormat: "regex",
        aText: "(0|1(01*0)*1)*",
        operation: "minimize",
    },
    {
        title: "Reducing an automaton by simulation",
        aFormat: "regex",
        aText: "(a|b)*a(a|b)*b",
        operation: "reduce",
    },
    {
        title: "Removing ε-transitions",
        aFormat: "mata",
        aText: EPSILON_NFA,
        operation: "remove_epsilon",
    },
    {
        title: "Complementing a nondeterministic automaton",
        aFormat: "mata",
        aText: NFA,
        operation: "complement",
    },
    {
        title: "Words with both an a and a b",
        aFormat: "regex",
        aText: "(a|b)*a(a|b)*",
        bFormat: "regex",
        bText: "(a|b)*b(a|b)*",
        operation: "intersection",
    },
    {
        title: "Is (a|b)*b included in (a|b)*abb?",
        aFormat: "regex",
        aText: "(a|b)*b",
        bFormat: "regex",
        bText: "(a|b)*abb",
        operation: "is_included",
    },
    {
        title: "Are (ab)*a and a(ba)* equivalent?",
        aFormat: "regex",
        aText: "(ab)*a",
        bFormat: "regex",
        bText: "a(ba)*",
        operation: "are_equivalent",
    },
    {
        title: "Does (a|b)*abb accept babb?",
        aFormat: "regex",
        aText: "(a|b)*abb",
        operation: "is_in_lang",
        word: "babb",
    },
];

const TRANSDUCER_A_TO_B = `@NFT-explicit
%Alphabet-auto
%Initial q0
%Final q0
%Levels q0:0 q1:1 q2:1
%LevelsNum 2
q0 a q1
q1 b q0
q0 c q2
q2 c q0
`;

const WORDS_OF_A_AND_C = `@NFA-explicit
%Alphabet-auto
%Initial p
%Final p
p a p
p c p
`;

const NO_A = `@NFA-explicit
%Alphabet-auto
%Initial s
%Final s
s b s
s c s
`;

// Script examples in groups, one group per namespace they show. Each has a title, the script, and the texts of the
// inputs and the values of the parameters it declares, by name. The smoke test of the build runs every one and expects
// the exit status `status` (0 when left out): bindings/wasm/script-smoke-test.mjs.
//
// Only the namespaces of the interpreter of this branch may be used: a group for `lazy:` or `ws1s:` scripts is added
// here with the namespace (its scripts declare it with `:requires`).
export const SCRIPT_EXAMPLE_GROUPS = [
    {
        title: "Scripts: automata and transducers",
        examples: [
            {
                title: "Minimizing an automaton and drawing it",
                script: `(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define language (nfa:create-from-regex "(a|b)*abb"))
(define minimal (nfa:minimize (nfa:determinize language)))
(check "deterministic" (nfa:deterministic? minimal))
(check "equivalent" (nfa:equivalent? language minimal))
(echo (concat "states: " (to-string (nfa:number-of-states minimal))))
(print (nfa:print-to-dot minimal))
`,
            },
            {
                title: "Inclusion of regular expressions, with a counterexample",
                script: `(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define smaller (nfa:create-from-regex "(a|b)*b"))
(define bigger (nfa:create-from-regex "(a|b)*abb"))
(check "included" (nfa:included? smaller bigger))
; A word of the smaller language that the bigger one lacks.
(print (witness (nfa:included? smaller bigger)))
`,
            },
            {
                title: "A regression specification with assertions",
                script: `(mata-script "0.1.0")
(set-option :alphabet "bytes")

(define language (nfa:create-from-regex "a(b|c)*"))
(define minimal (-> language nfa:determinize nfa:minimize))
(assert (nfa:deterministic? minimal))
(assert (nfa:equivalent? language minimal))
(assert (nfa:in-language? minimal (word "a" "b" "c")))
(assert (not (nfa:in-language? minimal (word "b"))))
(echo "all assertions hold")
`,
            },
            {
                title: "The image of a language under a transducer",
                script: `(mata-script "0.1.0")
; The transducer rewrites a to b and keeps c; the specification has no a.
(declare-input transducer Nft)
(declare-input language Nfa)
(declare-input specification Nfa)

(define image (nft:to-nfa (nft:apply transducer language)))
(define counterexample (witness (nfa:included? image specification)))
(if (none? counterexample)
    (echo "the image satisfies the specification")
    (print (get counterexample)))
(print (nfa:print-to-dot (nfa:minimize (nfa:determinize image))))
`,
                inputs: {
                    transducer: TRANSDUCER_A_TO_B,
                    language: WORDS_OF_A_AND_C,
                    specification: NO_A,
                },
            },
            {
                title: "The blow-up of determinization, timed",
                script: `(mata-script "0.1.0")
; The n-th symbol from the end is a: the minimal DFA has 2^n states.
(declare-param n Int 6)
(set-option :alphabet "bytes")

(define pattern (concat "(a|b)*a(a|b){" (to-string (- n 1)) "}"))
(define nfa (nfa:create-from-regex pattern))
(define dfa (time "determinize" (nfa:determinize nfa)))
(define minimal (time "minimize" (nfa:minimize dfa)))
(echo (concat "NFA states: " (to-string (nfa:number-of-states nfa))))
(echo (concat "minimal DFA states: " (to-string (nfa:number-of-states minimal))))
`,
                params: { n: "6" },
            },
            {
                title: "Words of length up to n, built in a loop",
                script: `(mata-script "0.1.0")
(set-option :alphabet "bytes")
(set-option :loops-and-recursion true)
(declare-param n Int 3)

(define letter (nfa:create-from-regex "a|b"))
(define words
  (loop ((i 0) (shorter (nfa:create-from-regex "")))
    (if (< i n)
        (recur (+ i 1) (nfa:union-nondeterministic shorter (nfa:concatenate shorter letter)))
        shorter)))
(check "same as the regex"
  (nfa:equivalent? words (nfa:create-from-regex (concat "(a|b){0," (to-string n) "}"))))
(print (nfa:print-to-dot (nfa:minimize (nfa:determinize words))))
`,
            },
        ],
    },
];
