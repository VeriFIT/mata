//! Property tests for transducers: every operation is checked against a direct simulation of a small
//! random transducer, on all tuples of words of length at most `MAX_LEN` over the symbols
//! `0..NUM_SYMBOLS`.

use std::collections::{BTreeSet, VecDeque};
use std::fmt::Write as _;

use mata::nft::{DONT_CARE, Nft, SyncLevel};
use mata::{Alphabet, Nfa, State, Symbol};
use proptest::prelude::*;
use proptest::test_runner::FileFailurePersistence;

const NUM_SYMBOLS: u32 = 2;
const MAX_STATES: usize = 4;
const MAX_LEN: usize = 2;

/// A symbol of one tape in a transducer transition.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Read {
    Symbol(u32),
    Epsilon,
    Any,
}

impl Read {
    fn symbol(self) -> Symbol {
        match self {
            Read::Symbol(value) => Symbol::new(value),
            Read::Epsilon => Symbol::EPSILON,
            Read::Any => DONT_CARE,
        }
    }
}

/// A small transducer in plain Rust: the oracle.
#[derive(Clone, Debug)]
struct Model {
    num_levels: usize,
    num_states: usize,
    transitions: Vec<(usize, Vec<Read>, usize)>,
    /// Jumps: one edge from a level-0 state to a level-0 state that reads its symbol on every tape.
    jumps: Vec<(usize, u32, usize)>,
    /// Dead ends: a chain of edges from a level-0 state that stops before it gets back to level 0. They
    /// accept nothing.
    dead_ends: Vec<(usize, Vec<Read>)>,
    initial: Vec<usize>,
    accepting: Vec<usize>,
}

impl Model {
    /// The transitions together with the jumps, written as ordinary transitions.
    fn all_transitions(&self) -> Vec<(usize, Vec<Read>, usize)> {
        let jumps = self.jumps.iter().map(|&(source, symbol, target)| {
            (source, vec![Read::Symbol(symbol); self.num_levels], target)
        });
        self.transitions.iter().cloned().chain(jumps).collect()
    }

    /// The `.mata` text of the transducer, with the inner states of each transition spelled out and each
    /// jump as a single edge.
    fn to_mata(&self) -> String {
        let name = |read: Read| match read {
            Read::Symbol(s) => s.to_string(),
            Read::Epsilon => "<eps>".to_owned(),
            Read::Any => "<dntcr>".to_owned(),
        };
        let mut levels: Vec<String> = (0..self.num_states).map(|q| format!("q{q}:0")).collect();
        let mut edges = Vec::new();
        let mut next = self.num_states;
        for (source, reads, target) in &self.transitions {
            let mut from = *source;
            for (level, read) in reads.iter().enumerate() {
                let to = if level + 1 == reads.len() {
                    *target
                } else {
                    levels.push(format!("q{next}:{}", level + 1));
                    next += 1;
                    next - 1
                };
                edges.push(format!("q{from} {} q{to}", name(*read)));
                from = to;
            }
        }
        for (source, symbol, target) in &self.jumps {
            edges.push(format!("q{source} {symbol} q{target}"));
        }
        for (source, reads) in &self.dead_ends {
            let mut from = *source;
            for (level, read) in reads.iter().enumerate() {
                levels.push(format!("q{next}:{}", level + 1));
                edges.push(format!("q{from} {} q{next}", name(*read)));
                from = next;
                next += 1;
            }
        }
        let list = |states: &[usize]| {
            states.iter().fold(String::new(), |mut list, state| {
                write!(list, " q{state}").expect("writing to a String cannot fail");
                list
            })
        };
        format!(
            "@NFT-explicit\n%Alphabet-auto\n%Initial{}\n%Final{}\n%Levels {}\n%LevelsNum {}\n{}\n",
            list(&self.initial),
            list(&self.accepting),
            levels.join(" "),
            self.num_levels,
            edges.join("\n")
        )
    }

