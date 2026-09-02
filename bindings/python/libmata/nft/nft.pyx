from enum import IntEnum

from libc.stdint cimport uint8_t
from libcpp cimport bool
from libcpp.memory cimport shared_ptr, make_shared, static_pointer_cast, const_pointer_cast
from libcpp.optional cimport optional
from libcpp.set cimport set as cset
from libcpp.string cimport string
from libcpp.unordered_map cimport unordered_map as umap
from libcpp.utility cimport pair
from libcpp.vector cimport vector

from cython.operator import dereference, postincrement as postinc, preincrement as preinc

cimport libmata.nft.nft as mata_nft
cimport libmata.nfa.nfa as mata_nfa
cimport libmata.alphabets as alph

from libmata.nft.nft cimport (
    Symbol, State, StateSet, StateRenaming, Level,
    CLevels, CNft, CJumpMode, CCompositionMode,
    CAutomaton, CDelta, CRun, CTrans, CSymbolPost, CNfa,
    CAlphabet, CConstAlphabet, CAlphabetLevels,
)
from libmata.nfa.nfa cimport CSparseSet, CBoolVector, CBinaryRelation
from libmata.nfa.nfa cimport Transition
from libmata.nfa.nfa cimport ostream, ofstream, stringstream
from libmata.utils cimport COrdVector, BinaryRelation, CPairHash
from libmata.nfa.nfa import Run, run_safely_external_command


cdef Symbol EPSILON = mata_nft.CNFT_EPSILON
cdef Symbol DONT_CARE = mata_nft.CDONT_CARE
cdef size_t DEFAULT_NUM_OF_LEVELS = mata_nft.CDEFAULT_NUM_OF_LEVELS
cdef State LIMITS_MAX_STATE = mata_nft.CLIMITS_MAX_STATE


cdef subset_map_to_dictionary(umap[StateSet, State] subset_map):
    result = {}
    cdef umap[StateSet, State].iterator it = subset_map.begin()
    while it != subset_map.end():
        key = dereference(it).first.to_vector()
        value = dereference(it).second
        result[tuple(sorted(key))] = value
        postinc(it)
    return result


def epsilon():
    return EPSILON


def dont_care():
    return DONT_CARE


class JumpMode(IntEnum):
    """Specifies how a jump transition (a transition with a length greater than 1) is interpreted."""
    RepeatSymbol = <int>mata_nft.JumpModeRepeatSymbol
    AppendDontCares = <int>mata_nft.JumpModeAppendDontCares
    NoJump = <int>mata_nft.JumpModeNoJump


class CompositionMode(IntEnum):
    """Mode of composition to use for `compose()`."""
    General = <int>mata_nft.CompositionModeGeneral
    FastNoJump = <int>mata_nft.CompositionModeFastNoJump
    Auto = <int>mata_nft.CompositionModeAuto


cdef CJumpMode _c_jump_mode(jump_mode):
    return <CJumpMode><int>jump_mode


cdef CCompositionMode _c_composition_mode(composition_mode):
    return <CCompositionMode><int>composition_mode


cdef StateSet _c_state_set(states):
    cdef vector[State] c_states_vec = sorted(states)
    return StateSet(c_states_vec)


cdef class Levels:
    """Wrapper over the per-state levels of an NFT (`mata::nft::Levels`)."""

    def __cinit__(
        self,
        num_of_levels=None,
        levels=None,
        count=None,
        value: Level = 0,
    ):
        """Constructor of Levels.

        :param num_of_levels: Number of levels (tracks) of the NFT.
        :param levels: Either another `Levels` instance to copy, or a list of per-state levels.
        :param count: When given together with `num_of_levels`, creates `count` states, all set to `value`.
        :param value: The level to use when `count` is given.
        """
        cdef vector[Level] c_levels_vec
        if isinstance(levels, Levels):
            self.thisptr = new CLevels((<Levels>levels).thisptr[0])
            return
        if count is not None:
            self.thisptr = new CLevels(<size_t>num_of_levels, <size_t>count, <Level>value)
            return
        if levels is not None:
            c_levels_vec = levels
            if num_of_levels is not None:
                self.thisptr = new CLevels(<size_t>num_of_levels, c_levels_vec)
            else:
                self.thisptr = new CLevels(c_levels_vec)
            return
        if num_of_levels is not None:
            self.thisptr = new CLevels(<size_t>num_of_levels)
        else:
            self.thisptr = new CLevels()

    def __dealloc__(self):
        if self.thisptr != NULL:
            del self.thisptr

    @property
    def num_of_levels(self) -> int:
        return self.thisptr.num_of_levels

    @num_of_levels.setter
    def num_of_levels(self, size_t value):
        self.thisptr.num_of_levels = value

    def __len__(self) -> int:
        return self.thisptr.size()

    def __getitem__(self, size_t state) -> Level:
        return self.thisptr.at(state)

    def __setitem__(self, State state, Level level):
        self.thisptr.set(state, level)

    def __iter__(self):
        cdef size_t i
        for i in range(self.thisptr.size()):
            yield self.thisptr.at(i)

    def __eq__(self, other):
        if not isinstance(other, Levels):
            return NotImplemented
        return dereference(self.thisptr) == dereference((<Levels>other).thisptr)

    def __repr__(self):
        return f"Levels(num_of_levels={self.num_of_levels}, levels={list(self)})"

    def append(self, Levels other) -> None:
        """Append `other`'s levels to the end of `self`."""
        self.thisptr.append(dereference(other.thisptr))

    def set(self, state_or_levels, level: Level = 0) -> Levels:
        """Set the level of a single state, or replace all levels with `state_or_levels`."""
        cdef vector[Level] c_levels_vec
        if isinstance(state_or_levels, int):
            self.thisptr.set(<State>state_or_levels, level)
        else:
            c_levels_vec = state_or_levels
            self.thisptr.set(c_levels_vec)
        return self

    def count(self, Level level) -> int:
        """Count the number of states with `level`."""
        return self.thisptr.count(level)

    def get_levels_of(self, states) -> list[Level]:
        """Get levels of the states in `states`."""
        cdef StateSet c_states = _c_state_set(states)
        cdef vector[Level] result = self.thisptr.get_levels_of(c_states)
        return [level for level in result]

    def map_levels_to(self, states) -> list[set[State]]:
        """Get a mapping of levels to sets of states from `states`."""
        cdef StateSet c_states = _c_state_set(states)
        cdef vector[StateSet] result = self.thisptr.map_levels_to(c_states)
        return [set(level_states.to_vector()) for level_states in result]

    def next_level_after(self, Level level) -> Level:
        """Get the next level that should follow after `level`."""
        return self.thisptr.next_level_after(level)

    def get_minimal_level_of(self, states):
        """Get the minimal level (0 < 1 < ... < num_of_levels-1) of the states in `states`."""
        cdef StateSet c_states = _c_state_set(states)
        cdef optional[Level] result = self.thisptr.get_minimal_level_of(c_states)
        return result.value() if result.has_value() else None

    def get_minimal_next_level_of(self, states):
        """Get the minimal next level (1 < 2 < ... < num_of_levels-1 < 0) of the states in `states`."""
        cdef StateSet c_states = _c_state_set(states)
        cdef optional[Level] result = self.thisptr.get_minimal_next_level_of(c_states)
        return result.value() if result.has_value() else None

    @staticmethod
    def can_follow(source_level: Level, target_level: Level) -> bool:
        """Check whether a transition can be made from a state with `source_level` to a state with `target_level`."""
        return CLevels.can_follow(<Level>source_level, <Level>target_level)

    def can_follow_for_states(self, State source, State target) -> bool:
        """Check whether a transition can be made from `source` to `target`."""
        return self.thisptr.can_follow_for_states(source, target)

    def num_of_levels_used(self) -> int | None:
        """Get the minimal number of levels that can accommodate all currently stored levels."""
        cdef optional[size_t] result = self.thisptr.num_of_levels_used()
        return result.value() if result.has_value() else None


