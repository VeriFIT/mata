from typing import Self, Iterable, Any, overload, Literal, TypedDict

from libmata.alphabets import Symbol
import libmata.alphabets as alph
from libmata.utils import BinaryRelation

import pandas
import networkx

State = int

def epsilon() -> Symbol:
    ...

class Run:
    """Wrapper over the run in NFA."""
    def __init__(self, *args, **kwargs) -> None:
        """Constructor of the transition

        :param State source: source state
        :param Symbol s: symbol
        :param State target: target state
        """
        ...
    @property
    def word(self) -> list[Symbol]:
        ...
    @word.setter
    def word(self, value: list[Symbol]) -> None:
        ...
    @property
    def path(self) -> list[State]:
        ...
    @path.setter
    def path(self, value : list[State]) -> None:
        ...

class Transition:
    """Wrapper over the transitions in NFA."""
    @property
    def source(self) -> State:
        """
        :return: source state of the transition
        """
        ...
    @property
    def symbol(self) -> Symbol:
        """
        :return: symbol for the transition
        """
        ...
    @property
    def target(self) -> State:
        """
        :return: target state of the transition
        """
        ...
    def __init__(self, source: State = 0, s: Symbol = 0, target: State = 0) -> None:
        """Constructor of the transition

        :param State source: source state
        :param Symbol s: symbol
        :param State target: target state
        """
        ...
    def __eq__(self, other: Self) -> bool:
        ...
    def __neq__(self, other: Self) -> bool:
        ...
    def __str__(self) -> str:
        ...
    def __repr__(self) -> str:
        ...

class SymbolPost:
    """Wrapper over pair of symbol and states for transitions"""
    @property
    def symbol(self) -> Symbol:
        ...
    @symbol.setter
    def symbol(self, value: Symbol) -> None:
        ...
    @property
    def targets(self) -> list[State]:
        ...
    @targets.setter
    def targets(self, value) -> None:
        ...
    def __init__(self, symbol: Symbol, states: list[State]) -> None:
        ...
    def __lt__(self, other: Self) -> bool:
        ...
    def __gt__(self, other: Self) -> bool:
        ...
    def __le__(self, other: Self) -> bool:
        ...
    def __ge__(self, other: Self) -> bool:
        ...
    def __eq__(self, other: Self) -> bool:
        ...
    def __neq__(self, other: Self) -> bool:
        ...
    def __str__(self) -> str:
        ...
    def __repr__(self) -> str:
        ...

