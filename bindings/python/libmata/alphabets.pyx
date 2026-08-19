cimport libmata.alphabets as alph

from enum import IntEnum

from cython.operator cimport dereference as deref

from libcpp cimport bool
from libcpp.memory cimport shared_ptr, make_shared, static_pointer_cast, dynamic_pointer_cast
from libcpp.optional cimport optional
from libcpp.vector cimport vector
from libmata.alphabets cimport (
    CAlphabet, CIntAlphabet, CEnumAlphabet, COnTheFlyAlphabet,
    CAlphabetLevels, CAlphabetLevelsMode,
    AlphabetLevelsModeGlobal, AlphabetLevelsModeMultiLevel,
    Level,
)
from libmata.nfa.nfa cimport State


class AlphabetLevelsMode(IntEnum):
    """Operating mode of an :class:`AlphabetLevels` instance."""
    Global = <int>AlphabetLevelsModeGlobal
    MultiLevel = <int>AlphabetLevelsModeMultiLevel


cdef object wrap_alphabet(shared_ptr[CAlphabet] ptr):
    """Wrap a `shared_ptr[CAlphabet]` coming back from C++ in the right concrete Python `Alphabet` subclass.

    Returns `None` for a null pointer. Falls back to a minimal generic `Alphabet` wrapper (delegating through the
    virtual base-class interface) if the pointee is not one of the concrete alphabet types known to the bindings.
    """
    if ptr.get() == NULL:
        return None

    cdef shared_ptr[CIntAlphabet] int_ptr = dynamic_pointer_cast[CIntAlphabet, CAlphabet](ptr)
    if int_ptr.get() != NULL:
        result = IntAlphabet.__new__(IntAlphabet)
        (<IntAlphabet>result).thisptr = int_ptr
        return result

    cdef shared_ptr[COnTheFlyAlphabet] otf_ptr = dynamic_pointer_cast[COnTheFlyAlphabet, CAlphabet](ptr)
    if otf_ptr.get() != NULL:
        result = OnTheFlyAlphabet.__new__(OnTheFlyAlphabet)
        (<OnTheFlyAlphabet>result).thisptr = otf_ptr
        return result

    cdef shared_ptr[CEnumAlphabet] enum_ptr = dynamic_pointer_cast[CEnumAlphabet, CAlphabet](ptr)
    if enum_ptr.get() != NULL:
        result = EnumAlphabet.__new__(EnumAlphabet)
        (<EnumAlphabet>result).thisptr = enum_ptr
        return result

    result = Alphabet.__new__(Alphabet)
    (<Alphabet>result)._generic_thisptr = ptr
    return result


cdef CAlphabet* unwrap_alphabet_or_null(Alphabet alphabet):
    """Get the underlying `CAlphabet*` of `alphabet`, or `NULL` if `alphabet` is `None`.

    Convenience for the common "explicit alphabet overrides a resolved default" pattern used throughout the NFA/NFT
    bindings (`resolve_alphabet`, `make_complete`, `is_included`, ...).
    """
    if alphabet is None:
        return NULL
    return alphabet.as_base().get()


cdef class Alphabet:
    """Base class for alphabets.

    A bare `Alphabet()` acts as a generic wrapper delegating through the virtual `mata::Alphabet` interface; it is
    used as a fallback by `wrap_alphabet()` for alphabet instances whose concrete type is not one of the known
    subclasses (`IntAlphabet`, `OnTheFlyAlphabet`, `EnumAlphabet`).
    """

    def __cinit__(self):
        pass

    cdef shared_ptr[CAlphabet] as_base(self):
        return self._generic_thisptr

    def _check_initialized(self):
        if self.as_base().get() == NULL:
            raise RuntimeError("Alphabet is not initialized.")

    def translate_symbol(self, str symbol):
        self._check_initialized()
        return self.as_base().get().translate_symb(symbol.encode('utf-8'))

    def reverse_translate_symbol(self, Symbol symbol) -> str:
        self._check_initialized()
        return self.as_base().get().reverse_translate_symbol(symbol).decode('utf-8')

    cpdef get_alphabet_symbols(self):
        """Returns a set of supported symbols.

        :return: Set of supported symbols.
        """
        self._check_initialized()
        cdef COrdVector[Symbol] symbols = self.as_base().get().get_alphabet_symbols()
        return {s for s in symbols}

    cdef get_symbols(self):
        return self.get_alphabet_symbols()

    def get_complement(self, symbols):
        """Returns the complement of `symbols` with respect to this alphabet.

        :param symbols: Set of symbols to complement.
        :return: Complement of `symbols`.
        """
        self._check_initialized()
        cdef vector[Symbol] c_symbols_vec = sorted(symbols)
        cdef COrdVector[Symbol] c_symbols = COrdVector[Symbol](c_symbols_vec)
        cdef COrdVector[Symbol] result = self.as_base().get().get_complement(c_symbols)
        return {s for s in result}

    def is_empty(self) -> bool:
        """Checks whether the alphabet has any symbols."""
        self._check_initialized()
        return self.as_base().get().empty()

    def clear(self) -> None:
        """Clears the alphabet."""
        self._check_initialized()
        self.as_base().get().clear()

    def __eq__(self, other):
        if not isinstance(other, Alphabet):
            return NotImplemented
        if self.as_base().get() == NULL or (<Alphabet>other).as_base().get() == NULL:
            return self.as_base().get() == (<Alphabet>other).as_base().get()
        return self.as_base().get().is_equal(deref((<Alphabet>other).as_base().get()))

    def __dealloc__(self):
        pass


