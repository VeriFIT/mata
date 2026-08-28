from enum import IntEnum
from typing import Self

from libmata.nfa.nfa import State

Symbol = int
Level = int

class Alphabet:
    """Base class for alphabets.

    A bare `Alphabet()` acts as a generic wrapper delegating through the virtual `mata::Alphabet` interface; it is
    used as a fallback for alphabet instances whose concrete type is not one of the known subclasses (`IntAlphabet`,
    `OnTheFlyAlphabet`, `EnumAlphabet`).
    """
    def __init__(self, *args, **kwargs) -> None: ...
    def translate_symbol(self, symbol: str) -> Symbol: ...
    def reverse_translate_symbol(self, symbol: Symbol) -> str: ...
    def get_alphabet_symbols(self) -> set[Symbol]:
        """Returns a set of supported symbols."""
    def get_complement(self, symbols: set[Symbol]) -> set[Symbol]:
        """Returns the complement of `symbols` with respect to this alphabet."""
    def is_empty(self) -> bool:
        """Checks whether the alphabet has any symbols."""
    def clear(self) -> None:
        """Clears the alphabet."""
    def __eq__(self, other: object) -> bool: ...

class OnTheFlyAlphabet(Alphabet):
    """OnTheFlyAlphabet represents alphabet that is not known beforehand and is constructed on-the-fly."""
    def __init__(self, initial_symbol: State = 0) -> None: ...
    @classmethod
    def from_symbol_map(cls, symbol_map: dict[str, int]) -> Self:
        """Create on the fly alphabet filled with symbol_map.

        :param symbol_map: Map mapping symbol names to symbol values.
        :return: On the fly alphabet.
        """
    @classmethod
    def for_symbol_names(cls, symbol_map: list[str]) -> Self: ...
    def add_symbols_from_symbol_map(self, symbol_map: dict[str, int]) -> None:
        """Add symbols from symbol_map to the current alphabet.

        :param symbol_map: Map mapping strings to symbols.
        """
    def add_symbols_for_names(self, symbol_names: list[str]) -> None:
        """Add symbols for symbol names to the current alphabet.

        :param symbol_names: Vector of symbol names.
        """
    def get_symbol_map(self) -> dict[str, int]:
        """Get map mapping strings to symbols.

        :return: Map of strings to symbols.
        """
    def translate_symbol(self, symbol: str) -> Symbol:
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
    def reverse_translate_symbol(self, symbol: Symbol) -> str:
        """Translate internal symbol value to the original symbol name.

        Throw an exception when the symbol is missing in the alphabet.
        :param Symbol symbol: Internal symbol value to be translated.
        :return str: Original symbol string name.
        """
    def get_alphabet_symbols(self) -> set[Symbol]:
        """Returns a set of supported symbols.

        :return: Set of supported symbols.
        """

class IntAlphabet(Alphabet):
    """IntAlphabet represents integer alphabet that directly maps integer string to their values."""
    def __init__(self, *args, **kwargs) -> None: ...
    def translate_symbol(self, symbol: str) -> Symbol:
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
    def reverse_translate_symbol(self, symbol: Symbol) -> str:
        """Translate internal symbol value to the original symbol name.

        :param Symbol symbol: Internal symbol value to be translated.
        :return str: Original symbol string name.
        """

class EnumAlphabet(Alphabet):
    """EnumAlphabet is a version of a direct (identity) alphabet that maintains an explicit, ordered set of symbols.

    Unlike `IntAlphabet`, `get_alphabet_symbols()` and `get_complement()` are meaningful on `EnumAlphabet`.
    """
    def __init__(self, symbols: set[Symbol] | None = None) -> None: ...
    def translate_symbol(self, symbol: str) -> Symbol: ...
    def reverse_translate_symbol(self, symbol: Symbol) -> str: ...
    def get_alphabet_symbols(self) -> set[Symbol]: ...
    def add_symbols_from(self, symbols: set[Symbol]) -> None:
        """Expand the alphabet by symbols from `symbols`. Adding a symbol which already exists raises an exception."""
    def add_new_symbol(self, symbol: str | Symbol) -> None:
        """Add a new symbol to the alphabet, either by its string representation or its numeric value directly."""
    def get_next_value(self) -> Symbol:
        """Get the next value that would be used for a newly added symbol."""
    def get_number_of_symbols(self) -> int:
        """Get the number of existing symbols."""
    def erase(self, symbol: Symbol) -> int:
        """Erase `symbol` from the alphabet. Returns the number of symbols erased (0 or 1)."""