class Nfa:
    """Wrapper over NFA

    Note: In order to add more properties to Nfa, see `nfa.pxd`, where there is forward declaration
    """
    def __init__(self, state_number: int = 0, alphabet: alph.Alphabet = None, label: Any = None) -> None:
        """Constructor of the NFA.

        :param int state_number: number of states in automaton
        :param alph.Alphabet alphabet: alphabet corresponding to the automaton
        """
        ...
    def deepcopy(self) -> Self:
        """Deepcopy Nfa using C++ copy constructor."""
        ...
    @property
    def label(self) -> Any:
        ...
    @label.setter
    def label(self, value: Any) -> None:
        ...
    @property
    def initial_states(self) -> list[State]:
        ...
    @initial_states.setter
    def initial_states(self, value: list[State]) -> None:
        ...
    @property
    def final_states(self) -> list[State]:
        ...
    @final_states.setter
    def final_states(self, value: list[State]) -> None:
        ...
    def is_state(self, state: State) -> bool:
        """Tests if state is in the automaton

        :param int state: tested state
        :return: true if state is in the automaton
        """
        ...
    def add_new_state(self) -> State:
        """Adds new state to automaton

        :return: number of the state
        """
        ...
    def add_state(self, state: State) -> State:
        """Adds passed state to the automaton.

        :return: number of the state
        """
        ...
    def make_initial_state(self, state: State) -> None:
        """Makes specified state from the automaton initial.

        :param State state: State to be made initial.
        """
        ...
    def make_initial_states(self, states: list[State]) -> None:
        """Makes specified states from the automaton initial.

        :param list states: List of states to be made initial.
        """
        ...
    def has_initial_state(self, st: State) -> bool:
        """Tests if automaton contains given state

        :param State st: tested state
        :return: true if automaton contains given state
        """
        ...
    def remove_initial_state(self, state: State) -> None:
        """Removes state from initial state set of the automaton.

        :param State state: State to be removed from initial states.
        """
        ...
    def clear_initial(self) -> None:
        """Clears initial state set of the automaton."""
        ...
    def make_final_state(self, state: State) -> None:
        """Makes specified state from the automaton final.

        :param State state: State to be made final.
        """
        ...
    def make_final_states(self, states: list[State]) -> None:
        """Makes specified states from the automaton final.

        :param vector[State] states: List of states to be made final.
        """
        ...
    def has_final_state(self, st: State) -> bool:
        """Tests if automaton contains given state

        :param State st: tested state
        :return: true if automaton contains given state
        """
        ...
    def remove_final_state(self, state: State) -> None:
        """Removes state from final state set of the automaton.

        :param State state: State to be removed from final states.
        """
        ...
    def clear_final(self) -> Self:
        """Clears final state set of the automaton."""
        ...
    def unify_initial(self, force_new_state: bool = False) -> Self:
        """Unify initial states into a single new initial state."""
        ...
    def unify_final(self, force_new_state: bool = False) -> Self:
        """Unify final states into a single new final state."""
        ...
    def add_transition_object(self, tr: Transition) -> None:
        """Adds transition to automaton

        :param Transition tr: added transition
        """
        ...
    @overload
    def add_transition(self, source: State, symbol: Symbol, target: State, alphabet: alph.Alphabet = None) -> None:
        """Constructs transition and adds it to automaton

        :param State source: source state
        :param Symbol symbol: symbol
        :param State target: target state
        :param alph.Alphabet alphabet: alphabet of the transition
        """
        ...
    @overload
    def add_transition(self, source: State, symbol: str, target: State, alphabet: alph.Alphabet) -> None:
        """Constructs transition and adds it to automaton

        :param State source: source state
        :param Symbol symbol: symbol
        :param State target: target state
        :param alph.Alphabet alphabet: alphabet of the transition
        """
        ...
    def remove_trans(self, tr: Transition) -> None:
        """Removes transition from the automaton.

        :param Transition tr: Transition to be removed.
        """
        ...
    def remove_trans_raw(self, source: State, symbol: Symbol, target: State) -> None:
        """Constructs transition and removes it from the automaton.

        :param State source: Source state of the transition to be removed.
        :param Symbol symbol: Symbol of the transition to be removed.
        :param State target: Target state of the transition to be removed.
        """
        ...
    def has_transition(self, source: State, symbol: Symbol, target: State) -> bool:
        """Tests if automaton contains transition

        :param State source: source state
        :param Symbol symbol: symbol
        :param State target: target state
        :return: true if automaton contains transition
        """
        ...
    def get_num_of_transitions(self) -> int:
        """Returns number of transitions in automaton

        :return: number of transitions in automaton
        """
        ...
    def clear(self) -> None:
        """Clears all of the internals in the automaton"""
        ...
    def num_of_states(self) -> int:
        """Get the current number of states in the whole automaton.
        :return: The number of states.
        """
        ...
    def iterate(self) -> Iterable[Transition]:
        """Iterates over all transitions

        :return: stream of transitions
        """
        ...
    def get_transitions_from_state(self, state: State) -> list[SymbolPost]:
        """Returns list of SymbolPost for the given state

        :param State state: state for which we are getting the transitions
        :return: SymbolPost
        """
        ...
    def get_transitions_to_state(self, state_to: State) -> list[Transition]:
        """Get transitions leading to state_to (state_to is their target state).

        :return: List mata_nfa.CTrans: List of transitions leading to state_to.
        """
        ...
    def get_trans_as_sequence(self) -> list[Transition]:
        """Get automaton transitions as a sequence.

        TODO: Refactor into a generator.

        :return: List of automaton transitions.
        """
        ...
    def get_trans_from_state_as_sequence(self, source: State) -> list[Transition]:
        """Get automaton transitions from state_from as a sequence.

        TODO: Refactor into a generator.

        :return: List of automaton transitions.
        """
        ...
    def get_useful_states(self) -> set[State]:
        """Get useful states (states which are reachable and terminating at the same time).

        :return: A set of useful states.
        """
        ...
    def get_reachable_states(self) -> set[State]:
        """Get reachable states.

        :return: A set of reachable states.
        """
        ...
    def get_terminating_states(self) -> set[State]:
        """Get terminating states (states with a path from them leading to any final state).

        :return: A set of terminating states.
        """
        ...
    def trim(self) -> Self:
        """Remove inaccessible (unreachable) and not co-accessible (non-terminating) states.

        Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
         starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
         the starting point of a path ending in a final state).
        :return: Self.
        """
        ...
    def trim_with_state_map(self) -> tuple[Self, dict[State, State]]:
        """Remove inaccessible (unreachable) and not co-accessible (non-terminating) states.

        Remove states which are not accessible (unreachable; state is accessible when the state is the endpoint of a path
         starting from an initial state) or not co-accessible (non-terminating; state is co-accessible when the state is
         the starting point of a path ending in a final state).

        :return: Self, State map of original to new states.
        """
        ...
    def concatenate(self, other: Self) -> Self:
        """Concatenate 'self' with 'other' in-place.

        :return: Self
        """
        ...
    def union(self, other: Self) -> Self:
        """Make union of 'self' with 'other' in-place.

        :return: Self
        """
        ...
    def is_lang_empty(self, run: Run = None) -> bool:
        """Check if language of automaton is empty.

        :return: true if the language of NFA is empty; path of states as a counter example if not.
        """
        ...
    def is_deterministic(self) -> bool:
        """Tests if the NFA is determinstic.

        :return: true if the NFA is deterministic.
        """
        ...
    def is_complete(self, alphabet: alph.Alphabet = None) -> bool:
        """Test if automaton is complete.

        :param alph.Alphabet alphabet: Alphabet to check completeness againts. If 'None', self.alphabet is used. If
          self.alphabet is empty, throws an exception.
        :return: true if the automaton is complete.
        """
        ...
    def get_one_letter_aut(self) -> Self:
        """Unify transitions to create a directed graph with at most a single transition between two states (using only
         one letter for all transitions).

        :return: mata_nfa.Nfa: One letter automaton representing a directed graph.
        """
        ...
    def is_epsilon(self, symbol: Symbol) -> bool:
        """Check whether passed symbol is epsilon symbol or not.

        :param Symbol symbol: The symbol to check.
        :return: True if the passed symbol is epsilon symbol, False otherwise.
        """
        ...
    def __str__(self) -> str:
        ...
    def __repr__(self) -> str:
        ...
    def to_dot_file(self, output_file: str = 'aut.dot', output_format: str = 'pdf', decode_ascii_chars: bool = False, use_intervals: bool = False, max_label_length: int = -1) -> None:
        """Transforms the automaton to dot format.

        By default, the result is saved to `aut.dot`, and further to `aut.dot.pdf`.

        One can choose other format that is supported by graphviz format (e.g. pdf or png).

        :param str output_file: name of the output file where the automaton will be stored
        :param str output_format: format of the output file (pdf/png/etc)
        :param bool decode_ascii_chars: whether to decode ascii codes
        :param bool use_intervals: whether to use intervals ([1-3] instead of [1,2,3])
        :param int max_label_lenght: maximum label length (-1 means no limit, 0 means no label)
        """
        ...
    def to_dot_str(self, encoding: str = 'utf-8', decode_ascii_chars: bool = False, use_intervals: bool = False, max_label_lenght: int = -1) -> str:
        """Transforms the automaton to dot format string

        :param str encoding: encoding of the dot string
        :param bool decode_ascii_chars: whether to decode ascii codes
        :param bool use_intervals: whether to use intervals ([1-3] instead of [1,2,3])
        :param int max_label_lenght: maximum label length (-1 means no limit, 0 means no label)
        :return: string with dot representation of the automaton
        """
        ...
    def to_dataframe(self) -> pandas.DataFrame:
        """Transforms the automaton to DataFrame format.

        Transforms the automaton into pandas.DataFrame format,
        that is suitable for further (statistical) analysis.
        The resulting DataFrame is in tabular format, with
        'source', 'symbol' and 'target' as the columns

        :return: automaton represented as a pandas dataframe
        """
        ...
    def to_networkx_graph(self) -> networkx.Graph:
        """Transforms the automaton into networkx.Graph

        Transforms the automaton into networkx.Graph format,
        that is represented as graph with edges (source, target) with
        additional properties.

        Each symbol is added as an property to each edge.

        :return:
        """
        ...
    def post_map_of(self, st: State, alphabet: alph.Alphabet) -> dict[Symbol, set[State]]:
        """Returns mapping of symbols to set of states.

        :param State st: source state
        :param alph.Alphabet alphabet: alphabet of the post
        :return: dictionary mapping symbols to set of reachable states from the symbol
        """
        ...
    def post_of(self, states: list[State], symbol: Symbol) -> set[State]:
        """Returns sets of reachable states from set of states through a symbol

        :param StateSet states: set of states
        :param Symbol symbol: source symbol
        :return: set of reachable states
        """
        ...
    def remove_epsilon_inplace(self, epsilon: Symbol = epsilon()) -> None:
        """Removes transitions which contain epsilon symbol.

        TODO: Possibly there may be issue with setting the size of the automaton beforehand?

        :param Symbol epsilon: Symbol representing the epsilon symbol.
        """
        ...
    def epsilon_symbol_posts(self, state: State, epsilon: Symbol = epsilon()) -> SymbolPost | None:
        """Get epsilon transitions for a state.

        :param state: State to get epsilon transitions for.
        :param epsilon: Epsilon symbol.
        :return: Epsilon transitions if there are any epsilon transitions for the passed state. None otherwise.
        """
        ...
    def is_universal(self, alphabet: alph.Alphabet, params: dict[Literal['algorithm'], str] = None) -> bool:
        """Tests if NFA is universal with regard to the given alphabet.

        :param OnTheFlyAlphabet alphabet: on the fly alphabet.
        :param dict params: additional params to the function, currently supports key 'algorithm',
            which determines used universality test.
        :return: true if NFA is universal.
        """
        ...
    def is_in_lang(self, word: list[Symbol], use_epsilon: bool = False, match_prefix: bool = False) -> bool:
        """Tests if word is in language.

        :param vector[Symbol] word: tested word.
        :param bool use_epsilon: whether the automaton uses epsilon transitions.
        :param bool match_prefix: whether to match prefix of the word.
        :return: true if word is in language of the NFA.
        """
        ...
    def get_word_for_path(self, path: list[State]) -> tuple[list[Symbol], bool]:
        """For a given path (set of states) returns a corresponding word

        >>> lhs.get_word_for_path([0, 1, 2])
        ([1, 1], True)

        :param Nfa lhs: source automaton
        :param list path: list of states
        :return: pair of word (list of symbols) and true or false, whether the search was successful
        """
        ...
    def make_complete(self, sink_state: State, alphabet: alph.Alphabet) -> None:
        """Makes NFA complete.

        :param Symbol sink_state: sink state of the automaton
        :param OnTheFlyAlphabet alphabet: alphabet to make complete against.
        """
        ...
    def get_symbols(self) -> set[Symbol]:
        """Return a set of symbols used on the transitions in NFA.

        :return: Set of symbols.
        """
        ...

