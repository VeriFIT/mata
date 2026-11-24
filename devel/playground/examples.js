// Examples of the playground. Each sets automata A and B, the operation, and the word.

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