cdef class Nft:
    """Wrapper over NFT (non-deterministic finite transducer).

    Note: In order to add more properties to Nft, see `nft.pxd`, where there is forward declaration.
    """

    def __cinit__(self, state_number = 0, num_of_levels: int = DEFAULT_NUM_OF_LEVELS, label=None):
        """Constructor of the NFT.

        :param int state_number: number of states in the automaton
        :param int num_of_levels: number of levels (tracks) of the transducer
        :param label: optional user-defined label
        """
        cdef CSparseSet[State] empty_default_sparse_set
        cdef shared_ptr[CAlphabetLevels] c_alphabets
        self.thisptr = make_shared[CNft](
            <size_t>state_number, empty_default_sparse_set, empty_default_sparse_set,
            CLevels(<size_t>num_of_levels), c_alphabets
        )
        self.label = label

    @classmethod
    def with_levels(cls, levels, num_of_states = 0, alphabets: alph.AlphabetLevels = None) -> Nft:
        """Construct a new explicit NFT with `num_of_states` states, using `levels`.

        :param levels: Either the number of levels of the NFT, or a `Levels` instance to use directly.
        :param int num_of_states: Number of states for which to preallocate the delta.
        :param alph.AlphabetLevels alphabets: Per-level alphabets of the NFT.
        """
        cdef CSparseSet[State] empty_default_sparse_set
        cdef shared_ptr[CAlphabetLevels] c_alphabets
        cdef CLevels c_levels
        if alphabets is not None:
            c_alphabets = alphabets.thisptr
        if isinstance(levels, Levels):
            c_levels = dereference((<Levels>levels).thisptr)
        else:
            c_levels = CLevels(<size_t>levels)
        result = Nft.__new__(Nft)
        (<Nft>result).thisptr = make_shared[CNft](
            mata_nft.c_nft_with_levels(
                c_levels, <size_t>num_of_states, empty_default_sparse_set, empty_default_sparse_set, c_alphabets
            )
        )
        return result

    @classmethod
    def from_nfa(cls, mata_nfa.Nfa nfa, num_of_levels: int = 1, default_level: Level = 0) -> Nft:
        """Construct a new NFT with `num_of_levels` levels from `nfa`. All states' levels are set to `default_level`."""
        result = Nft.__new__(Nft)
        (<Nft>result).thisptr = make_shared[CNft](
            dereference(nfa.thisptr.get()), <size_t>num_of_levels, <Level>default_level
        )
        return result

    @classmethod
    def from_nfa_with_levels(cls, mata_nfa.Nfa nfa, Levels levels) -> Nft:
        """Construct a new NFT from `nfa`, using `levels` for the states of `nfa`."""
        result = Nft.__new__(Nft)
        (<Nft>result).thisptr = make_shared[CNft](dereference(nfa.thisptr.get()), dereference(levels.thisptr))
        return result

    def deepcopy(self):
        """Return a deep copy of the NFT.

        :return: A deep copy of the NFT.
        """
        cdef Nft new_nft = Nft.__new__(Nft)
        new_nft.thisptr = make_shared[CNft](dereference(self.thisptr.get()))
        return new_nft

    @property
    def label(self):
        return self.label

    @label.setter
    def label(self, value):
        self.label = value

    @property
    def levels(self) -> Levels:
        """A snapshot copy of the per-state levels. Assign back to update the NFT's levels."""
        cdef Levels result = Levels.__new__(Levels)
        result.thisptr = new CLevels(self.thisptr.get().levels)
        return result

    @levels.setter
    def levels(self, Levels value):
        self.thisptr.get().levels = dereference(value.thisptr)

    @property
    def alphabets(self) -> alph.AlphabetLevels | None:
        """The per-level alphabets of the NFT, or `None` if none is set."""
        if self.thisptr.get().alphabets.get() == NULL:
            return None
        result = alph.AlphabetLevels.__new__(alph.AlphabetLevels)
        (<alph.AlphabetLevels>result).thisptr = self.thisptr.get().alphabets
        return result

    @alphabets.setter
    def alphabets(self, alph.AlphabetLevels value):
        if value is None:
            self.thisptr.get().alphabets = shared_ptr[CAlphabetLevels]()
        else:
            self.thisptr.get().alphabets = value.thisptr

    @property
    def initial_states(self):
        return [s for s in self.thisptr.get().initial]

    @initial_states.setter
    def initial_states(self, vector[State] value):
        self.thisptr.get().initial.clear()
        for state in value:
            self.thisptr.get().initial.insert(state)

    @property
    def final_states(self):
        return [s for s in self.thisptr.get().final]

    @final_states.setter
    def final_states(self, vector[State] value):
        self.thisptr.get().final.clear()
        for state in value:
            self.thisptr.get().final.insert(state)

    def is_state(self, state):
        return 0 <= state and state < self.thisptr.get().num_of_states()

    def add_new_state(self):
        """Add a new (fresh) state to the automaton.

        :return: The newly created state.
        """
        return self.thisptr.get().add_state()

    def add_state(self, State state):
        """Add `state` to the automaton if not already present.

        :return: The requested state.
        """
        return self.thisptr.get().add_state(state)

    def add_new_state_with_level(self, Level level):
        """Add a new (fresh) state to the automaton with `level`.

        :return: The newly created state.
        """
        return self.thisptr.get().add_state_with_level(level)

    def add_state_with_level(self, State state, Level level):
        """Add `state` to the automaton with `level` if not already present.

        :return: The requested state.
        """
        return self.thisptr.get().add_state_with_level(state, level)

    def num_of_states_with_level(self, Level level) -> int:
        """Get the number of states with `level`."""
        return self.thisptr.get().num_of_states_with_level(level)

    def make_initial_state(self, State state):
        self.thisptr.get().initial.insert(state)

    def make_initial_states(self, vector[State] states):
        cdef State state
        for state in states:
            self.thisptr.get().initial.insert(state)

    def has_initial_state(self, State st):
        return self.thisptr.get().initial.contains(st)

    def remove_initial_state(self, State state):
        self.thisptr.get().initial.erase(state)

    def clear_initial(self):
        self.thisptr.get().initial.clear()

    def make_final_state(self, State state):
        self.thisptr.get().final.insert(state)

    def make_final_states(self, vector[State] states):
        cdef State state
        for state in states:
            self.thisptr.get().final.insert(state)

    def has_final_state(self, State st):
        return self.thisptr.get().final.contains(st)

    def remove_final_state(self, State state):
        self.thisptr.get().final.erase(state)

    def clear_final(self):
        self.thisptr.get().final.clear()

    @property
    def delta(self) -> mata_nfa.Delta:
        """The transition relation of the automaton.

        Returns a live view over the automaton's transitions, allowing operations similar to the C++ interface,
        e.g. `nft.delta.add(source, symbol, target)`, `nft.delta.contains(...)`, or iterating `for t in nft.delta`.
        """
        return mata_nfa.wrap_delta(static_pointer_cast[CAutomaton, CNft](self.thisptr))

    def add_transition_object(self, Transition tr):
        self.thisptr.get().delta.add(dereference((<mata_nfa.Transition>tr).thisptr))

    def add_transition(self, State source, symbols, target = None):
        """Add a single NFT transition creating new inner states for all tapes.

        :param State source: Source state, must already exist.
        :param symbols: Either a nonempty list of symbols (one per tape) for `add_transition`, or a single symbol
          together with `length` handled via `add_transition_with_length`.
        :param State target: Target state. If `None`, a new target state is created.
        :return: The target state.
        """
        cdef vector[Symbol] c_symbols = symbols
        if target is None:
            return self.thisptr.get().add_transition_by_levels(source, c_symbols)
        return self.thisptr.get().add_transition_by_levels(source, c_symbols, target)

    def add_transition_with_length(self, State source, Symbol symbol, size_t length, jump_mode = JumpMode.RepeatSymbol):
        """Add a single NFT transition with `length`, creating a new target state.

        :return: The newly created target state.
        """
        return self.thisptr.get().add_transition_with_length(source, symbol, length, _c_jump_mode(jump_mode))

    def add_transition_with_target(self, State source, Symbol symbol, State target, jump_mode = JumpMode.RepeatSymbol):
        """Add a single NFT transition from `source` to `target`."""
        self.thisptr.get().add_transition_with_target(source, symbol, target, _c_jump_mode(jump_mode))

    def add_transition_with_same_level_targets(self, State source, Symbol symbol, targets, jump_mode = JumpMode.RepeatSymbol):
        """Add a NFT transition from `source` to a set of `targets`, sharing the common prefix of the transition."""
        cdef StateSet c_targets = _c_state_set(targets)
        self.thisptr.get().add_transition_with_same_level_targets(source, symbol, c_targets, _c_jump_mode(jump_mode))

    def remove_trans(self, Transition tr):
        self.thisptr.get().delta.remove(dereference((<mata_nfa.Transition>tr).thisptr))

    def remove_trans_raw(self, State source, Symbol symbol, State target):
        self.thisptr.get().delta.remove(source, symbol, target)

    def has_transition(self, State source, Symbol symbol, State target):
        return self.thisptr.get().delta.contains(source, symbol, target)

    def get_num_of_transitions(self):
        return self.thisptr.get().delta.num_of_transitions()

    def clear(self):
        self.thisptr.get().clear()

    def num_of_states(self) -> int:
        return self.thisptr.get().num_of_states()

    def iterate(self):
        """Iterates over all transitions.

        :return: stream of transitions
        """
        cdef mata_nfa.CTransitions transitions = self.thisptr.get().delta.transitions()
        cdef mata_nfa.CTransitions.const_iterator iterator = transitions.begin()
        while iterator != transitions.end():
            trans = Transition()
            (<Transition>trans).copy_from(dereference(iterator))
            preinc(iterator)
            yield trans

    def get_trans_as_sequence(self) -> list[Transition]:
        """Get automaton transitions as a sequence.

        :return: List of automaton transitions.
        """
        cdef mata_nfa.CTransitions c_transitions = self.thisptr.get().delta.transitions()
        transitions = []
        for c_transition in c_transitions:
            transitions.append(Transition(c_transition.source, c_transition.symbol, c_transition.target))
        return transitions

    def get_used_symbols(self):
        """Return a set of symbols used on the transitions in NFT.

        :return: Set of symbols.
        """
        cdef COrdVector[Symbol] symbols = self.thisptr.get().delta.get_used_symbols()
        return {s for s in symbols}

    def is_identical(self, Nft other) -> bool:
        return self.thisptr.get().is_identical(dereference(other.thisptr.get()))

    def is_lang_empty(self, run = None):
        cdef CRun local_run
        cdef CRun* c_run = NULL
        if run is not None:
            c_run = &local_run
        result = self.thisptr.get().is_lang_empty(c_run)
        if run is not None:
            run.word = list(local_run.word)
            run.path = list(local_run.path)
        return result

    def is_deterministic(self):
        return self.thisptr.get().is_deterministic()

    def is_complete(self, alph.Alphabet alphabet = None):
        if alphabet:
            return self.thisptr.get().is_complete(alphabet.as_base().get())
        else:
            return self.thisptr.get().is_complete()

    def insert_word(self, State source, word, target = None):
        """Insert `word` into the NFT from `source` to `target`, creating new states along the path.

        :param State source: The source state, must already exist.
        :param word: The nonempty word to insert.
        :param State target: The target state. If `None`, a new target state is created.
        :return: The state where the inserted word ends.
        """
        cdef vector[Symbol] c_word = word
        if target is None:
            return self.thisptr.get().insert_word(source, c_word)
        return self.thisptr.get().insert_word(source, c_word, target)

    def insert_word_by_levels(self, State source, word_parts_on_levels, target = None):
        """Insert a word interleaved from `word_parts_on_levels` (one part per level) from `source` to `target`.

        :param State source: The source state, must already exist and be at level 0.
        :param word_parts_on_levels: A list of word parts, one per level.
        :param State target: The target state. If `None`, a new target state is created.
        :return: The state where the inserted word ends.
        """
        cdef vector[vector[Symbol]] c_word_parts
        for part in word_parts_on_levels:
            c_word_parts.push_back(<vector[Symbol]>part)
        if target is None:
            return self.thisptr.get().insert_word_by_levels(source, c_word_parts)
        return self.thisptr.get().insert_word_by_levels(source, c_word_parts, target)

    def insert_identity(self, State state, symbols_or_alphabet = None, jump_mode = JumpMode.RepeatSymbol) -> Nft:
        """Insert identity transition(s) at `state`.

        :param State state: The state where the identity transition will be inserted (both source and target).
        :param symbols_or_alphabet: Either a single symbol, a list of symbols, an `alph.Alphabet` (resolved via
          `resolve_alphabet`), or `None` (also resolved via `resolve_alphabet`).
        :return: Self with the identity transition(s) inserted.
        """
        cdef vector[Symbol] c_symbols
        cdef CAlphabet* c_alphabet
        cdef CJumpMode c_jump_mode = _c_jump_mode(jump_mode)
        if symbols_or_alphabet is None or isinstance(symbols_or_alphabet, alph.Alphabet):
            c_alphabet = alph.unwrap_alphabet_or_null(symbols_or_alphabet)
            self.thisptr.get().insert_identity(state, c_alphabet, c_jump_mode)
        elif isinstance(symbols_or_alphabet, (list, set, tuple)):
            c_symbols = list(symbols_or_alphabet)
            self.thisptr.get().insert_identity(state, c_symbols, c_jump_mode)
        else:
            self.thisptr.get().insert_identity(state, <Symbol>symbols_or_alphabet, c_jump_mode)
        return self

    def contains_jump_transitions(self) -> bool:
        """Check if the transducer contains any jump transition."""
        return self.thisptr.get().contains_jump_transitions()

    def trim(self):
        """Remove inaccessible and not co-accessible states.

        :return: Nft (self).
        """
        self.thisptr.get().trim(NULL)
        return self

    def trim_with_state_map(self):
        """Remove inaccessible and not co-accessible states.

        :return: Nft (self), state map of original to new states.
        """
        cdef StateRenaming state_map
        self.thisptr.get().trim(&state_map)
        return self, {k: v for k, v in state_map}

    def remove_epsilon_inplace(self, Symbol epsilon = EPSILON):
        """Remove simple epsilon transitions in place."""
        self.thisptr.get().remove_epsilon(epsilon)

    def concatenate(self, Nft other) -> Nft:
        """Concatenate `self` with `other` in-place.

        :return: Nft (self).
        """
        self.thisptr.get().concatenate(dereference(other.thisptr.get()))
        return self

    def union(self, Nft other) -> Nft:
        """Make a non-deterministic union of `self` with `other` in-place.

        :return: Nft (self).
        """
        self.thisptr.get().unite_nondet_with(dereference(other.thisptr.get()))
        return self

    def get_one_letter_aut(self, levels_to_keep = None, Symbol abstract_symbol = ord('x')) -> Nft:
        """Get an NFT where transitions of `self` are replaced with transitions over one symbol.

        :param levels_to_keep: Transitions coming from states with any of these levels are not replaced.
        :param Symbol abstract_symbol: The symbol to replace with.
        :return: Nft.
        """
        cdef cset[Level] c_levels_to_keep
        for level in (levels_to_keep or []):
            c_levels_to_keep.insert(<Level>level)
        cdef Nft one_letter_aut = Nft.__new__(Nft)
        one_letter_aut.thisptr = make_shared[CNft](
            self.thisptr.get().get_one_letter_aut(c_levels_to_keep, abstract_symbol)
        )
        return one_letter_aut

    def unwind_jumps_inplace(self, dont_care_symbol_replacements = None, jump_mode = JumpMode.RepeatSymbol):
        """Unwind jump transitions in place."""
        cdef COrdVector[Symbol] c_replacements = _c_ord_vector_symbol(dont_care_symbol_replacements or {DONT_CARE})
        self.thisptr.get().unwind_jumps_inplace(c_replacements, _c_jump_mode(jump_mode))

    def unwind_jumps(self, dont_care_symbol_replacements = None, jump_mode = JumpMode.RepeatSymbol) -> Nft:
        """Create a transducer with unwound jump transitions from `self`."""
        cdef COrdVector[Symbol] c_replacements = _c_ord_vector_symbol(dont_care_symbol_replacements or {DONT_CARE})
        cdef Nft result = Nft.__new__(Nft)
        result.thisptr = make_shared[CNft](self.thisptr.get().unwind_jumps(c_replacements, _c_jump_mode(jump_mode)))
        return result

    def is_epsilon(self, Symbol symbol) -> bool:
        """Check whether the passed symbol is the epsilon symbol."""
        return self.thisptr.get().is_epsilon(symbol)

    def __str__(self):
        result = "initial_states: {}\n".format([s for s in self.thisptr.get().initial])
        result += "final_states: {}\n".format([s for s in self.thisptr.get().final])
        result += "transitions:\n"
        for trans in self.iterate():
            result += f"{trans.source}-[{trans.symbol}]→{trans.target}\n"
        return result

    def __repr__(self):
        return str(self)

    def to_dot_file(self, output_file='nft.dot', output_format='pdf', decode_ascii_chars=False, use_intervals=False, max_label_length=-1):
        """Transforms the automaton to dot format and renders it via graphviz.

        :param str output_file: name of the output file where the automaton will be stored
        :param str output_format: format of the output file (pdf/png/etc)
        """
        cdef ofstream* output
        output = new ofstream(output_file.encode('utf-8'))
        try:
            self.thisptr.get().print_to_dot(dereference(output), decode_ascii_chars, use_intervals, max_label_length, NULL)
        finally:
            del output

        graphviz_command = f"dot -O -T{output_format} {output_file}"
        _, err = run_safely_external_command(graphviz_command)
        if err:
            print(f"error while dot file: {err}")

    def to_dot_str(self, encoding='utf-8', decode_ascii_chars=False, use_intervals=False, max_label_length=-1):
        """Transforms the automaton to a dot format string.

        :return: string with dot representation of the automaton
        """
        cdef stringstream* output_stream
        output_stream = new stringstream("".encode('ascii'))
        cdef string result
        try:
            self.thisptr.get().print_to_dot(dereference(output_stream), decode_ascii_chars, use_intervals, max_label_length, NULL)
            result = output_stream.str()
        finally:
            del output_stream
        return result.decode(encoding)

    def to_mata_str(self, encoding='utf-8') -> str:
        """Transforms the automaton to a mata-format string."""
        cdef string result = self.thisptr.get().print_to_mata()
        return result.decode(encoding)

    def to_mata_file(self, output_file='nft.mata', encoding='utf-8'):
        """Writes the automaton to `output_file` in mata format."""
        with open(output_file, 'w', encoding=encoding) as handle:
            handle.write(self.to_mata_str(encoding))

    def post_of(self, states, Symbol symbol, symbol_level = None):
        """Get the set of states reachable from `states` over `symbol` (optionally, on a given `symbol_level`)."""
        cdef StateSet c_states = _c_state_set(states)
        cdef StateSet result
        if symbol_level is None:
            result = self.thisptr.get().post(c_states, symbol)
        else:
            result = self.thisptr.get().post(c_states, symbol, <Level>symbol_level)
        return set(result.to_vector())

    def is_universal(self, alph.Alphabet alphabet, params = None):
        """Tests if the NFT is universal with regard to the given alphabet."""
        params = params or {'algorithm': 'antichains'}
        cdef umap[string, string] c_params = {
            k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v for k, v in params.items()
        }
        return self.thisptr.get().is_universal(<CAlphabet&>dereference(alphabet.as_base()), c_params)

    def is_in_lang(self, run_or_word, match_prefix = False, jump_mode = JumpMode.RepeatSymbol, has_epsilon_cycles = True):
        """Tests if `run_or_word` (or its prefix, with `match_prefix`) is in the language of the NFT."""
        cdef CRun c_run
        if isinstance(run_or_word, Run):
            c_run.word = <vector[Symbol]>run_or_word.word
            c_run.path = <vector[State]>run_or_word.path
        else:
            c_run.word = <vector[Symbol]>run_or_word
        return self.thisptr.get().is_in_lang(c_run, match_prefix, _c_jump_mode(jump_mode), has_epsilon_cycles)

    def is_in_lang_prefix(self, run_or_word, jump_mode = JumpMode.RepeatSymbol, has_epsilon_cycles = True):
        """Tests if a prefix of `run_or_word` is in the language of the NFT."""
        return self.is_in_lang(run_or_word, True, jump_mode, has_epsilon_cycles)

    def is_in_lang_by_levels(self, level_words, match_prefix = False, jump_mode = JumpMode.RepeatSymbol, has_epsilon_cycles = True):
        """Tests if the tuple `level_words` (one word per level) is in the regular relation accepted by the NFT."""
        cdef vector[vector[Symbol]] c_level_words
        for word in level_words:
            c_level_words.push_back(<vector[Symbol]>word)
        return self.thisptr.get().is_in_lang_by_levels(
            c_level_words, match_prefix, _c_jump_mode(jump_mode), has_epsilon_cycles
        )

    def is_in_lang_prefix_by_levels(self, level_words, jump_mode = JumpMode.RepeatSymbol, has_epsilon_cycles = True):
        """Tests if a prefix of the tuple `level_words` is in the regular relation accepted by the NFT."""
        return self.is_in_lang_by_levels(level_words, True, jump_mode, has_epsilon_cycles)

    def get_word_for_path(self, path):
        """For a given path (list of states) returns a corresponding word.

        :return: pair of word (list of symbols) and true or false, whether the search was successful
        """
        cdef CRun c_run
        c_run.path = <vector[State]>path
        cdef pair[CRun, bool] result = self.thisptr.get().get_word_for_path(c_run)
        return list(result.first.word), result.second

    def mk_level_word_from_word(self, word) -> list[list[int]]:
        """Convert `word` (interleaved representation) to level words according to the levels of the NFT."""
        cdef vector[Symbol] c_word = word
        cdef vector[vector[Symbol]] result = self.thisptr.get().mk_level_word_from_word(c_word)
        return [list(level_word) for level_word in result]

    def mk_word_from_level_word(self, level_words) -> list[int]:
        """Convert `level_words` to a word (interleaved representation) according to the levels of the NFT."""
        cdef vector[vector[Symbol]] c_level_words
        for word in level_words:
            c_level_words.push_back(<vector[Symbol]>word)
        cdef vector[Symbol] result = self.thisptr.get().mk_word_from_level_word(c_level_words)
        return list(result)

    def get_words(self, max_length = None, jump_mode = JumpMode.RepeatSymbol) -> set[tuple[int, ...]]:
        """Get the set of all words in the language of the NFT whose length is <= `max_length`."""
        cdef size_t c_max_length = max_length if max_length is not None else <size_t>(-1)
        cdef cset[vector[Symbol]] result = self.thisptr.get().get_words(c_max_length, _c_jump_mode(jump_mode))
        return {tuple(word) for word in result}

    def apply(self, nfa_or_word, level_to_apply_on: Level = 0, project_out_applied_level = True, jump_mode = JumpMode.RepeatSymbol) -> Nft:
        """Apply `nfa_or_word` to `self`, intersecting it with `level_to_apply_on` of `self`."""
        cdef vector[Symbol] c_word
        cdef Nft result = Nft.__new__(Nft)
        if isinstance(nfa_or_word, mata_nfa.Nfa):
            result.thisptr = make_shared[CNft](
                self.thisptr.get().apply_nfa(
                    dereference((<mata_nfa.Nfa>nfa_or_word).thisptr.get()), level_to_apply_on,
                    project_out_applied_level, _c_jump_mode(jump_mode)
                )
            )
        else:
            c_word = nfa_or_word
            result.thisptr = make_shared[CNft](
                self.thisptr.get().apply_word(c_word, level_to_apply_on, project_out_applied_level, _c_jump_mode(jump_mode))
            )
        return result

    def to_nfa_copy(self) -> mata_nfa.Nfa:
        """Copy the NFT as an NFA. Transitions are not updated to have only one level."""
        cdef mata_nfa.Nfa result = mata_nfa.Nfa()
        result.thisptr = make_shared[CNfa](self.thisptr.get().to_nfa_copy())
        return result

    def to_nfa_update_copy(self, dont_care_symbol_replacements = None, jump_mode = JumpMode.RepeatSymbol) -> mata_nfa.Nfa:
        """Copy the NFT as an NFA, updating the transitions to have only one level."""
        cdef COrdVector[Symbol] c_replacements = _c_ord_vector_symbol(dont_care_symbol_replacements or {DONT_CARE})
        cdef mata_nfa.Nfa result = mata_nfa.Nfa()
        result.thisptr = make_shared[CNfa](
            self.thisptr.get().to_nfa_update_copy(c_replacements, _c_jump_mode(jump_mode))
        )
        return result

    def make_complete(self, alphabet_or_symbols = None, sink_states = None) -> bool:
        """Make the NFT complete in place.

        :param alphabet_or_symbols: Either an `alph.Alphabet` (resolved via `resolve_alphabet`), or an already
          precomputed set of symbols. If `None`, resolved via `resolve_alphabet`.
        :param sink_states: The level-indexed list of sink states to use. If `None`, new sink states are added.
        """
        cdef CAlphabet* c_alphabet
        cdef COrdVector[Symbol] c_symbols
        cdef optional[vector[State]] c_sink_states
        if sink_states is not None:
            c_sink_states = optional[vector[State]](<vector[State]>sink_states)
        if alphabet_or_symbols is None or isinstance(alphabet_or_symbols, alph.Alphabet):
            c_alphabet = alph.unwrap_alphabet_or_null(alphabet_or_symbols)
            return self.thisptr.get().make_complete(c_alphabet, c_sink_states)
        c_symbols = _c_ord_vector_symbol(alphabet_or_symbols)
        return self.thisptr.get().make_complete(c_symbols, c_sink_states)

    def resolve_alphabet(self, alph.Alphabet alphabet = None, level = None) -> alph.Alphabet:
        """Resolve which alphabet to use for the current operation on `level`.

        :warning: When resolution falls back to `self.alphabets`/`self.alphabet` (no explicit `alphabet` given), the
          returned object is the NFT's own live alphabet, not a copy — the same instance is typically shared with
          other automata (see `AlphabetLevels`). Treat it as read-only: mutating it (e.g. `.clear()`,
          `.add_new_symbol(...)`) mutates it for everyone sharing it. Prefer `get_symbols_to_work_with()` if you only
          need the symbol set.
        """
        cdef CAlphabet* c_alphabet = alph.unwrap_alphabet_or_null(alphabet)
        cdef optional[Level] c_level = optional[Level]() if level is None else optional[Level](<Level>level)
        cdef shared_ptr[CConstAlphabet] resolved = self.thisptr.get().resolve_alphabet(c_alphabet, c_level)
        return alph.wrap_alphabet(const_pointer_cast[CAlphabet, CConstAlphabet](resolved))

    def get_symbols_to_work_with(self, alph.Alphabet alphabet = None, level = None):
        """Get the set of symbols to work with for the current operation on `level`."""
        cdef CAlphabet* c_alphabet = alph.unwrap_alphabet_or_null(alphabet)
        cdef optional[Level] c_level = optional[Level]() if level is None else optional[Level](<Level>level)
        cdef COrdVector[Symbol] symbols = self.thisptr.get().get_symbols_to_work_with(c_alphabet, c_level)
        return {s for s in symbols}