    /// Whether the tuple `words` is in the relation: a breadth-first search over (state, positions).
    fn accepts(&self, words: &[Vec<u32>]) -> bool {
        let mut seen = BTreeSet::new();
        let mut queue: VecDeque<(usize, Vec<usize>)> = self
            .initial
            .iter()
            .map(|&state| (state, vec![0; self.num_levels]))
            .collect();
        while let Some((state, positions)) = queue.pop_front() {
            if !seen.insert((state, positions.clone())) {
                continue;
            }
            if self.accepting.contains(&state)
                && positions.iter().zip(words).all(|(&p, w)| p == w.len())
            {
                return true;
            }
            for (source, reads, target) in &self.all_transitions() {
                if *source != state {
                    continue;
                }
                let mut next = positions.clone();
                let fits = reads.iter().enumerate().all(|(tape, read)| match read {
                    Read::Epsilon => true,
                    Read::Symbol(s) => {
                        let ok = words[tape].get(positions[tape]) == Some(s);
                        next[tape] += 1;
                        ok
                    }
                    Read::Any => {
                        let ok = positions[tape] < words[tape].len();
                        next[tape] += 1;
                        ok
                    }
                });
                if fits {
                    queue.push_back((*target, next));
                }
            }
        }
        false
    }

    /// Whether some tuple is accepted whose tape `i` is `words[i]`, or anything where `words[i]` is `None`.
    fn accepts_projected(&self, words: &[Option<Vec<u32>>]) -> bool {
        let mut seen = BTreeSet::new();
        let mut queue: VecDeque<(usize, Vec<usize>)> = self
            .initial
            .iter()
            .map(|&state| (state, vec![0; self.num_levels]))
            .collect();
        while let Some((state, positions)) = queue.pop_front() {
            if !seen.insert((state, positions.clone())) {
                continue;
            }
            let done = positions
                .iter()
                .zip(words)
                .all(|(&p, w)| w.as_ref().is_none_or(|w| p == w.len()));
            if self.accepting.contains(&state) && done {
                return true;
            }
            for (source, reads, target) in &self.all_transitions() {
                if *source != state {
                    continue;
                }
                let mut next = positions.clone();
                let fits = reads.iter().enumerate().all(|(tape, read)| {
                    let Some(word) = &words[tape] else {
                        return true;
                    };
                    match read {
                        Read::Epsilon => true,
                        Read::Symbol(s) => {
                            next[tape] += 1;
                            word.get(positions[tape]) == Some(s)
                        }
                        Read::Any => {
                            next[tape] += 1;
                            positions[tape] < word.len()
                        }
                    }
                });
                if fits {
                    queue.push_back((*target, next));
                }
            }
        }
        false
    }

    fn to_nft(&self) -> Nft {
        if !self.jumps.is_empty() || !self.dead_ends.is_empty() {
            let text = self.to_mata();
            return text
                .parse()
                .unwrap_or_else(|error| panic!("{error}\n{text}"));
        }
        let mut nft = Nft::new(self.num_levels).unwrap();
        let states: Vec<State> = (0..self.num_states).map(|_| nft.add_state()).collect();
        for &state in &self.initial {
            nft.add_initial(states[state]).unwrap();
        }
        for &state in &self.accepting {
            nft.add_final(states[state]).unwrap();
        }
        for (source, reads, target) in &self.transitions {
            let symbols: Vec<Symbol> = reads.iter().map(|read| read.symbol()).collect();
            nft.add_transition(states[*source], &symbols, states[*target])
                .unwrap();
        }
        nft
    }
}

