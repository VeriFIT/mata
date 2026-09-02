from collections.abc import Iterable
from enum import IntEnum
from typing import Self

import libmata.alphabets as alph
import libmata.nfa.nfa as mata_nfa
from libmata.alphabets import Level, Symbol
from libmata.nfa.nfa import Run, State, Transition
from libmata.utils import BinaryRelation

DEFAULT_NUM_OF_LEVELS: int

def epsilon() -> Symbol: ...
def dont_care() -> Symbol: ...

class JumpMode(IntEnum):
    """Specifies how a jump transition (a transition with a length greater than 1) is interpreted."""

    RepeatSymbol = 0
    AppendDontCares = 1
    NoJump = 2

class CompositionMode(IntEnum):
    """Mode of composition to use for `compose()`."""

    General = 0
    FastNoJump = 1
    Auto = 2

class Levels:
    """Wrapper over the per-state levels of an NFT (`mata::nft::Levels`)."""
    def __init__(
        self,
        num_of_levels: int | None = None,
        levels: Self | list[Level] | None = None,
        count: int | None = None,
        value: Level = 0,
    ) -> None: ...
    @property
    def num_of_levels(self) -> int: ...
    @num_of_levels.setter
    def num_of_levels(self, value: int) -> None: ...
    def __len__(self) -> int: ...
    def __getitem__(self, state: State) -> Level: ...
    def __setitem__(self, state: State, level: Level) -> None: ...
    def __iter__(self): ...
    def __eq__(self, other: object) -> bool: ...
    def append(self, other: Self) -> None:
        """Append `other`'s levels to the end of `self`."""
    def set(self, state_or_levels: State | list[Level], level: Level = 0) -> Self:
        """Set the level of a single state, or replace all levels with `state_or_levels`."""
    def count(self, level: Level) -> int:
        """Count the number of states with `level`."""
    def get_levels_of(self, states: Iterable[State]) -> list[Level]:
        """Get levels of the states in `states`."""
    def map_levels_to(self, states: Iterable[State]) -> list[set[State]]:
        """Get a mapping of levels to sets of states from `states`."""
    def next_level_after(self, level: Level) -> Level:
        """Get the next level that should follow after `level`."""
    def get_minimal_level_of(self, states: Iterable[State]) -> Level | None:
        """Get the minimal level (0 < 1 < ... < num_of_levels-1) of the states in `states`."""
    def get_minimal_next_level_of(self, states: Iterable[State]) -> Level | None:
        """Get the minimal next level (1 < 2 < ... < num_of_levels-1 < 0) of the states in `states`."""
    @staticmethod
    def can_follow(source_level: Level, target_level: Level) -> bool:
        """Check whether a transition can be made from a state with `source_level` to a state with `target_level`."""
    def can_follow_for_states(self, source: State, target: State) -> bool:
        """Check whether a transition can be made from `source` to `target`."""
    def num_of_levels_used(self) -> int | None:
        """Get the minimal number of levels that can accommodate all currently stored levels."""