cdef COrdVector[Level] _c_ord_vector_level(levels):
    cdef vector[Level] c_levels_vec = sorted(levels)
    return COrdVector[Level](c_levels_vec)


cdef COrdVector[Symbol] _c_ord_vector_symbol(symbols):
    cdef vector[Symbol] c_symbols_vec = sorted(symbols)
    return COrdVector[Symbol](c_symbols_vec)


cdef umap[string, string] _params_to_c(params):
    cdef umap[string, string] c_params = {
        k.encode('utf-8'): v.encode('utf-8') if isinstance(v, str) else v for k, v in params.items()
    }
    return c_params


# Operations
def determinize(Nft lhs):
    """Determinize `lhs`.

    :return: Deterministic NFT.
    """
    result = Nft()
    mata_nft.c_nft_determinize(result.thisptr.get(), dereference(lhs.thisptr.get()), NULL)
    return result


def determinize_with_subset_map(Nft lhs):
    """Determinize `lhs`.

    :return: Deterministic NFT, subset map of sets of states to determinized states.
    """
    result = Nft()
    cdef umap[StateSet, State] subset_map
    mata_nft.c_nft_determinize(result.thisptr.get(), dereference(lhs.thisptr.get()), &subset_map)
    return result, subset_map_to_dictionary(subset_map)