/// Whether (`input`, `output`) is in the composition of the two-level relations `left` and `right`: some
/// middle word `middle` has (`input`, `middle`) in `left` and (`middle`, `output`) in `right`. A search
/// over (state of `left`, state of `right`, position in `input`, position in `output`): steps that read
/// nothing of the middle word move alone, the others move together.
fn composed_accepts(left: &Model, right: &Model, input: &[u32], output: &[u32]) -> bool {
    let matches = |left_read: Read, right_read: Read| match (left_read, right_read) {
        (Read::Symbol(left_symbol), Read::Symbol(right_symbol)) => left_symbol == right_symbol,
        (Read::Epsilon, _) | (_, Read::Epsilon) => false,
        _ => true,
    };
    let reads = |read: Read, word: &[u32], position: usize| match read {
        Read::Epsilon => Some(position),
        Read::Symbol(symbol) => (word.get(position) == Some(&symbol)).then_some(position + 1),
        Read::Any => (position < word.len()).then_some(position + 1),
    };
    let left_transitions = left.all_transitions();
    let right_transitions = right.all_transitions();
    let mut seen = BTreeSet::new();
    let mut queue: VecDeque<(usize, usize, usize, usize)> = VecDeque::new();
    for &left_state in &left.initial {
        for &right_state in &right.initial {
            queue.push_back((left_state, right_state, 0, 0));
        }
    }
    while let Some(config @ (left_state, right_state, in_pos, out_pos)) = queue.pop_front() {
        if !seen.insert(config) {
            continue;
        }
        if left.accepting.contains(&left_state)
            && right.accepting.contains(&right_state)
            && in_pos == input.len()
            && out_pos == output.len()
        {
            return true;
        }
        for (source, left_reads, target) in &left_transitions {
            if *source != left_state {
                continue;
            }
            let Some(next_in) = reads(left_reads[0], input, in_pos) else {
                continue;
            };
            if left_reads[1] == Read::Epsilon {
                queue.push_back((*target, right_state, next_in, out_pos));
                continue;
            }
            for (right_source, right_reads, right_target) in &right_transitions {
                if *right_source != right_state || !matches(left_reads[1], right_reads[0]) {
                    continue;
                }
                if let Some(next_out) = reads(right_reads[1], output, out_pos) {
                    queue.push_back((*target, *right_target, next_in, next_out));
                }
            }
        }
        for (source, right_reads, target) in &right_transitions {
            if *source == right_state
                && right_reads[0] == Read::Epsilon
                && let Some(next_out) = reads(right_reads[1], output, out_pos)
            {
                queue.push_back((left_state, *target, in_pos, next_out));
            }
        }
    }
    false
}

fn read(epsilon: bool, any: bool) -> BoxedStrategy<Read> {
    let symbol = (0..NUM_SYMBOLS).prop_map(Read::Symbol);
    match (epsilon, any) {
        (false, false) => symbol.boxed(),
        (true, false) => prop_oneof![4 => symbol, 1 => Just(Read::Epsilon)].boxed(),
        (false, true) => prop_oneof![6 => symbol, 1 => Just(Read::Any)].boxed(),
        (true, true) => {
            prop_oneof![6 => symbol, 2 => Just(Read::Epsilon), 1 => Just(Read::Any)].boxed()
        }
    }
}

fn model_with(
    levels: usize,
    epsilon: bool,
    any: bool,
    jumps: bool,
) -> impl Strategy<Value = Model> {
    (1..=MAX_STATES).prop_flat_map(move |n| {
        let states: Vec<usize> = (0..n).collect();
        let max_jumps = if jumps { n } else { 0 };
        // A dead end stops before the last level, so it needs two levels.
        let max_dead_ends = if levels > 1 { 2 } else { 0 };
        (
            proptest::collection::vec(
                (
                    0..n,
                    proptest::collection::vec(read(epsilon, any), levels),
                    0..n,
                ),
                0..=2 * n + 1,
            ),
            proptest::collection::vec((0..n, 0..NUM_SYMBOLS, 0..n), 0..=max_jumps),
            proptest::collection::vec(
                (
                    0..n,
                    proptest::collection::vec(read(epsilon, any), 1..levels.max(2)),
                ),
                0..=max_dead_ends,
            ),
            proptest::sample::subsequence(states.clone(), 1..=n),
            proptest::sample::subsequence(states, 0..=n),
        )
            .prop_map(
                move |(transitions, jumps, dead_ends, initial, accepting)| Model {
                    num_levels: levels,
                    num_states: n,
                    transitions,
                    jumps,
                    dead_ends,
                    initial,
                    accepting,
                },
            )
    })
}

/// Transducers with epsilon, `DONT_CARE` and (sometimes) jumps.
fn model(levels: usize) -> impl Strategy<Value = Model> {
    prop_oneof![
        model_with(levels, true, true, false),
        model_with(levels, true, true, true)
    ]
}

fn words() -> Vec<Vec<u32>> {
    let mut all = vec![vec![]];
    let mut layer = vec![vec![]];
    for _ in 0..MAX_LEN {
        layer = layer
            .iter()
            .flat_map(|w: &Vec<u32>| {
                (0..NUM_SYMBOLS).map(move |s| {
                    let mut longer = w.clone();
                    longer.push(s);
                    longer
                })
            })
            .collect();
        all.extend(layer.iter().cloned());
    }
    all
}