class Nft:
    """Wrapper over NFT (non-deterministic finite transducer)."""
    def __init__(
        self,
        state_number: int = 0,
        num_of_levels: int = DEFAULT_NUM_OF_LEVELS,
        label=None,
    ) -> None: ...
    @classmethod
    def with_levels(
        cls,
        levels: int | Levels,
        num_of_states: int = 0,
        alphabets: alph.AlphabetLevels | None = None,
    ) -> Self:
        """Construct a new explicit NFT with `num_of_states` states, using `levels`."""
    @classmethod
    def from_nfa(cls, nfa: mata_nfa.Nfa, num_of_levels: int = 1, default_level: Level = 0) -> Self:
        """Construct a new NFT with `num_of_levels` levels from `nfa`."""
    @classmethod
    def from_nfa_with_levels(cls, nfa: mata_nfa.Nfa, levels: Levels) -> Self:
        """Construct a new NFT from `nfa`, using `levels` for the states of `nfa`."""
    def deepcopy(self) -> Self:
        """Return a deep copy of the NFT."""
    @property
    def label(self): ...
    @label.setter
    def label(self, value) -> None: ...
    @property
    def levels(self) -> Levels:
        """A snapshot copy of the per-state levels. Assign back to update the NFT's levels."""
    @levels.setter
    def levels(self, value: Levels) -> None: ...
    @property
    def alphabets(self) -> alph.AlphabetLevels | None:
        """The per-level alphabets of the NFT, or `None` if none is set."""
    @alphabets.setter
    def alphabets(self, value: alph.AlphabetLevels | None) -> None: ...
    @property
    def initial_states(self) -> list[State]: ...
    @initial_states.setter
    def initial_states(self, value: list[State]) -> None: ...
    @property
    def final_states(self) -> list[State]: ...
    @final_states.setter
    def final_states(self, value: list[State]) -> None: ...
    def is_state(self, state: State) -> bool: ...
    def add_new_state(self) -> State:
        """Add a new (fresh) state to the automaton."""
    def add_state(self, state: State) -> State:
        """Add `state` to the automaton if not already present."""
    def add_new_state_with_level(self, level: Level) -> State:
        """Add a new (fresh) state to the automaton with `level`."""
    def add_state_with_level(self, state: State, level: Level) -> State:
        """Add `state` to the automaton with `level` if not already present."""
    def num_of_states_with_level(self, level: Level) -> int:
        """Get the number of states with `level`."""
    def make_initial_state(self, state: State) -> None: ...
    def make_initial_states(self, states: list[State]) -> None: ...
    def has_initial_state(self, state: State) -> bool: ...
    def remove_initial_state(self, state: State) -> None: ...
    def clear_initial(self) -> None: ...
    def make_final_state(self, state: State) -> None: ...
    def make_final_states(self, states: list[State]) -> None: ...
    def has_final_state(self, state: State) -> bool: ...
    def remove_final_state(self, state: State) -> None: ...
    def clear_final(self) -> None: ...
    @property
    def delta(self) -> mata_nfa.Delta:
        """The transition relation of the automaton.

        Returns a live view over the automaton's transitions, allowing operations similar to the C++ interface,
        e.g. `nft.delta.add(source, symbol, target)`, `nft.delta.contains(...)`, or iterating `for t in nft.delta`.
        """
    def add_transition_object(self, tr: Transition) -> None: ...
    def add_transition(self, source: State, symbols: Symbol | list[Symbol], target: State | None = None) -> State:
        """Add a single NFT transition creating new inner states for all tapes."""
    def add_transition_with_length(
        self,
        source: State,
        symbol: Symbol,
        length: int,
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
    ) -> State:
        """Add a single NFT transition with `length`, creating a new target state."""
    def add_transition_with_target(
        self,
        source: State,
        symbol: Symbol,
        target: State,
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
    ) -> None:
        """Add a single NFT transition from `source` to `target`."""
    def add_transition_with_same_level_targets(
        self,
        source: State,
        symbol: Symbol,
        targets: Iterable[State],
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
    ) -> None:
        """Add a NFT transition from `source` to a set of `targets`, sharing the common prefix of the transition."""
    def remove_trans(self, tr: Transition) -> None: ...
    def remove_trans_raw(self, source: State, symbol: Symbol, target: State) -> None: ...
    def has_transition(self, source: State, symbol: Symbol, target: State) -> bool: ...
    def get_num_of_transitions(self) -> int: ...
    def clear(self) -> None: ...
    def num_of_states(self) -> int: ...
    def iterate(self):
        """Iterates over all transitions."""
    def get_trans_as_sequence(self) -> list[Transition]:
        """Get automaton transitions as a sequence."""
    def get_used_symbols(self) -> set[Symbol]:
        """Return a set of symbols used on the transitions in NFT."""
    def is_identical(self, other: Self) -> bool: ...
    def is_lang_empty(self, run: Run | None = None) -> bool: ...
    def is_deterministic(self) -> bool: ...
    def is_complete(self, alphabet: alph.Alphabet | None = None) -> bool: ...
    def insert_word(self, source: State, word: list[Symbol], target: State | None = None) -> State:
        """Insert `word` into the NFT from `source` to `target`, creating new states along the path."""
    def insert_word_by_levels(
        self,
        source: State,
        word_parts_on_levels: list[list[Symbol]],
        target: State | None = None,
    ) -> State:
        """Insert a word interleaved from `word_parts_on_levels` (one part per level) from `source` to `target`."""
    def insert_identity(
        self,
        state: State,
        symbols_or_alphabet: Symbol | list[Symbol] | alph.Alphabet | None = None,
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
    ) -> Self:
        """Insert identity transition(s) at `state`."""
    def contains_jump_transitions(self) -> bool:
        """Check if the transducer contains any jump transition."""
    def trim(self) -> Self:
        """Remove inaccessible and not co-accessible states."""
    def trim_with_state_map(self) -> tuple[Self, dict[State, State]]:
        """Remove inaccessible and not co-accessible states."""
    def remove_epsilon_inplace(self, epsilon: Symbol = ...) -> None:
        """Remove simple epsilon transitions in place."""
    def concatenate(self, other: Self) -> Self:
        """Concatenate `self` with `other` in-place."""
    def union(self, other: Self) -> Self:
        """Make a non-deterministic union of `self` with `other` in-place."""
    def get_one_letter_aut(
        self,
        levels_to_keep: Iterable[Level] | None = None,
        abstract_symbol: Symbol = ...,
    ) -> Self:
        """Get an NFT where transitions of `self` are replaced with transitions over one symbol."""
    def unwind_jumps_inplace(
        self,
        dont_care_symbol_replacements: Iterable[Symbol] | None = None,
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
    ) -> None:
        """Unwind jump transitions in place."""
    def unwind_jumps(
        self,
        dont_care_symbol_replacements: Iterable[Symbol] | None = None,
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
    ) -> Self:
        """Create a transducer with unwound jump transitions from `self`."""
    def is_epsilon(self, symbol: Symbol) -> bool: ...
    def to_dot_file(
        self,
        output_file: str = "nft.dot",
        output_format: str = "pdf",
        decode_ascii_chars: bool = False,
        use_intervals: bool = False,
        max_label_length: int = -1,
    ) -> None: ...
    def to_dot_str(
        self,
        encoding: str = "utf-8",
        decode_ascii_chars: bool = False,
        use_intervals: bool = False,
        max_label_length: int = -1,
    ) -> str: ...
    def to_mata_str(self, encoding: str = "utf-8") -> str:
        """Transforms the automaton to a mata-format string."""
    def to_mata_file(self, output_file: str = "nft.mata", encoding: str = "utf-8") -> None:
        """Writes the automaton to `output_file` in mata format."""
    def post_of(self, states: Iterable[State], symbol: Symbol, symbol_level: Level | None = None) -> set[State]:
        """Get the set of states reachable from `states` over `symbol` (optionally, on a given `symbol_level`)."""
    def is_universal(self, alphabet: alph.Alphabet, params: dict[str, str] | None = None) -> bool:
        """Tests if the NFT is universal with regard to the given alphabet."""
    def is_in_lang(
        self,
        run_or_word: Run | list[Symbol],
        match_prefix: bool = False,
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
        has_epsilon_cycles: bool = True,
    ) -> bool:
        """Tests if `run_or_word` (or its prefix, with `match_prefix`) is in the language of the NFT."""
    def is_in_lang_prefix(
        self,
        run_or_word: Run | list[Symbol],
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
        has_epsilon_cycles: bool = True,
    ) -> bool:
        """Tests if a prefix of `run_or_word` is in the language of the NFT."""
    def is_in_lang_by_levels(
        self,
        level_words: list[list[Symbol]],
        match_prefix: bool = False,
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
        has_epsilon_cycles: bool = True,
    ) -> bool:
        """Tests if the tuple `level_words` (one word per level) is in the regular relation accepted by the NFT."""
    def is_in_lang_prefix_by_levels(
        self,
        level_words: list[list[Symbol]],
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
        has_epsilon_cycles: bool = True,
    ) -> bool:
        """Tests if a prefix of the tuple `level_words` is in the regular relation accepted by the NFT."""
    def get_word_for_path(self, path: list[State]) -> tuple[list[Symbol], bool]:
        """For a given path (list of states) returns a corresponding word."""
    def mk_level_word_from_word(self, word: list[Symbol]) -> list[list[Symbol]]:
        """Convert `word` (interleaved representation) to level words according to the levels of the NFT."""
    def mk_word_from_level_word(self, level_words: list[list[Symbol]]) -> list[Symbol]:
        """Convert `level_words` to a word (interleaved representation) according to the levels of the NFT."""
    def get_words(
        self, max_length: int | None = None, jump_mode: JumpMode = JumpMode.RepeatSymbol
    ) -> set[tuple[Symbol, ...]]:
        """Get the set of all words in the language of the NFT whose length is <= `max_length`."""
    def apply(
        self,
        nfa_or_word: mata_nfa.Nfa | list[Symbol],
        level_to_apply_on: Level = 0,
        project_out_applied_level: bool = True,
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
    ) -> Self:
        """Apply `nfa_or_word` to `self`, intersecting it with `level_to_apply_on` of `self`."""
    def to_nfa_copy(self) -> mata_nfa.Nfa:
        """Copy the NFT as an NFA. Transitions are not updated to have only one level."""
    def to_nfa_move(self) -> mata_nfa.Nfa:
        """Move the NFT as an NFA. Transitions are not updated to have only one level."""
    def to_nfa_update_copy(
        self,
        dont_care_symbol_replacements: Iterable[Symbol] | None = None,
        jump_mode: JumpMode = JumpMode.RepeatSymbol,
    ) -> mata_nfa.Nfa:
        """Copy the NFT as an NFA, updating the transitions to have only one level."""
    def make_complete(
        self,
        alphabet_or_symbols: alph.Alphabet | set[Symbol] | None = None,
        sink_states: list[State] | None = None,
    ) -> bool:
        """Make the NFT complete in place."""
    def resolve_alphabet(self, alphabet: alph.Alphabet | None = None, level: Level | None = None) -> alph.Alphabet:
        """Resolve which alphabet to use for the current operation on `level`."""
    def get_symbols_to_work_with(
        self, alphabet: alph.Alphabet | None = None, level: Level | None = None
    ) -> set[Symbol]:
        """Get the set of symbols to work with for the current operation on `level`."""