def union_nondet(Nft lhs, Nft rhs) -> Nft:
    """Compute a non-deterministic union of `lhs` and `rhs`."""
    result = Nft()
    mata_nft.c_nft_union_nondet(result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()))
    return result


def intersection(
    Nft lhs, Nft rhs, jump_mode = JumpMode.RepeatSymbol,
    lhs_first_aux_state = LIMITS_MAX_STATE, rhs_first_aux_state = LIMITS_MAX_STATE
) -> Nft:
    """Compute the intersection of `lhs` and `rhs`."""
    result = Nft()
    mata_nft.c_nft_intersection(
        result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), NULL,
        _c_jump_mode(jump_mode), lhs_first_aux_state, rhs_first_aux_state
    )
    return result


def intersection_with_product_map(
    Nft lhs, Nft rhs, jump_mode = JumpMode.RepeatSymbol,
    lhs_first_aux_state = LIMITS_MAX_STATE, rhs_first_aux_state = LIMITS_MAX_STATE
):
    """Compute the intersection of `lhs` and `rhs`.

    :return: Intersection of `lhs` and `rhs`, product map of pairs of original states to new states.
    """
    result = Nft()
    cdef umap[pair[State, State], State, CPairHash[State, State]] c_product_map
    mata_nft.c_nft_intersection(
        result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), &c_product_map,
        _c_jump_mode(jump_mode), lhs_first_aux_state, rhs_first_aux_state
    )
    return result, {tuple(k): v for k, v in c_product_map}