class AlphabetLevelsMode(IntEnum):
    """Operating mode of an `AlphabetLevels` instance."""

    Global = 0
    MultiLevel = 1

class AlphabetLevels:
    """Per-level alphabets for transducer-like automata (e.g., `Nft`).

    Operates in one of two modes (see `AlphabetLevelsMode`):
      - `Global`: a single shared alphabet applies to every level; the level argument is ignored.
      - `MultiLevel`: distinct per-level alphabets; the level argument is required.
    """
    def __init__(
        self, alphabets: list[Alphabet | None] | None = None, mode: AlphabetLevelsMode | None = None
    ) -> None: ...
    @staticmethod
    def global_mode(alphabet: Alphabet) -> AlphabetLevels:
        """Create an `AlphabetLevels` sharing a single alphabet across every level."""
    @staticmethod
    def multi_level_mode(alphabets: list[Alphabet | None] | None = None) -> AlphabetLevels:
        """Create an `AlphabetLevels` with distinct per-level alphabets, indexed by level."""
    @property
    def mode(self) -> AlphabetLevelsMode:
        """Current operating mode."""
    def set_global_mode(self, level_for_kept_alphabet_or_alphabet: Level | Alphabet = 0) -> None:
        """Switch to `Global` mode."""
    def set_multi_level_mode(self, alphabets: list[Alphabet | None] | None = None) -> None:
        """Switch to `MultiLevel` mode, optionally replacing the per-level alphabets with `alphabets`."""
    def __len__(self) -> int: ...
    def size(self) -> int:
        """Number of alphabet slots currently stored."""
    def __eq__(self, other: object) -> bool: ...
    def for_level(self, level: Level | None = None) -> Alphabet:
        """Get the alphabet assigned to `level` (mode-aware, validated)."""
    def __getitem__(self, level: Level | None) -> Alphabet: ...
    def at(self, index: int) -> Alphabet | None:
        """Raw, mode-agnostic slot access by index. Unlike `for_level`, a null entry is returned as `None`."""
    def __iter__(self): ...
    def push_back(self, alphabet: Alphabet | None) -> None:
        """Append `alphabet` as the new last level."""
    def insert(self, index: int, alphabet: Alphabet | None) -> None:
        """Insert `alphabet` before `index`, shifting subsequent levels up."""
    def pop_back(self) -> None:
        """Remove the last level's alphabet."""
    def reserve(self, new_cap: int) -> None:
        """Reserve storage for at least `new_cap` levels."""
    def resize(self, count: int) -> None:
        """Resize the number of levels, leaving any newly added slots empty."""
    def erase(self, index: int, stop: int | None = None) -> None:
        """Remove the alphabet at `index`, or the range [`index`, `stop`) when `stop` is given."""
    def translate_symbol(self, symbol: str, level: Level | None = None) -> Symbol:
        """Translate `symbol` using the alphabet for `level` (see `for_level` for level resolution)."""
    def reverse_translate_symbol(self, symbol: Symbol, level: Level | None = None) -> str:
        """Translate `symbol` back to its name using the alphabet for `level`."""
    def get_alphabet_symbols(self, level: Level | None = None) -> set[Symbol]:
        """Get the symbols known to the alphabet on `level`."""
    def get_complement(self, symbols: set[Symbol], level: Level | None = None) -> set[Symbol]:
        """Get the complement of `symbols` with respect to the alphabet for `level`."""
    def is_empty(self, level: Level | None = None) -> bool:
        """Check whether the alphabet for `level` is empty."""
    def clear(self, level: Level | None = None) -> None:
        """Clear the alphabet for `level`."""