# Operations
def determinize_with_subset_map(lhs: Nfa) -> tuple[Nfa, dict[tuple[State], State]]:
    """Determinize the lhs automaton

    :param Nfa lhs: non-deterministic finite automaton
    :return: deterministic finite automaton, subset map
    """
    ...

def determinize(lhs: Nfa) -> Nfa:
    """Determinize the lhs automaton

    :param Nfa lhs: non-deterministic finite automaton
    :return: deterministic finite automaton
    """
    ...

def union(lhs: Nfa, rhs: Nfa) -> Nfa:
    """Performs union of lhs and rhs

    :param Nfa lhs: first automaton
    :param Nfa rhs: second automaton
    :return: union of lhs and rhs
    """
    ...

def intersection(lhs: Nfa, rhs: Nfa, first_epsilon: Symbol = epsilon()) -> Nfa:
    """Performs intersection of lhs and rhs.

    Supports epsilon symbols when preserve_epsilon is set to True.

    When computing intersection preserving epsilon transitions, create product of two NFAs, where both automata can contain ε-transitions. The product preserves the ε-transitions
     of both automata. This means that for each ε-transition of the form `s-ε->p` and each product state `(s,a)`,
     an ε-transition `(s,a)-ε->(p,a)` is created. Furthermore, for each ε-transition `s-ε->p` and `a-ε->b`,
     a product state `(s,a)-ε->(p,b)` is created.

    Automata must share alphabets.

    :param Nfa lhs: First automaton.
    :param Nfa rhs: Second automaton.
    :param Symbol first_epsilon: the smallest epsilon.
    :return: Intersection of lhs and rhs.
    """