def compose(
    Nft lhs, Nft rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels = True,
    jump_mode = JumpMode.RepeatSymbol, composition_mode = CompositionMode.Auto
) -> Nft:
    """Compose `lhs` and `rhs` by aligning their synchronization levels.

    :param lhs_sync_levels: A single level, or an ordered iterable of synchronization levels of `lhs`.
    :param rhs_sync_levels: A single level, or an ordered iterable of synchronization levels of `rhs`.
    """
    if isinstance(lhs_sync_levels, int):
        lhs_sync_levels = [lhs_sync_levels]
    if isinstance(rhs_sync_levels, int):
        rhs_sync_levels = [rhs_sync_levels]
    cdef COrdVector[Level] c_lhs_sync_levels = _c_ord_vector_level(lhs_sync_levels)
    cdef COrdVector[Level] c_rhs_sync_levels = _c_ord_vector_level(rhs_sync_levels)
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](
        mata_nft.c_compose(
            dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), c_lhs_sync_levels, c_rhs_sync_levels,
            project_out_sync_levels, _c_jump_mode(jump_mode), _c_composition_mode(composition_mode)
        )
    )
    return result


def compose_alphabets(Nft lhs, Nft rhs, lhs_sync_levels, rhs_sync_levels, project_out_sync_levels = True):
    """Compose the per-level alphabets for NFTs to be composed via `compose`.

    :return: The composed `alph.AlphabetLevels`, or `None` if either `lhs.alphabets` or `rhs.alphabets` is `None`.
    """
    if isinstance(lhs_sync_levels, int):
        lhs_sync_levels = [lhs_sync_levels]
    if isinstance(rhs_sync_levels, int):
        rhs_sync_levels = [rhs_sync_levels]
    cdef COrdVector[Level] c_lhs_sync_levels = _c_ord_vector_level(lhs_sync_levels)
    cdef COrdVector[Level] c_rhs_sync_levels = _c_ord_vector_level(rhs_sync_levels)
    cdef shared_ptr[CAlphabetLevels] result = mata_nft.c_compose_alphabets(
        dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), c_lhs_sync_levels, c_rhs_sync_levels,
        project_out_sync_levels
    )
    if result.get() == NULL:
        return None
    wrapped = alph.AlphabetLevels.__new__(alph.AlphabetLevels)
    (<alph.AlphabetLevels>wrapped).thisptr = result
    return wrapped