# Operations
def determinize(lhs: Nft) -> Nft: ...
def determinize_with_subset_map(lhs: Nft) -> tuple[Nft, dict]: ...
def union_nondet(lhs: Nft, rhs: Nft) -> Nft:
    """Compute a non-deterministic union of `lhs` and `rhs`."""

def intersection(
    lhs: Nft,
    rhs: Nft,
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
    lhs_first_aux_state: State = ...,
    rhs_first_aux_state: State = ...,
) -> Nft:
    """Compute the intersection of `lhs` and `rhs`."""

def intersection_with_product_map(
    lhs: Nft,
    rhs: Nft,
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
    lhs_first_aux_state: State = ...,
    rhs_first_aux_state: State = ...,
) -> tuple[Nft, dict[tuple[State, State], State]]: ...
def compose(
    lhs: Nft,
    rhs: Nft,
    lhs_sync_levels: Level | Iterable[Level],
    rhs_sync_levels: Level | Iterable[Level],
    project_out_sync_levels: bool = True,
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
    composition_mode: CompositionMode = CompositionMode.Auto,
) -> Nft:
    """Compose `lhs` and `rhs` by aligning their synchronization levels."""

def compose_alphabets(
    lhs: Nft,
    rhs: Nft,
    lhs_sync_levels: Level | Iterable[Level],
    rhs_sync_levels: Level | Iterable[Level],
    project_out_sync_levels: bool = True,
) -> alph.AlphabetLevels | None:
    """Compose the per-level alphabets for NFTs to be composed via `compose`."""