cdef class OnTheFlyAlphabet(Alphabet):
    """OnTheFlyAlphabet represents alphabet that is not known before hand and is constructed on-the-fly."""

    cdef shared_ptr[COnTheFlyAlphabet] thisptr

    def __cinit__(self, State initial_symbol = 0):
        self.thisptr = make_shared[COnTheFlyAlphabet](initial_symbol)

    @classmethod
    def from_symbol_map(cls, symbol_map: dict[str, int]) -> OnTheFlyAlphabet:
        """Create on the fly alphabet filled with symbol_map.

        :param symbol_map: Map mapping symbol names to symbol values.
        :return: On the fly alphabet.
        """
        alphabet = cls()
        alphabet.add_symbols_from_symbol_map(symbol_map)
        return alphabet

    @classmethod
    def for_symbol_names(cls, symbol_map: list[str]) -> OnTheFlyAlphabet:
        alphabet = cls()
        alphabet.add_symbols_for_names(symbol_map)
        return alphabet

    def add_symbols_from_symbol_map(self, symbol_map: dict[str, int]) -> None:
        """Add symbols from symbol_map to the current alphabet.

        :param symbol_map: Map mapping strings to symbols.
        """
        cdef COnTheFlyAlphabet.StringToSymbolMap c_symbol_map
        for symbol, value in symbol_map.items():
            c_symbol_map[symbol.encode('utf-8')] = value
        self.thisptr.get().add_symbols_from(<COnTheFlyAlphabet.StringToSymbolMap>c_symbol_map)

    def add_symbols_for_names(self, symbol_names: list[str]) -> None:
        """Add symbols for symbol names to the current alphabet.

        :param symbol_names: Vector of symbol names.
        """
        cdef vector[string] c_symbol_names
        for symbol_name in symbol_names:
            c_symbol_names.push_back(symbol_name.encode('utf-8'))
        self.thisptr.get().add_symbols_from(c_symbol_names)

    def get_symbol_map(self) -> dict[str, int]:
        """Get map mapping strings to symbols.

        :return: Map of strings to symbols.
        """
        cdef umap[string, Symbol] c_symbol_map = self.thisptr.get().get_symbol_map()
        symbol_map = {}
        for symbol, value in c_symbol_map:
            symbol_map[symbol.decode('utf-8')] = value
        return symbol_map

    def translate_symbol(self, str symbol):
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
        return self.thisptr.get().translate_symb(symbol.encode('utf-8'))

    def reverse_translate_symbol(self, Symbol symbol) -> str:
        """Translate internal symbol value to the original symbol name.

        Throw an exception when the symbol is missing in the alphabet.
        :param Symbol symbol: Internal symbol value to be translated.
        :return str: Original symbol string name.
        """
        return self.thisptr.get().reverse_translate_symbol(symbol).decode('utf-8')

    cpdef get_alphabet_symbols(self):
        """Returns a set of supported symbols.

        :return: Set of supported symbols.
        """
        cdef COrdVector[Symbol] symbols = self.thisptr.get().get_alphabet_symbols()
        return {s for s in symbols}

    cdef shared_ptr[CAlphabet] as_base(self):
        """Retypes the alphabet to its base class

        :return: alphabet as shared_ptr[CAlphabet]
        """
        return static_pointer_cast[CAlphabet, COnTheFlyAlphabet](self.thisptr)