def concatenate(Nft lhs, Nft rhs, use_epsilon: bool = False) -> Nft:
    """Concatenate two NFTs."""
    result = Nft()
    mata_nft.c_nft_concatenate(
        result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), use_epsilon, NULL, NULL
    )
    return result


def concatenate_with_result_state_maps(Nft lhs, Nft rhs, use_epsilon: bool = False):
    """Concatenate two NFTs.

    :return: Concatenated NFT, state map for `lhs`, state map for `rhs`.
    """
    result = Nft()
    cdef StateRenaming c_lhs_map
    cdef StateRenaming c_rhs_map
    mata_nft.c_nft_concatenate(
        result.thisptr.get(), dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), use_epsilon,
        &c_lhs_map, &c_rhs_map
    )
    return result, {k: v for k, v in c_lhs_map}, {k: v for k, v in c_rhs_map}


def concatenate_nth_power(Nft nft, unsigned power) -> Nft:
    """Compute the NFT accepting the `power`-th power of the language of `nft`."""
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](mata_nft.c_concatenate_nth_power(dereference(nft.thisptr.get()), power))
    return result


def complement(Nft nft, alph.Alphabet alphabet, params = None) -> Nft:
    """Compute the complement of `nft`.

    :warning: Only supports NFTs without epsilon transitions (length-preserving NFTs).
    """
    result = Nft()
    params = params or {'algorithm': 'classical', 'minimize': 'false'}
    mata_nft.c_nft_complement(
        result.thisptr.get(), dereference(nft.thisptr.get()), <CAlphabet&>dereference(alphabet.as_base()),
        _params_to_c(params)
    )
    return result