def intersection_with_product_map(lhs: Nfa, rhs: Nfa, first_epsilon: Symbol = epsilon()) -> tuple[Nfa, dict[tuple[State, State], State]]:
    """Performs intersection of lhs and rhs.

    Supports epsilon symbols when preserve_epsilon is set to True.

    When computing intersection preserving epsilon transitions, create product of two NFAs, where both automata can contain ε-transitions. The product preserves the ε-transitions
     of both automata. This means that for each ε-transition of the form `s-ε->p` and each product state `(s,a)`,
     an ε-transition `(s,a)-ε->(p,a)` is created. Furthermore, for each ε-transition `s-ε->p` and `a-ε->b`,
     a product state `(s,a)-ε->(p,b)` is created.

    Automata must share alphabets.

    :param preserve_epsilon: Whether to compute intersection preserving epsilon transitions.
    :param Nfa lhs: First automaton.
    :param Nfa rhs: Second automaton.
    :return: Intersection of lhs and rhs, product map of original pairs of states to new states.
    """
    ...

def concatenate(lhs: Nfa, rhs: Nfa, use_epsilon: bool = False) -> Nfa:
    """Concatenate two NFAs.

    Supports epsilon symbols when @p use_epsilon is set to true.
    :param Nfa lhs: First automaton to concatenate.
    :param Nfa rhs: Second automaton to concatenate.
    :param use_epsilon: Whether to concatenate over an epsilon symbol.
    :return: Nfa: Concatenated automaton.
    """
    ...