cdef class IntAlphabet(Alphabet):
    """IntAlphabet represents integer alphabet that directly maps integer string to their values."""

    cdef shared_ptr[CIntAlphabet] thisptr

    def __cinit__(self):
        self.thisptr = make_shared[CIntAlphabet]()

    def translate_symbol(self, str symbol):
        """Translates symbol to the position of the seen values

        :param str symbol: translated symbol
        :return: order of the symbol as was seen during the construction
        """
        return self.thisptr.get().translate_symb(symbol.encode('utf-8'))

    def reverse_translate_symbol(self, Symbol symbol) -> str:
        """Translate internal symbol value to the original symbol name.

        :param Symbol symbol: Internal symbol value to be translated.
        :return str: Original symbol string name.
        """
        return self.thisptr.get().reverse_translate_symbol(symbol).decode('utf-8')

    cdef shared_ptr[CAlphabet] as_base(self):
        """Retypes the alphabet to its base class

        :return: alphabet as shared_ptr[CAlphabet]
        """
        return static_pointer_cast[CAlphabet, CIntAlphabet](self.thisptr)


cdef class EnumAlphabet(Alphabet):
    """EnumAlphabet is a version of a direct (identity) alphabet that maintains an explicit, ordered set of symbols.

    Unlike `IntAlphabet`, `get_alphabet_symbols()` and `get_complement()` are meaningful on `EnumAlphabet`.
    """

    def __cinit__(self, symbols: set[Symbol] | None = None):
        cdef vector[Symbol] c_symbols_vec
        cdef COrdVector[Symbol] c_symbols
        if symbols is None:
            self.thisptr = make_shared[CEnumAlphabet]()
        else:
            c_symbols_vec = sorted(symbols)
            c_symbols = COrdVector[Symbol](c_symbols_vec)
            self.thisptr = make_shared[CEnumAlphabet](c_symbols)

    # translate_symbol()/reverse_translate_symbol() are inherited from Alphabet unchanged: both dispatch through
    #  self.as_base(), which this class already overrides below to point at the right underlying CEnumAlphabet.

    cpdef get_alphabet_symbols(self):
        """Returns a set of supported symbols.

        :return: Set of supported symbols.
        """
        cdef COrdVector[Symbol] symbols = self.thisptr.get().get_alphabet_symbols()
        return {s for s in symbols}

    def add_symbols_from(self, symbols: set[Symbol]) -> None:
        """Expand the alphabet by symbols from `symbols`.

        Adding a symbol which already exists raises an exception.
        :param symbols: Symbols to add.
        """
        cdef vector[Symbol] c_symbols_vec = sorted(symbols)
        cdef COrdVector[Symbol] c_symbols = COrdVector[Symbol](c_symbols_vec)
        self.thisptr.get().add_symbols_from(c_symbols)

    def add_new_symbol(self, symbol: str | Symbol) -> None:
        """Add a new symbol to the alphabet.

        :param symbol: Either the string representation of the symbol (its value is derived automatically), or the
          symbol's numeric value directly.
        """
        cdef string c_symbol
        if isinstance(symbol, str):
            c_symbol = symbol.encode('utf-8')
            self.thisptr.get().add_new_symbol(c_symbol)
        else:
            self.thisptr.get().add_new_symbol(<Symbol>symbol)

    def get_next_value(self) -> Symbol:
        """Get the next value that would be used for a newly added symbol."""
        return self.thisptr.get().get_next_value()

    def get_number_of_symbols(self) -> int:
        """Get the number of existing symbols."""
        return self.thisptr.get().get_number_of_symbols()

    def erase(self, Symbol symbol) -> int:
        """Erase `symbol` from the alphabet.

        :return: Number of symbols erased (0 or 1).
        """
        return self.thisptr.get().erase(symbol)

    cdef shared_ptr[CAlphabet] as_base(self):
        """Retypes the alphabet to its base class

        :return: alphabet as shared_ptr[CAlphabet]
        """
        return static_pointer_cast[CAlphabet, CEnumAlphabet](self.thisptr)


