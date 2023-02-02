/* alphabet.hh -- File containing alphabets for automata
 *
 * This file is a part of libmata.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MATA_ALPHABET_HH
#define _MATA_ALPHABET_HH

#include <forward_list>
#include <string>

#include <mata/util.hh>

namespace Mata {
        using Symbol = unsigned long;
        using StringToSymbolMap = std::unordered_map<std::string, Symbol>;

         /**
          * The abstract interface for NFA alphabets.
          */
        class Alphabet {
        public:
            /// translates a string into a symbol
            virtual Symbol translate_symb(const std::string &symb) = 0;

            /**
             * @brief Translate internal @p symbol representation back to its original string name.
             *
             * Throws an exception when the @p symbol is missing in the alphabet.
             * @param[in] symbol Symbol to translate.
             * @return @p symbol original name.
             */
            virtual std::string reverse_translate_symbol(Symbol symbol) const = 0;

            /// also translates strings to symbols
            Symbol operator[](const std::string &symb) { return this->translate_symb(symb); }

            /**
             * @brief Get a set of all symbols in the alphabet.
             *
             * The result does not have to equal the list of symbols in the automaton using this alphabet.
             */
            virtual Util::OrdVector<Symbol> get_alphabet_symbols() const { // {{{
                throw std::runtime_error("Unimplemented");
            } // }}}

            /// complement of a set of symbols wrt the alphabet
            virtual std::list<Symbol> get_complement(const std::set<Symbol> &syms) const { // {{{
                (void) syms;
                throw std::runtime_error("Unimplemented");
            } // }}}

            virtual ~Alphabet() = default;

            /**
             * @brief Check whether two alphabets are equal.
             *
             * In general, two alphabets are equal if and only if they are of the same class instance.
             * @param other_alphabet The other alphabet to compare with for equality.
             * @return True if equal, false otherwise.
             */
            virtual bool is_equal(const Alphabet &other_alphabet) const {
                return address() == other_alphabet.address();
            }

            /**
             * @brief Check whether two alphabets are equal.
             *
             * In general, two alphabets are equal if and only if they are of the same class instance.
             * @param other_alphabet The other alphabet to compare with for equality.
             * @return True if equal, false otherwise.
             */
            virtual bool is_equal(const Alphabet *const other_alphabet) const {
                return address() == other_alphabet->address();
            }

            bool operator==(const Alphabet &) const = delete;

        protected:
            virtual const void *address() const { return this; }
        }; // class Alphabet.

        /**
        * Direct alphabet (also identity alphabet or integer alphabet) using integers as symbols.
        *
        * This alphabet presumes that all integers are valid symbols.
        * Therefore, calling member functions get_complement() and get_alphabet_symbols() makes no sense in this context and the methods
        *  will throw exceptions warning about the inappropriate use of IntAlphabet. If one needs these functions, they should
        *  use OnTheFlyAlphabet instead of IntAlphabet.
        */
        class IntAlphabet : public Alphabet {
        public:
            IntAlphabet() : alphabet_instance(IntAlphabetSingleton::get()) {}

            Symbol translate_symb(const std::string &symb) override {
                Symbol symbol;
                std::istringstream stream(symb);
                stream >> symbol;
                return symbol;
            }

            std::string reverse_translate_symbol(Symbol symbol) const override {
                return std::to_string(symbol);
            }

            Util::OrdVector<Symbol> get_alphabet_symbols() const override {
                throw std::runtime_error("Nonsensical use of get_alphabet_symbols() on IntAlphabet.");
            }

            std::list<Symbol> get_complement(const std::set<Symbol> &syms) const override {
                (void) syms;
                throw std::runtime_error("Nonsensical use of get_alphabet_symbols() on IntAlphabet.");
            }

            IntAlphabet(const IntAlphabet &) = default;

            IntAlphabet &operator=(const IntAlphabet &int_alphabet) = delete;

        protected:
            const void *address() const override { return &alphabet_instance; }

        private:
            /**
             * Singleton class implementing integer alphabet_instance for class IntAlphabet.
             *
             * Users have to use IntAlphabet instead which provides interface identical to other alphabets and can be used in
             *  places where an instance of the abstract class Alphabet is required.
             */
            class IntAlphabetSingleton {
            public:
                static IntAlphabetSingleton &get() {
                    static IntAlphabetSingleton alphabet;
                    return alphabet;
                }

                IntAlphabetSingleton(IntAlphabetSingleton &) = delete;

                IntAlphabetSingleton(IntAlphabetSingleton &&) = delete;

                IntAlphabetSingleton &operator=(const IntAlphabetSingleton &) = delete;

                IntAlphabetSingleton &operator=(IntAlphabetSingleton &&) = delete;

                ~IntAlphabetSingleton() = default;

            protected:
                IntAlphabetSingleton() = default;
            }; // class IntAlphabetSingleton.

            IntAlphabetSingleton &alphabet_instance;
        }; // class IntAlphabet.