def concatenate_with_result_state_maps(lhs: Nfa, rhs: Nfa, use_epsilon: bool = False) \
        -> tuple[Nfa, dict[str, str], dict[str, str]]:
    """Concatenate two NFAs.

    :param Nfa lhs: First automaton to concatenate.
    :param Nfa rhs: Second automaton to concatenate.
    :param use_epsilon: Whether to concatenate over an epsilon symbol.
    :return: Nfa: Concatenated automaton.
    """
    ...

def complement(nfa: Nfa, alphabet: alph.Alphabet, params: dict[Literal['algorithm'], Literal['classical', 'brzozowski']] = None) -> Nfa:
    """Computes the complement of the nfa.

    :param Nfa nfa: The automaton to complement.
    :param OnTheFlyAlphabet alphabet: The alphabet of the nfa.
    :param dict params: Additional parameters.
      - "algorithm":
        - "classical": The classical algorithm determinizes the automaton, makes it complete and swaps final and
                        non-final states.
        - "brzozowski": The Brzozowski algorithm determinizes the automaton using Brzozowski minimization, makes it
                         complete, and swaps final and non-final states.
    :return: The complemented automaton.
    """
    ...

def revert(lhs: Nfa) -> Nfa:
    """Reverses transitions in the lhs

    :param Nfa lhs: source automaton
    :return: automaton with reversed transitions
    """
    ...

def remove_epsilon(lhs: Nfa, epsilon: Symbol = epsilon()) -> Nfa:
    """Removes transitions that contain epsilon symbol.

    :param Nfa lhs: Automaton, where epsilon transitions will be removed.
    :param Symbol epsilon: Symbol representing the epsilon.
    :return: Nfa: Automaton with epsilon transitions removed.
    """
    ...

def minimize(lhs: Nfa, params: dict[Literal['algorithm'], Literal['brzozowski', 'hopcroft']] = None) -> Nfa:
    """Minimizes the automaton lhs

    :param Nfa lhs: automaton to be minimized
    :param Dict params: Additional parameters for the minimization operation:
      - "algorithm":
        - "brzozowski": The Brzozowski minimization algorithm.
        - "hopcroft": The Hopcroft minimization algorithm, only works on trimmed (no useless states) DFAs as input.
    :return: minimized automaton
    """
    ...

def reduce_with_state_map(aut: Nfa, params: dict[Literal['algorithm'], Literal['simulation']] = None) -> tuple[Nfa, dict[State, State]]:
    """Reduce the automaton.

    :param Nfa aut: Original automaton to reduce.
    :param Dict params: Additional parameters for the reduction algorithm:
        - "algorithm":
          - "simulation": Reduce size by simulation.
    :return: (Reduced automaton, state map of original to new states)
    """
    ...

class _ResidualReduceParams(TypedDict):
    algorithm: Literal['residual']
    type: Literal['after', 'with']
    direction: Literal['forward', 'backward']