/// Every tuple of `levels` words of length at most `MAX_LEN`.
fn tuples(levels: usize) -> Vec<Vec<Vec<u32>>> {
    let mut all = vec![vec![]];
    for _ in 0..levels {
        all = all
            .into_iter()
            .flat_map(|prefix: Vec<Vec<u32>>| {
                words().into_iter().map(move |w| {
                    let mut longer = prefix.clone();
                    longer.push(w);
                    longer
                })
            })
            .collect();
    }
    all
}

fn symbols(words: &[Vec<u32>]) -> Vec<Vec<Symbol>> {
    words
        .iter()
        .map(|w| w.iter().copied().map(Symbol::new).collect())
        .collect()
}

/// Asserts the invariants of the transducers this crate hands out: levels in range, initial and final
/// states on level 0, and every edge moving to the next level (wrapping to level 0), so no jumps.
fn assert_well_formed(nft: &Nft, what: &str) {
    let n = nft.num_levels();
    for state in nft.states() {
        assert!(
            nft.level(state).is_some_and(|level| level < n),
            "{what}: level out of range\n{nft:?}"
        );
    }
    for &state in nft.initial_states().iter().chain(nft.final_states()) {
        assert_eq!(
            nft.level(state),
            Some(0),
            "{what}: initial or final state off level 0\n{nft:?}"
        );
    }
    for edge in nft.edges() {
        let (from, to) = (
            nft.level(edge.source).unwrap(),
            nft.level(edge.target).unwrap(),
        );
        assert_eq!(
            to,
            (from + 1) % n,
            "{what}: edge {edge} skips levels\n{nft:?}"
        );
    }
}

/// Asserts that `nft` accepts exactly the tuples (up to `MAX_LEN`) for which `expected` holds.
fn assert_relation(nft: &Nft, expected: impl Fn(&[Vec<u32>]) -> bool, what: &str) {
    assert_well_formed(nft, what);
    for tuple in tuples(nft.num_levels()) {
        assert_eq!(
            nft.accepts(&symbols(&tuple)),
            expected(&tuple),
            "{what} on {tuple:?}\n{nft:?}"
        );
    }
}

/// Asserts that every tuple (up to `MAX_LEN`) that `nft` accepts satisfies `expected`.
fn assert_relation_within(nft: &Nft, expected: impl Fn(&[Vec<u32>]) -> bool, what: &str) {
    assert_well_formed(nft, what);
    for tuple in tuples(nft.num_levels()) {
        if nft.accepts(&symbols(&tuple)) {
            assert!(expected(&tuple), "{what}: unexpected {tuple:?}\n{nft:?}");
        }
    }
}

fn sigma() -> Alphabet {
    Alphabet::from_symbols((0..NUM_SYMBOLS).map(Symbol::new)).unwrap()
}

/// 128 cases, or `PROPTEST_CASES` when set.
fn config() -> ProptestConfig {
    let cases = std::env::var("PROPTEST_CASES")
        .ok()
        .and_then(|cases| cases.parse().ok())
        .unwrap_or(128);
    ProptestConfig {
        // Next to this file, as `<file>.proptest-regressions` (the default looks for `src/lib.rs`).
        failure_persistence: Some(Box::new(FileFailurePersistence::WithSource(
            "proptest-regressions",
        ))),
        ..ProptestConfig::with_cases(cases)
    }
}