cdef class AlphabetLevels:
    """Per-level alphabets for transducer-like automata (e.g., `Nft`).

    Operates in one of two modes (see `AlphabetLevelsMode`):
      - `Global`: a single shared alphabet applies to every level; the level argument is ignored.
      - `MultiLevel`: distinct per-level alphabets; the level argument is required.
    """

    def __cinit__(self, alphabets: list[Alphabet | None] | None = None, mode: AlphabetLevelsMode | None = None):
        cdef vector[shared_ptr[CAlphabet]] c_alphabets
        cdef Alphabet a
        for a in (alphabets or []):
            c_alphabets.push_back(a.as_base() if a is not None else shared_ptr[CAlphabet]())
        cdef CAlphabetLevelsMode c_mode = (
            AlphabetLevelsModeMultiLevel if mode is None else <CAlphabetLevelsMode><int>mode
        )
        self.thisptr = make_shared[CAlphabetLevels](c_alphabets, c_mode)

    @staticmethod
    def global_mode(Alphabet alphabet) -> AlphabetLevels:
        """Create an `AlphabetLevels` sharing a single alphabet across every level."""
        result = AlphabetLevels.__new__(AlphabetLevels)
        (<AlphabetLevels>result).thisptr = make_shared[CAlphabetLevels](alphabet.as_base())
        return result

    @staticmethod
    def multi_level_mode(alphabets: list[Alphabet | None] | None = None) -> AlphabetLevels:
        """Create an `AlphabetLevels` with distinct per-level alphabets, indexed by level."""
        return AlphabetLevels(alphabets, AlphabetLevelsMode.MultiLevel)

    @property
    def mode(self) -> AlphabetLevelsMode:
        """Current operating mode."""
        return AlphabetLevelsMode(<int>self.thisptr.get().mode())

    def set_global_mode(self, level_for_kept_alphabet_or_alphabet=0) -> None:
        """Switch to `Global` mode.

        :param level_for_kept_alphabet_or_alphabet: Either the level whose alphabet to keep (dropping all others),
          or an `Alphabet` to use as the new single shared alphabet for every level.
        """
        if isinstance(level_for_kept_alphabet_or_alphabet, Alphabet):
            self.thisptr.get().set_global_mode((<Alphabet>level_for_kept_alphabet_or_alphabet).as_base())
        else:
            self.thisptr.get().set_global_mode(<Level>level_for_kept_alphabet_or_alphabet)

    def set_multi_level_mode(self, alphabets: list[Alphabet | None] | None = None) -> None:
        """Switch to `MultiLevel` mode, optionally replacing the per-level alphabets with `alphabets`."""
        cdef vector[shared_ptr[CAlphabet]] c_alphabets
        cdef Alphabet a
        if alphabets is None:
            self.thisptr.get().set_multi_level_mode()
            return
        for a in alphabets:
            c_alphabets.push_back(a.as_base() if a is not None else shared_ptr[CAlphabet]())
        self.thisptr.get().set_multi_level_mode(c_alphabets)

    def __len__(self) -> int:
        return self.thisptr.get().size()

    def size(self) -> int:
        """Number of alphabet slots currently stored."""
        return self.thisptr.get().size()

    def __eq__(self, other):
        if not isinstance(other, AlphabetLevels):
            return NotImplemented
        return deref(self.thisptr.get()) == deref((<AlphabetLevels>other).thisptr.get())

    def for_level(self, level: int | None = None) -> Alphabet:
        """Get the alphabet assigned to `level` (mode-aware, validated).

        In `Global` mode, the single shared alphabet is returned regardless of `level`. In `MultiLevel` mode,
        `level` is required and must index a non-null slot.
        """
        cdef size_t size = self.thisptr.get().size()
        cdef size_t idx
        cdef shared_ptr[CAlphabet] slot
        if self.thisptr.get().mode() == AlphabetLevelsModeGlobal:
            if size == 0:
                raise RuntimeError("AlphabetLevels (Global) has no underlying alphabet.")
            slot = self.thisptr.get().at(0)
            if slot.get() == NULL:
                raise RuntimeError("AlphabetLevels (Global) has no underlying alphabet.")
            return wrap_alphabet(slot)

        if level is None:
            raise RuntimeError("AlphabetLevels (MultiLevel) requires an explicit level.")
        idx = <size_t><Level>level
        if idx >= size:
            raise RuntimeError(f"AlphabetLevels has no alphabet for level {level} (out of range).")
        slot = self.thisptr.get().at(idx)
        if slot.get() == NULL:
            raise RuntimeError(f"AlphabetLevels has no alphabet for level {level} (entry is null).")
        return wrap_alphabet(slot)

    def __getitem__(self, level: int | None) -> Alphabet:
        return self.for_level(level)

    def at(self, size_t index) -> Alphabet | None:
        """Raw, mode-agnostic slot access by index. Unlike `for_level`, a null entry is returned as `None`."""
        return wrap_alphabet(self.thisptr.get().at(index))

    def __iter__(self):
        cdef size_t i
        cdef size_t size = self.thisptr.get().size()
        for i in range(size):
            yield wrap_alphabet(self.thisptr.get().at(i))

    def push_back(self, Alphabet alphabet) -> None:
        """Append `alphabet` as the new last level."""
        self.thisptr.get().push_back(alphabet.as_base() if alphabet is not None else shared_ptr[CAlphabet]())

    def insert(self, size_t index, Alphabet alphabet) -> None:
        """Insert `alphabet` before `index`, shifting subsequent levels up."""
        cdef size_t size = self.thisptr.get().size()
        if index > size:
            raise IndexError(f"insert index {index} out of range for AlphabetLevels of size {size}")
        cdef vector[shared_ptr[CAlphabet]].iterator it = self.thisptr.get().begin() + index
        self.thisptr.get().insert(it, alphabet.as_base() if alphabet is not None else shared_ptr[CAlphabet]())

    def pop_back(self) -> None:
        """Remove the last level's alphabet."""
        if self.thisptr.get().size() == 0:
            raise IndexError("pop_back from an empty AlphabetLevels")
        self.thisptr.get().pop_back()

    def reserve(self, size_t new_cap) -> None:
        """Reserve storage for at least `new_cap` levels."""
        self.thisptr.get().reserve(new_cap)

    def resize(self, size_t count) -> None:
        """Resize the number of levels, leaving any newly added slots empty."""
        self.thisptr.get().resize(count)

    def erase(self, size_t index, stop: int | None = None) -> None:
        """Remove the alphabet at `index`, or the range [`index`, `stop`) when `stop` is given."""
        cdef size_t size = self.thisptr.get().size()
        cdef size_t c_stop = size if stop is None else <size_t>stop
        if index >= size:
            raise IndexError(f"erase index {index} out of range for AlphabetLevels of size {size}")
        if c_stop < index or c_stop > size:
            raise IndexError(f"erase stop {c_stop} out of range for AlphabetLevels of size {size} (index={index})")
        cdef vector[shared_ptr[CAlphabet]].iterator first = self.thisptr.get().begin() + index
        cdef vector[shared_ptr[CAlphabet]].iterator last
        if stop is None:
            self.thisptr.get().erase(first)
        else:
            last = self.thisptr.get().begin() + c_stop
            self.thisptr.get().erase(first, last)

    cdef optional[Level] _c_level(self, level):
        if level is None:
            return optional[Level]()
        return optional[Level](<Level>level)

    def translate_symbol(self, str symbol, level: int | None = None):
        """Translate `symbol` using the alphabet for `level` (see `for_level` for level resolution)."""
        return self.thisptr.get().translate_symb(symbol.encode('utf-8'), self._c_level(level))

    def reverse_translate_symbol(self, Symbol symbol, level: int | None = None) -> str:
        """Translate `symbol` back to its name using the alphabet for `level`."""
        return self.thisptr.get().reverse_translate_symbol(symbol, self._c_level(level)).decode('utf-8')

    def get_alphabet_symbols(self, level: int | None = None):
        """Get the symbols known to the alphabet on `level`."""
        cdef COrdVector[Symbol] symbols = self.thisptr.get().get_alphabet_symbols(self._c_level(level))
        return {s for s in symbols}

    def get_complement(self, symbols, level: int | None = None):
        """Get the complement of `symbols` with respect to the alphabet for `level`."""
        cdef vector[Symbol] c_symbols_vec = sorted(symbols)
        cdef COrdVector[Symbol] c_symbols = COrdVector[Symbol](c_symbols_vec)
        cdef COrdVector[Symbol] result = self.thisptr.get().get_complement(c_symbols, self._c_level(level))
        return {s for s in result}

    def is_empty(self, level: int | None = None) -> bool:
        """Check whether the alphabet for `level` is empty (see `AlphabetLevels::empty` for level resolution)."""
        return self.thisptr.get().empty(self._c_level(level))

    def clear(self, level: int | None = None) -> None:
        """Clear the alphabet for `level`."""
        self.thisptr.get().clear(self._c_level(level))