def revert(Nft lhs) -> Nft:
    """Reverse transitions in `lhs`."""
    result = Nft()
    mata_nft.c_nft_revert(result.thisptr.get(), dereference(lhs.thisptr.get()))
    return result


def invert_levels(Nft aut, jump_mode = JumpMode.RepeatSymbol) -> Nft:
    """Invert the levels of `aut`: level 0 becomes the last level, level 1 the second-to-last, and so on."""
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](mata_nft.c_invert_levels(dereference(aut.thisptr.get()), _c_jump_mode(jump_mode)))
    return result


def remove_epsilon(Nft lhs, Symbol epsilon = EPSILON) -> Nft:
    """Remove simple epsilon transitions from `lhs`."""
    result = Nft()
    mata_nft.c_nft_remove_epsilon(result.thisptr.get(), dereference(lhs.thisptr.get()), epsilon)
    return result


def reduce(Nft aut, params = None) -> Nft:
    """Reduce the size of `aut`."""
    params = params or {"algorithm": "simulation"}
    result = Nft()
    mata_nft.c_nft_reduce(result.thisptr.get(), dereference(aut.thisptr.get()), NULL, _params_to_c(params))
    return result


def reduce_with_state_map(Nft aut, params = None):
    """Reduce the size of `aut`.

    :return: Reduced NFT, state map of original to new states.
    """
    params = params or {"algorithm": "simulation"}
    cdef StateRenaming state_map
    result = Nft()
    mata_nft.c_nft_reduce(result.thisptr.get(), dereference(aut.thisptr.get()), &state_map, _params_to_c(params))
    return result, {k: v for k, v in state_map}


def compute_relation(Nft lhs, params = None):
    """Compute the relation for the NFT."""
    params = params or {'relation': 'simulation', 'direction': 'forward'}
    cdef CBinaryRelation relation = mata_nft.c_nft_compute_relation(dereference(lhs.thisptr.get()), _params_to_c(params))
    result = BinaryRelation()
    cdef size_t relation_size = relation.size()
    result.resize(relation_size)
    for i in range(0, relation_size):
        for j in range(0, relation_size):
            result.set(i, j, relation.get(i, j))
    return result


# Tests
def is_included_with_cex(Nft smaller, Nft bigger, alph.Alphabet alphabet = None, jump_mode = JumpMode.RepeatSymbol, params = None):
    """Test inclusion between two NFTs.

    :return: True if `smaller` is included in `bigger`, counterexample run if not.
    """
    cdef CRun c_run
    cdef CAlphabet* c_alphabet = alph.unwrap_alphabet_or_null(alphabet)
    params = params or {'algorithm': 'antichains'}
    result = mata_nft.c_is_included(
        dereference(smaller.thisptr.get()), dereference(bigger.thisptr.get()), &c_run, c_alphabet,
        _c_jump_mode(jump_mode), _params_to_c(params)
    )
    run = Run()
    run.word = list(c_run.word)
    run.path = list(c_run.path)
    return result, run


def is_included(Nft smaller, Nft bigger, alph.Alphabet alphabet = None, jump_mode = JumpMode.RepeatSymbol, params = None):
    """Test inclusion between two NFTs: `smaller` <= `bigger`."""
    cdef CAlphabet* c_alphabet = alph.unwrap_alphabet_or_null(alphabet)
    params = params or {'algorithm': 'antichains'}
    return mata_nft.c_is_included(
        dereference(smaller.thisptr.get()), dereference(bigger.thisptr.get()), NULL, c_alphabet,
        _c_jump_mode(jump_mode), _params_to_c(params)
    )