proptest! {
    #![proptest_config(config())]

    #[test]
    fn membership(m in (1usize..=3).prop_flat_map(model)) {
        let nft = m.to_nft();
        prop_assert_eq!(nft.num_levels(), m.num_levels);
        assert_relation(&nft, |t| m.accepts(t), "membership");
        let mut trimmed = nft.clone();
        trimmed.trim();
        assert_relation(&trimmed, |t| m.accepts(t), "trim");
        prop_assert!(nft.is_equivalent_to(&nft.clone())?);
    }

    #[test]
    fn union_and_concat(
        (a, b) in (1usize..=2).prop_flat_map(|levels| (model(levels), model(levels)))
    ) {
        let (x, y) = (a.to_nft(), b.to_nft());
        let union = x.union(&y)?;
        assert_relation(&union, |t| a.accepts(t) || b.accepts(t), "union");
        prop_assert!(x.is_included_in(&union)?);
        assert_relation(&x.concat(&y)?, |t| {
            // Some split of every tape into a prefix for `a` and a suffix for `b`.
            let splits: Vec<Vec<usize>> = t.iter().map(|w| (0..=w.len()).collect()).collect();
            let mut choice = vec![0; t.len()];
            loop {
                let (left, right): (Vec<Vec<u32>>, Vec<Vec<u32>>) = t
                    .iter()
                    .zip(&choice)
                    .map(|(w, &i)| (w[..i].to_vec(), w[i..].to_vec()))
                    .unzip();
                if a.accepts(&left) && b.accepts(&right) {
                    return true;
                }
                let mut tape = 0;
                loop {
                    if tape == t.len() {
                        return false;
                    }
                    choice[tape] += 1;
                    if choice[tape] < splits[tape].len() {
                        break;
                    }
                    choice[tape] = 0;
                    tape += 1;
                }
            }
        }, "concat");
    }

    #[test]
    fn intersection(
        (a, b) in (1usize..=2).prop_flat_map(|levels| (model(levels), model(levels)))
    ) {
        let (x, y) = (a.to_nft(), b.to_nft());
        let both = x.intersection(&y)?;
        // The product synchronizes the encodings, so in general it accepts a subset of the intersection.
        assert_relation_within(&both, |t| a.accepts(t) && b.accepts(t), "intersection");
        prop_assert!(both.is_included_in(&x)? && both.is_included_in(&y)?);
    }

    #[test]
    fn intersection_of_length_preserving(
        (a, b) in (1usize..=2).prop_flat_map(|levels| (model_with(levels, false, false, false), model_with(levels, false, false, false)))
    ) {
        let (x, y) = (a.to_nft(), b.to_nft());
        assert_relation(&x.intersection(&y)?, |t| a.accepts(t) && b.accepts(t), "intersection");
    }

    #[test]
    fn inclusion_of_length_preserving(
        (a, b) in (1usize..=2).prop_flat_map(|levels| (model_with(levels, false, true, true), model_with(levels, false, true, true)))
    ) {
        // Without epsilon, inclusion of the relations is inclusion of the encodings, with DONT_CARE spelled
        // out as the used symbols and one unused symbol.
        let (x, y) = (a.to_nft(), b.to_nft());
        let symbols = Alphabet::from_symbols((0..=NUM_SYMBOLS).map(Symbol::new))?;
        let (x_nfa, y_nfa) = (x.to_nfa_over(&symbols), y.to_nfa_over(&symbols));
        let included = x.is_included_in(&y)?;
        prop_assert_eq!(included, x_nfa.is_included_in(&y_nfa));
        prop_assert_eq!(x.is_equivalent_to(&y)?, x_nfa.is_equivalent_to(&y_nfa));
        if included {
            for tuple in tuples(a.num_levels) {
                prop_assert!(!a.accepts(&tuple) || b.accepts(&tuple), "{:?}", tuple);
            }
        }
    }

    #[test]
    fn invert_levels(m in (1usize..=3).prop_flat_map(model)) {
        let nft = m.to_nft();
        let inverted = nft.invert_levels();
        assert_relation(&inverted, |t| {
            let reversed: Vec<Vec<u32>> = t.iter().rev().cloned().collect();
            m.accepts(&reversed)
        }, "invert_levels");
        prop_assert!(inverted.invert_levels().is_equivalent_to(&nft)?);
    }

    #[test]
    fn compose(a in model(2), b in model(2)) {
        let (x, y) = (a.to_nft(), b.to_nft());
        let composed = x.compose(&y)?;
        prop_assert_eq!(composed.num_levels(), 2);
        assert_relation(&composed, |t| composed_accepts(&a, &b, &t[0], &t[1]), "compose");
        let kept = x.compose_on(&y, 1, 0, SyncLevel::Keep)?;
        prop_assert_eq!(kept.num_levels(), 3);
        assert_relation(&kept, |t| {
            a.accepts(&[t[0].clone(), t[1].clone()]) && b.accepts(&[t[1].clone(), t[2].clone()])
        }, "compose_on (keep)");
    }

    #[test]
    fn apply_and_project(m in model(2)) {
        let nft = m.to_nft();
        // All words over the symbols 0 and 1.
        let mut sigma_star = Nfa::with_states(1);
        let q = State::new(0);
        sigma_star.add_initial(q)?;
        sigma_star.add_final(q)?;
        for s in 0..NUM_SYMBOLS {
            sigma_star.add_transition(q, Symbol::new(s), q)?;
        }
        let image = nft.apply(&sigma_star, 0)?;
        prop_assert_eq!(image.num_levels(), 1);
        let preimage = nft.apply(&sigma_star, 1)?;
        let projected = nft.project_out(&[0])?;
        prop_assert_eq!(projected.num_levels(), 1);
        let kept = nft.project_to(&[1])?;
        for v in words() {
            let tuple = symbols(std::slice::from_ref(&v));
            let on_tape_1 = m.accepts_projected(&[None, Some(v.clone())]);
            let on_tape_0 = m.accepts_projected(&[Some(v.clone()), None]);
            prop_assert_eq!(image.accepts(&tuple), on_tape_1, "apply on {:?}", v);
            prop_assert_eq!(projected.accepts(&tuple), on_tape_1, "project_out on {:?}", v);
            prop_assert_eq!(kept.accepts(&tuple), on_tape_1, "project_to on {:?}", v);
            prop_assert_eq!(preimage.accepts(&tuple), on_tape_0, "apply on level 1 on {:?}", v);
            prop_assert_eq!(image.to_nfa_over(&sigma()).accepts(&tuple[0]), on_tape_1, "to_nfa_over on {:?}", v);
        }
        let restricted = nft.restrict(&sigma_star, 0)?;
        assert_relation(&restricted, |t| m.accepts(t), "restrict to all words");
    }

    #[test]
    fn identity_and_empty_word(levels in 1usize..=3) {
        let mut nfa = Nfa::with_states(2);
        let (p, q) = (State::new(0), State::new(1));
        nfa.add_initial(p)?;
        nfa.add_final(q)?;
        nfa.add_transition(p, Symbol::new(0), q)?;
        nfa.add_transition(q, Symbol::new(1), q)?;
        let identity = Nft::identity(&nfa, levels)?;
        assert_relation(&identity, |t| t.iter().all(|w| w == &t[0]) && nfa.accepts(&symbols(&t[..1])[0]), "identity");
        let empty = Nft::empty_word(levels)?;
        assert_relation(&empty, |t| t.iter().all(Vec::is_empty), "empty_word");
    }

    #[test]
    fn text_round_trip(m in (1usize..=3).prop_flat_map(model)) {
        let nft = m.to_nft();
        let text = nft.to_string();
        let parsed: Nft = text.parse()?;
        prop_assert_eq!(parsed.num_levels(), nft.num_levels());
        assert_relation(&parsed, |t| m.accepts(t), &format!("parsed\n{text}"));
    }

    #[test]
    fn to_nfa_interleaves(m in model_with(2, false, false, true)) {
        let nft = m.to_nft();
        let nfa = nft.to_nfa();
        for t in tuples(2) {
            if t[0].len() != t[1].len() {
                continue;
            }
            let interleaved: Vec<Symbol> = t[0].iter().zip(&t[1]).flat_map(|(&x, &y)| [Symbol::new(x), Symbol::new(y)]).collect();
            prop_assert_eq!(nfa.accepts(&interleaved), m.accepts(&t), "{:?}", t);
        }
    }
}

proptest! {
    #![proptest_config(config())]

    #[test]
    fn determinize_remove_epsilon_reverse(m in (1usize..=3).prop_flat_map(model)) {
        let nft = m.to_nft();
        assert_relation(&nft.determinize(), |t| m.accepts(t), "determinize");
        let mut without_epsilon = nft.clone();
        without_epsilon.remove_epsilon();
        assert_relation(&without_epsilon, |t| m.accepts(t), "remove_epsilon");
        assert_relation(&nft.reverse(), |t| {
            let reversed: Vec<Vec<u32>> = t.iter().map(|w| w.iter().rev().copied().collect()).collect();
            m.accepts(&reversed)
        }, "reverse");
    }
}