def concatenate(lhs: Nft, rhs: Nft, use_epsilon: bool = False) -> Nft:
    """Concatenate two NFTs."""

def concatenate_with_result_state_maps(
    lhs: Nft, rhs: Nft, use_epsilon: bool = False
) -> tuple[Nft, dict[State, State], dict[State, State]]: ...
def concatenate_nth_power(nft: Nft, power: int) -> Nft:
    """Compute the NFT accepting the `power`-th power of the language of `nft`."""

def complement(nft: Nft, alphabet: alph.Alphabet, params: dict[str, str] | None = None) -> Nft:
    """Compute the complement of `nft`."""

def revert(lhs: Nft) -> Nft:
    """Reverse transitions in `lhs`."""

def invert_levels(aut: Nft, jump_mode: JumpMode = JumpMode.RepeatSymbol) -> Nft:
    """Invert the levels of `aut`."""

def remove_epsilon(lhs: Nft, epsilon: Symbol = ...) -> Nft:
    """Remove simple epsilon transitions from `lhs`."""

def reduce(aut: Nft, params: dict[str, str] | None = None) -> Nft:
    """Reduce the size of `aut`."""

def reduce_with_state_map(aut: Nft, params: dict[str, str] | None = None) -> tuple[Nft, dict[State, State]]: ...
def compute_relation(lhs: Nft, params: dict[str, str] | None = None) -> BinaryRelation:
    """Compute the relation for the NFT."""