def are_equivalent(Nft lhs, Nft rhs, alph.Alphabet alphabet = None, jump_mode = JumpMode.RepeatSymbol, params = None) -> bool:
    """Test equivalence of two NFTs."""
    params = params or {'algorithm': 'antichains'}
    if alphabet is not None:
        return mata_nft.c_are_equivalent(
            dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), alph.unwrap_alphabet_or_null(alphabet),
            _c_jump_mode(jump_mode), _params_to_c(params)
        )
    return mata_nft.c_are_equivalent(
        dereference(lhs.thisptr.get()), dereference(rhs.thisptr.get()), _c_jump_mode(jump_mode), _params_to_c(params)
    )


def project_out(Nft nft, levels_to_project, jump_mode = JumpMode.RepeatSymbol) -> Nft:
    """Project out `levels_to_project` (a single level or an iterable of levels) in `nft`."""
    if isinstance(levels_to_project, int):
        levels_to_project = [levels_to_project]
    cdef COrdVector[Level] c_levels = _c_ord_vector_level(levels_to_project)
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](
        mata_nft.c_project_out(dereference(nft.thisptr.get()), c_levels, _c_jump_mode(jump_mode))
    )
    return result


def project_to(Nft nft, levels_to_project, jump_mode = JumpMode.RepeatSymbol) -> Nft:
    """Project to `levels_to_project` (a single level or an iterable of levels) in `nft`."""
    if isinstance(levels_to_project, int):
        levels_to_project = [levels_to_project]
    cdef COrdVector[Level] c_levels = _c_ord_vector_level(levels_to_project)
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](
        mata_nft.c_project_to(dereference(nft.thisptr.get()), c_levels, _c_jump_mode(jump_mode))
    )
    return result


def insert_levels(
    Nft nft, new_levels_mask, new_level_alphabets = None, jump_mode = JumpMode.RepeatSymbol
) -> Nft:
    """Insert new levels, as specified by the boolean mask `new_levels_mask`, into `nft`.

    :param new_level_alphabets: Alphabets to assign to the newly-inserted levels, in the order the levels appear.
        When `None` or empty, the newly-inserted levels are left without an alphabet.
    """
    cdef CBoolVector c_mask = CBoolVector(<vector[uint8_t]>[1 if v else 0 for v in new_levels_mask])
    cdef vector[shared_ptr[CAlphabet]] c_new_level_alphabets
    cdef alph.Alphabet a
    for a in (new_level_alphabets or []):
        c_new_level_alphabets.push_back(a.as_base() if a is not None else shared_ptr[CAlphabet]())
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](
        mata_nft.c_insert_levels(
            dereference(nft.thisptr.get()), c_mask, c_new_level_alphabets, _c_jump_mode(jump_mode)
        )
    )
    return result


def insert_level(
    Nft nft, Level new_level, new_level_alphabet: alph.Alphabet = None, jump_mode = JumpMode.RepeatSymbol
) -> Nft:
    """Insert a new level `new_level` into `nft`.

    :param new_level_alphabet: Alphabet to assign to the newly-inserted level. When `None` (the default), the
        newly-inserted level is left without an alphabet.
    """
    cdef shared_ptr[CAlphabet] c_new_level_alphabet = (
        new_level_alphabet.as_base() if new_level_alphabet is not None else shared_ptr[CAlphabet]()
    )
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](
        mata_nft.c_insert_level(dereference(nft.thisptr.get()), new_level, c_new_level_alphabet, _c_jump_mode(jump_mode))
    )
    return result


def encode_word(alph.Alphabet alphabet, word):
    """Encode `word` (list of symbol names) based on `alphabet`.

    :return: Encoded word.
    """
    cdef CAlphabet* c_alphabet = alphabet.as_base().get()
    result = mata_nft.c_encode_word(c_alphabet, [s.encode('utf-8') for s in word])
    return result.word


def symbols_match(Symbol a, Symbol b) -> bool:
    """Check whether `a` and `b` match: equal, or one of them is `DONT_CARE` and the other is not `EPSILON`."""
    return mata_nft.c_symbols_match(a, b)


def has_epsilon_cycle(Nft nft) -> bool:
    """Check whether `nft` has a cycle of epsilon transitions."""
    return mata_nft.c_has_epsilon_cycle(dereference(nft.thisptr.get()))


# Builder functions
def create_single_word_nft(word) -> Nft:
    """Create an NFT accepting only a single `word`."""
    cdef vector[Symbol] c_word = word
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](mata_nft.c_create_single_word_nft(c_word))
    return result


def create_empty_string_nft(num_of_levels: int = DEFAULT_NUM_OF_LEVELS) -> Nft:
    """Create an NFT accepting only the empty string."""
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](mata_nft.c_create_empty_string_nft(<size_t>num_of_levels))
    return result


def create_sigma_star_nft(num_of_levels: int = DEFAULT_NUM_OF_LEVELS) -> Nft:
    """Create an NFT accepting sigma star using the `DONT_CARE` symbol."""
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](mata_nft.c_create_sigma_star_nft(<size_t>num_of_levels))
    return result


def parse_from_mata_string(nft_in_mata: str) -> Nft:
    """Parse an NFT from a string in mata format."""
    cdef string c_content = nft_in_mata.encode('utf-8')
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](mata_nft.c_parse_from_mata(c_content))
    return result


def parse_from_mata_file(nft_file: str, encoding='utf-8') -> Nft:
    """Parse an NFT from a file in mata format."""
    with open(nft_file, 'r', encoding=encoding) as handle:
        return parse_from_mata_string(handle.read())


def from_nfa_with_levels_zero(mata_nfa.Nfa nfa, num_of_levels: int = DEFAULT_NUM_OF_LEVELS, explicit_transitions: bool = True, next_levels_symbol = None) -> Nft:
    """Create an NFT from `nfa` with `num_of_levels` levels, taking `nfa`'s transitions between level 0 and level 1."""
    cdef optional[Symbol] c_next_levels_symbol = (
        optional[Symbol]() if next_levels_symbol is None else optional[Symbol](<Symbol>next_levels_symbol)
    )
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](
        mata_nft.c_from_nfa_with_levels_zero(
            dereference(nfa.thisptr.get()), <size_t>num_of_levels, explicit_transitions, c_next_levels_symbol
        )
    )
    return result


def from_nfa_with_levels_advancing(mata_nfa.Nfa nfa, num_of_levels: int) -> Nft:
    """Create an NFT from `nfa` with `num_of_levels` levels, assigning levels by distance from the initial state."""
    cdef Nft result = Nft.__new__(Nft)
    result.thisptr = make_shared[CNft](
        mata_nft.c_from_nfa_with_levels_advancing(dereference(nfa.thisptr.get()), <size_t>num_of_levels)
    )
    return result