def reduce(aut: Nfa, params: dict[Literal['algorithm'], Literal['simulation']] | _ResidualReduceParams = None) -> Nfa:
    """Reduce the automaton.

    :param Nfa aut: Original automaton to reduce.
    :param Dict params: Additional parameters for the reduction algorithm:
        - "algorithm":
          - "simulation": Reduce size by simulation.
          - "residual": Reduce size by residual construction.
        - "type":
          - "after": (Only for "algorithm": "residual") Residual construction after the last determinization.
          - "with": (Only for "algorithm": "residual") Residual construction during the last determinization.
        - "direction":
          - "forward": (Only for "algorithm": "residual") Forward residual construction.
          - "backward": (Only for "algorithm": "residual") Backward residual construction.
    :return: Reduced automaton.
    """
    ...

def reduce_residual_after(aut: Nfa) -> Nfa:
    """Reduce the automaton by residual construction after the last determinization.

    :param Nfa aut: Original automaton to reduce.
    :return: Reduced automaton.
    """
    ...

def reduce_residual_with(aut: Nfa) -> Nfa:
    """Reduce the automaton by residual construction during the last determinization.

    :param Nfa aut: Original automaton to reduce.
    :return: Reduced automaton.
    """
    ...

def compute_relation(lhs: Nfa, params: dict[str, str] = None) -> BinaryRelation:
    """Computes the relation for the automaton

    :param Nfa lhs: automaton
    :param Dict params: parameters of the computed relation
    :return: computd relation
    """
    ...

# Tests
def is_included_with_cex(lhs: Nfa, rhs: Nfa, alphabet: alph.Alphabet = None, params: dict[str, str] = None) -> tuple[bool, Run]:
    """Test inclusion between two automata

    :param Nfa lhs: smaller automaton
    :param Nfa rhs: bigger automaton
    :param alph.Alphabet alphabet: alpabet shared by two automata
    :param dict params: additional params
    :return: true if lhs is included by rhs, counter example word if not
    """
    ...

def is_included(lhs: Nfa, rhs: Nfa, alphabet: alph.Alphabet = None, params: dict[str, str] = None) -> bool:
    """Test inclusion between two automata

    :param Nfa lhs: smaller automaton
    :param Nfa rhs: bigger automaton
    :param alph.Alphabet alphabet: alpabet shared by two automata
    :param dict params: additional params
    :return: true if lhs is included by rhs, counter example word if not
    """
    ...

def equivalence_check(lhs: Nfa, rhs: Nfa, alphabet: alph.Alphabet = None, params: dict[str, str] = None) -> bool:
    """Test equivalence of two automata.

:param Nfa lhs: Smaller automaton.
:param Nfa rhs: Bigger automaton.
:param alph.Alphabet alphabet: Alphabet shared by two automata.
:param dict params: Additional params:
- "algorithm": "antichains"
:return: True if lhs is equivalent to rhs, False otherwise.
"""
    ...

def accepts_epsilon(lhs: Nfa) -> bool:
    """Tests if automaton accepts epsilon

    :param Nfa lhs: tested automaton
    :return: true if automaton accepts epsilon
    """
    ...

# Helper functions
def encode_word(alphabet: alph.Alphabet, word: Iterable[str]) -> list[Symbol]:
    """Encodes word based on a passed alphabet

    >>> mata_nfa.Nfa.encode_word(OnTheFlyAlphabet.from_symbol_map({'a': 1, 'b': 2, "c": 0}), "abca")
    [1, 2, 0, 1]

    :param alph.Alphabet alphabet: Alphabet to encode the word with.
    :param word: list of strings representing an encoded word.
    :return: Encoded word.
    """
    ...

# Temporary for testing
def divisible_by(k: int) -> Nfa:
    """Constructs automaton accepting strings containing ones divisible by "k"."""
    ...

def run_safely_external_command(cmd: str, check_results: bool = True, quiet: bool = True, timeout: int = None, **kwargs) -> tuple[str, str] | tuple[bytes, bytes]:
    """Safely runs the piped command, without executing of the shell

    Courtesy of: https://blog.avinetworks.com/tech/python-best-practices

    :param str cmd: string with command that we are executing
    :param bool check_results: check correct command exit code and raise exception in case of fail
    :param bool quiet: if set to False, then it will print the output of the command
    :param int timeout: timeout of the command
    :param dict kwargs: additional args to subprocess call
    :return: returned standard output and error
    :raises subprocess.CalledProcessError: when any of the piped commands fails
    """
    ...

def get_elements_from_bool_vec(bool_vec: list[int]) -> set[State]:
    ...

def store() -> dict[str, None | dict[str, str]]:
    ...