def is_included_with_cex(
    smaller: Nft,
    bigger: Nft,
    alphabet: alph.Alphabet | None = None,
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
    params: dict[str, str] | None = None,
) -> tuple[bool, Run]:
    """Test inclusion between two NFTs."""

def is_included(
    smaller: Nft,
    bigger: Nft,
    alphabet: alph.Alphabet | None = None,
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
    params: dict[str, str] | None = None,
) -> bool:
    """Test inclusion between two NFTs: `smaller` <= `bigger`."""

def are_equivalent(
    lhs: Nft,
    rhs: Nft,
    alphabet: alph.Alphabet | None = None,
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
    params: dict[str, str] | None = None,
) -> bool:
    """Test equivalence of two NFTs."""

def project_out(
    nft: Nft,
    levels_to_project: Level | Iterable[Level],
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
) -> Nft:
    """Project out `levels_to_project` in `nft`."""

def project_to(
    nft: Nft,
    levels_to_project: Level | Iterable[Level],
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
) -> Nft:
    """Project to `levels_to_project` in `nft`."""

def insert_levels(
    nft: Nft,
    new_levels_mask: Iterable[bool],
    new_level_alphabets: list[alph.Alphabet | None] | None = None,
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
) -> Nft:
    """Insert new levels, as specified by the boolean mask `new_levels_mask`, into `nft`."""

def insert_level(
    nft: Nft,
    new_level: Level,
    new_level_alphabet: alph.Alphabet | None = None,
    jump_mode: JumpMode = JumpMode.RepeatSymbol,
) -> Nft:
    """Insert a new level `new_level` into `nft`."""

def encode_word(alphabet: alph.Alphabet, word: list[str]) -> list[Symbol]:
    """Encode `word` (list of symbol names) based on `alphabet`."""

def symbols_match(a: Symbol, b: Symbol) -> bool:
    """Check whether `a` and `b` match."""

def has_epsilon_cycle(nft: Nft) -> bool:
    """Check whether `nft` has a cycle of epsilon transitions."""

# Builder functions
def create_single_word_nft(word: list[Symbol]) -> Nft:
    """Create an NFT accepting only a single `word`."""

def create_empty_string_nft(num_of_levels: int = DEFAULT_NUM_OF_LEVELS) -> Nft:
    """Create an NFT accepting only the empty string."""

def create_sigma_star_nft(num_of_levels: int = DEFAULT_NUM_OF_LEVELS) -> Nft:
    """Create an NFT accepting sigma star using the `DONT_CARE` symbol."""

def parse_from_mata_string(nft_in_mata: str) -> Nft:
    """Parse an NFT from a string in mata format."""

def parse_from_mata_file(nft_file: str, encoding: str = "utf-8") -> Nft:
    """Parse an NFT from a file in mata format."""

def from_nfa_with_levels_zero(
    nfa: mata_nfa.Nfa,
    num_of_levels: int = DEFAULT_NUM_OF_LEVELS,
    explicit_transitions: bool = True,
    next_levels_symbol: Symbol | None = None,
) -> Nft:
    """Create an NFT from `nfa` with `num_of_levels` levels, taking `nfa`'s transitions between level 0 and level 1."""

def from_nfa_with_levels_advancing(nfa: mata_nfa.Nfa, num_of_levels: int) -> Nft:
    """Create an NFT from `nfa` with `num_of_levels` levels, assigning levels by distance from the initial state."""
