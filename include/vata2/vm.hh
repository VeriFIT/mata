/* vm.hh -- the VATA virtual machine
 *
 * Copyright (c) 2018 Ondrej Lengal <ondra.lengal@gmail.com>
 *
 * This file is a part of libvata2.
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

#ifndef _VATA2_VM_HH_
#define _VATA2_VM_HH_

#include <stack>

// VATA headers
#include <vata2/parser.hh>

namespace Vata2
{

/// the type of tokens
extern const std::string TYPE_TOKEN;
/// the type name of the boolean data type
extern const std::string TYPE_BOOL;
/// the type name of the string data type
extern const std::string TYPE_STR;
/// the type name of the void data type
extern const std::string TYPE_VOID;
/// the type name of Not a Value
extern const std::string TYPE_NOT_A_VALUE;
/// the type name of parsed section
extern const std::string TYPE_PARSEC;

namespace VM
{

/// Data type representing a pointer to a memory holding a value
using VMPointer = const void*;

/**
 * Data type representing a value, which is composed of a type and a pointer to
 * a general memory
*/
class VMValue
{ // {{{
public:
	/**
	 * Name of the type
	 *
	 * Built-in types:
	 *   NaV      : Not a Value
	 *   str      : a string of characters
	 *   void     : a void type (e.g. a return type of a procedure)
	 *   Parsec   : parsed section
	 */
	std::string type;

private:

	/// pointer to the object
	VMPointer ptr;

public:
	/// default constructor
	VMValue() : type(), ptr() { }
	/// standard constructor
	VMValue(const std::string& type, VMPointer ptr) : type(type), ptr(ptr) { }
	/// copy constructor
	VMValue(const VMValue& rhs) : type(rhs.type), ptr(rhs.ptr) { }
	/// assignment operator
	VMValue& operator=(const VMValue& rhs)
	{ // {{{
		if (this != &rhs) {
			this->type = rhs.type;
			this->ptr = rhs.ptr;
		}

		// FIXME: expecting memory issues here
		return *this;
	} // operator=() }}}

	/// returns the included pointer
	VMPointer get_ptr() const { return this->ptr; }

	/// conversion to string
	friend std::ostream& operator<<(std::ostream& os, const VMValue& val)
	{ // {{{
		os << "<" << val.type << ": ";
		if (TYPE_STR == val.type || TYPE_TOKEN == val.type) {
			// FIXME: dispatch this call to val.type dispatcher
			os << *static_cast<const std::string*>(val.get_ptr());
		} else {
			os << val.get_ptr();
		}

		os << ">";
		return os;
	} // operator<<(std::ostream) }}}
}; // VMValue }}}

/**
 * @brief  The virtual machine executing VATA code
 *
 * The virtual machine executing VATA code.  It consists of a storage, which
 * contains named objects, and an execution stack.  The virtual machine is a
 * stack machine that interprets input code written in the VATA@CODE syntax.
 *
 * FIXME: is the following correct?
 * A program in VATA@CODE is a sequence of statements, separated by ends of
 * lines.  Programs are straight-line, i.e., no control flow or loops statements
 * are present (at least for now).  Statements are either **variable
 * assignments** or **procedure calls**.  **Variable assignments** are of the
 * form `lhs = expr`, e.g.,
 *
 * @code{.vata}
 *   aut = (load_file "nfa1.vtf")
 * @endcode
 *
 * where `aut` is a variable name and `(load_file "nfa1.vtf")` is the expression
 * the value of which is to be assigned to `aut`.  **Procedure calls** are of
 * the form `expr`, e.g.,
 *
 * @code{.vata}
 *   (print (string "Hello World"))
 * @endcode
 *
 * where `print` is a function with a `void` return type (i.e., a procedure).
 *
 * ## Expressions
 * An expression is a either a *token* or a *function application*. **Token** is
 * the same as defined in [.vtf syntax](README.md) and **function application**
 * is of the form
 *
 * @code{.vata}
 *   (func-name arg1 arg2 ... argN)
 * @endcode
 *
 * where `func-name` is the function name and `arg1`, `arg2`, ..., `argN` is a
 * list of positional arguments, which are also expressions.
 *
 * We do not force functions to have a fixed number of arguments.
 *
 * ## Types
 * Every expression has a type, which are:
 *  ** **basic**: e.g. `void`, `bool`, or `string`
 *  ** **complex**: e.g. `NFA`, `NTA`, or `STATE-REL`
 *
 * ### A "Hello World" Example
 * @code{.vata}
 *   @CODE
 *   (print (string "Hello World"))
 * @endcode
 *
 * ## Function selection
 * VATA Virtual Machine has a polymorphis extensible function mechanism.  This
 * means that with an exception of a few in-built functions, any data type can
 * define its own function handlers.  The resolution of a function to call is
 * based on the type of the first argument of the function --- the function call
 * will be passed to its dispatcher function.
 */
class VirtualMachine
{
private:

	/// A dictionary mapping names to values
	using VMStorage = std::unordered_map<std::string, VMValue>;
	/// A stack for VMValues
	using VMStack = std::stack<VMValue>;

	/// The memory assigning values to names
	VMStorage mem;
	VMStack exec_stack;

	/// Pushes a new value on top of the execution stack
	void push_to_stack(VMValue val);

public:

	/// default constructor
	VirtualMachine() : mem(), exec_stack() { }

	void run(const Vata2::Parser::Parsed& parsed);
	void run(const Vata2::Parser::ParsedSection& parsec);
	void run_code(const Vata2::Parser::ParsedSection& parsec);

	/// Executes one line of code
	void execute_line(const Parser::BodyLine& line);
	void process_token(const std::string& tok);
	void exec_cmd(const std::vector<VMValue>& exec_vec);

	/**
	 * @brief  Storage get accessor
	 *
	 * @param[in]  name  Name of the object in the storage
	 *
	 * @returns  VMValue with the object stored at position @p name
	 *
	 * @throws  VMException  If there is nothing at position @p name, an exception
	 *                       is thrown
	 */
	VMValue load_from_storage(const std::string& name) const;

	/**
	 * @brief  Storage set accessor
	 *
	 * @param[in]  name  Name of the object in the storage
	 * @param[in]  val   The value to store in the storage
	 */
	void save_to_storage(const std::string& name, VMValue val);

	/// Cleans the stack
	void clean_stack();
};

/// The exception for virtual machine errors
class VMException : public std::runtime_error
{
public:
	// use base class constructors
	using std::runtime_error::runtime_error;
};

// CLOSING NAMESPACES AND GUARDS
} /* VM */
} /* Vata2 */

#endif /* _VATA2_VM_HH_ */
