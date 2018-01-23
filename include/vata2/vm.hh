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
	/// name of the type
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
		if ("string" == val.type) {
			// FIXME: dispatch this call to val.type dispatcher
			os << *static_cast<const std::string*>(val.get_ptr());
		} else {
			os << val.get_ptr();
		}

		os << ">";
		return os;
	} // operator<<(std::ostream) }}}
}; // VMValue }}}

/// A dictionary mapping names to values
using VMStorage = std::unordered_map<std::string, VMValue>;

/// The virtual machine executing VATA code
class VirtualMachine
{
private:

	/// The memory assigning values to names
	VMStorage mem;
	std::stack<VMValue> exec_stack;

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
