//! The transition iterator shared by automata and transducers.

use std::fmt;
use std::iter::FusedIterator;
use std::ops::Range;

use mata_sys::ffi;

use crate::types::states_from_raw;
use crate::{State, Symbol, Transition, check};

/// Iterator over the transitions of an automaton, ordered by source, then symbol, then target.
///
/// Returned by [`Nfa::transitions`](crate::Nfa::transitions) and
/// [`Nfa::transitions_from`](crate::Nfa::transitions_from), and by the transducer counterparts.
#[derive(Clone)]
pub struct Transitions<'a> {
    automaton: &'a ffi::Automaton,
    /// The source states still to visit; the current one is `sources.start - 1`.
    sources: Range<u32>,
    /// The symbol posts of the current source still to visit.
    posts: Range<usize>,
    symbol: Symbol,
    targets: std::slice::Iter<'a, State>,
}

impl<'a> Transitions<'a> {
    pub(crate) fn new(automaton: &'a ffi::Automaton, sources: Range<u32>) -> Transitions<'a> {
        Transitions {
            automaton,
            sources,
            posts: 0..0,
            symbol: Symbol::new(0),
            targets: [].iter(),
        }
    }
}

impl Iterator for Transitions<'_> {
    type Item = Transition;

    fn next(&mut self) -> Option<Transition> {
        loop {
            if let Some(&target) = self.targets.next() {
                let source = State::new(self.sources.start - 1);
                return Some(Transition {
                    source,
                    symbol: self.symbol,
                    target,
                });
            }
            if let Some(post) = self.posts.next() {
                let (automaton, source) = (self.automaton, self.sources.start - 1);
                self.symbol = Symbol::new(check(ffi::symbol_post_symbol(automaton, source, post)));
                self.targets =
                    states_from_raw(check(ffi::symbol_post_targets(automaton, source, post)))
                        .iter();
                continue;
            }
            let source = self.sources.next()?;
            self.posts = 0..ffi::num_symbol_posts(self.automaton, source);
        }
    }
}

impl FusedIterator for Transitions<'_> {}

// SAFETY: The iterator only reads the borrowed automaton, through `const` Mata calls. `Nfa` and `Nft` are
// `Sync` (see the safety comments there), so the automaton may be read from any thread while it is borrowed.
unsafe impl Send for Transitions<'_> {}

// SAFETY: Through `&Transitions`, only `Clone` and `Debug` are reachable, which read the same shared borrow.
unsafe impl Sync for Transitions<'_> {}

impl fmt::Debug for Transitions<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Transitions")
            .field("sources", &self.sources)
            .finish_non_exhaustive()
    }
}