/**
 * An alphabet constructed 'on the fly'.
 * Should be use anytime the automata have a specific names for the symbols.
 */
    class OnTheFlyAlphabet : public Alphabet {
    public:
        using InsertionResult = std::pair<StringToSymbolMap::const_iterator, bool>; ///< Result of the insertion of a new symbol.

        explicit OnTheFlyAlphabet(Symbol init_symbol = 0) : next_symbol_value(init_symbol) {};
        OnTheFlyAlphabet(const OnTheFlyAlphabet& rhs) : symbol_map(rhs.symbol_map), next_symbol_value(rhs.next_symbol_value) {}

        explicit OnTheFlyAlphabet(const StringToSymbolMap& str_sym_map)
                : symbol_map(str_sym_map) {}

        /**
         * Create alphabet from a list of symbol names.
         * @param symbol_names Names for symbols on transitions.
         * @param init_symbol Start of a sequence of values to use for new symbols.
         */
        explicit OnTheFlyAlphabet(const std::vector<std::string>& symbol_names, Symbol init_symbol = 0)
                : symbol_map(), next_symbol_value(init_symbol) { add_symbols_from(symbol_names); }

        Util::OrdVector<Symbol> get_alphabet_symbols() const override;
        std::list<Symbol> get_complement(const std::set<Symbol>& syms) const override;

        std::string reverse_translate_symbol(const Symbol symbol) const override {
            for (const auto& symbol_mapping: symbol_map) {
                if (symbol_mapping.second == symbol) {
                    return symbol_mapping.first;
                }
            }
            throw std::runtime_error("symbol '" + std::to_string(symbol) + "' is out of range of enumeration");
        }

    private:
        OnTheFlyAlphabet& operator=(const OnTheFlyAlphabet& rhs);

    public:
        /**
         * @brief Expand alphabet by symbols from the passed @p symbol_names.
         *
         * Adding a symbol name which already exists will throw an exception.
         * @param[in] symbol_names Vector of symbol names.
         */
        void add_symbols_from(const std::vector<std::string>& symbol_names) {
            for (const std::string& symbol_name: symbol_names) {
                add_new_symbol(symbol_name);
            }
        }

        /**
         * @brief Expand alphabet by symbols from the passed @p symbol_map.
         *
         * The value of the already existing symbols will NOT be overwritten.
         * @param[in] new_symbol_map Map of strings to symbols.
         */
        void add_symbols_from(const StringToSymbolMap& new_symbol_map);

        template <class InputIt>
        OnTheFlyAlphabet(InputIt first, InputIt last) : OnTheFlyAlphabet() {
            for (; first != last; ++first) {
                add_new_symbol(*first, next_symbol_value);
            }
        }

        OnTheFlyAlphabet(std::initializer_list<std::string> l) : OnTheFlyAlphabet(l.begin(), l.end()) {}

        Symbol translate_symb(const std::string& str) override
        {
            const auto it_insert_pair = symbol_map.insert({str, next_symbol_value});
            if (it_insert_pair.second) {
                return next_symbol_value++;
            } else {
                return it_insert_pair.first->second;
            }

            // TODO: How can the user specify to throw exceptions when we encounter an unknown symbol? How to specify that
            //  the alphabet should have only the previously fixed symbols?
            //auto it = symbol_map.find(str);
            //if (symbol_map.end() == it)
            //{
            //    throw std::runtime_error("unknown symbol \'" + str + "\'");
            //}

            //return it->second;
        }

        /**
         * @brief Add new symbol to the alphabet with the value of @c next_symbol_value.
         *
         * Throws an exception when the adding fails.
         *
         * @param[in] key User-space representation of the symbol.
         * @return Result of the insertion as @c InsertionResult.
         */
        InsertionResult add_new_symbol(const std::string& key) {
            InsertionResult insertion_result{ try_add_new_symbol(key, next_symbol_value) };
            if (!insertion_result.second) { // If the insertion of key-value pair failed.
                throw std::runtime_error("multiple occurrences of the same symbol");
            }
            ++next_symbol_value;
            return insertion_result;
        }

        /**
         * @brief Add new symbol to the alphabet.
         *
         * Throws an exception when the adding fails.
         *
         * @param[in] key User-space representation of the symbol.
         * @param[in] value Number of the symbol to be used on transitions.
         * @return Result of the insertion as @c InsertionResult.
         */
        InsertionResult add_new_symbol(const std::string& key, Symbol value) {
            InsertionResult insertion_result{ try_add_new_symbol(key, value) };
            if (!insertion_result.second) { // If the insertion of key-value pair failed.
                throw std::runtime_error("multiple occurrences of the same symbol");
            }
            update_next_symbol_value(value);
            return insertion_result;
        }

        /**
         * @brief Try to add symbol to the alphabet map.
         *
         * Does not throw an exception when the adding fails.
         *
         * @param[in] key User-space representation of the symbol.
         * @param[in] value Number of the symbol to be used on transitions.
         * @return Result of the insertion as @c InsertionResult.
         */
        InsertionResult try_add_new_symbol(const std::string& key, Symbol value) {
            return symbol_map.insert({ key, value});
        }

        /**
         * Get the next value for a potential new symbol.
         * @return Next Symbol value.
         */
        Symbol get_next_value() const { return next_symbol_value; }

        /**
         * Get the number of existing symbols, epsilon symbols excluded.
         * @return The number of symbols.
         */
        size_t get_number_of_symbols() const { return next_symbol_value; }

        /**
         * Get the symbol map used in the alphabet.
         * @return Map mapping strings to symbols used internally in Mata.
         */
        const StringToSymbolMap& get_symbol_map() const { return symbol_map; }

    private:
        StringToSymbolMap symbol_map{}; ///< Map of string transition symbols to symbol values.
        Symbol next_symbol_value{}; ///< Next value to be used for a newly added symbol.

    public:
        /**
         * @brief Update next symbol value when appropriate.
         *
         * When the newly inserted value is larger or equal to the current next symbol value, update the next symbol
         *  value to a value one larger than the new value.
         * @param value The value of the newly added symbol.
         */
        void update_next_symbol_value(Symbol value) {
            if (next_symbol_value <= value) {
                next_symbol_value = value + 1;
            }
        }
    }; // class OnTheFlyAlphabet.
} // namespace Mata

namespace std
{ // {{{
    std::ostream& operator<<(std::ostream& os, const Mata::Alphabet& alphabet);
}
#endif //_MATA_ALPHABET_HH
