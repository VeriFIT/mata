/* vm.cc -- implementation of the VATA virtual machine
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

#include <vata2/vm.hh>
#include <vata2/vm-dispatch.hh>

#include <fstream>


/// definitions
const std::string Vata2::TYPE_TOKEN = "token";
const std::string Vata2::TYPE_BOOL = "bool";
const std::string Vata2::TYPE_STR = "str";
const std::string Vata2::TYPE_VOID = "void";
const std::string Vata2::TYPE_NOT_A_VALUE = "nav";
const std::string Vata2::TYPE_PARSEC = "parsec";

unsigned Vata2::LOG_VERBOSITY = 0;

void Vata2::VM::VirtualMachine::run(const Vata2::Parser::Parsed& parsed)
{ // {{{
	for (const auto& parsec : parsed) {
		this->run(parsec);
	}
} // run(Parsed) }}}


/**
 * @brief  Executes a parsed section
 *
 * This method is the workhorse of VATA.  It interprets an element of
 * ParsedSection and accordingly modifies the state of the virtual machine.
 *
 * If @p parsec is not of the type CODE, an object of the corresponding type is
 * constructed and saved into the virtual machine.
 *
 * If @p parsec is of the type CODE, the program inside is executed, taking into
 * account the state of the virtual machine, and modifying it.
 *
 * @param[in]  parsec  The parsed section to execute
 */
void Vata2::VM::VirtualMachine::run(const Vata2::Parser::ParsedSection& parsec)
{ // {{{
	if (parsec.type == "CODE") {
		this->run_code(parsec);
		return;
	}

	DEBUG_VM_LOW_PRINT_LN(parsec.type << "\n");

	VMDispatcherFunc dispatch = Vata2::VM::find_dispatcher(parsec.type);
	std::string name;
	if (!util::haskey(parsec.dict, "Name")) {
		WARN_PRINT("constructing an entity without a name; "
			"the result will be discarded");
	}
	else {
		const auto& vec_of_strs = parsec.dict.at("Name");
		if (vec_of_strs.size() == 0) {
			throw std::runtime_error("the \"name\" attribute provided without value");
		}
		if (vec_of_strs.size() > 1) {
			throw std::runtime_error(
				"the \"name\" attribute provided with multiple values");
		}
		else {
			name = vec_of_strs[0];
		}
	}

	// TODO: is the dispatch OK? shouldn't weget the type of the parsec first?

	VMValue arg(Vata2::TYPE_PARSEC, new Parser::ParsedSection(parsec));
	VMFuncArgs args = {arg};
	VMValue val = dispatch("construct", args);
	if (!name.empty()) {
		this->save_to_storage(name, val);
		this->mem[name] = val;
	}
} // run(ParsedSection) }}}


void Vata2::VM::VirtualMachine::run_code(
	const Vata2::Parser::ParsedSection& parsec)
{ // {{{
	DEBUG_VM_LOW_PRINT("VATA-CODE START");
	for (const auto& line : parsec.body) {
		assert(this->exec_stack.empty());

		DEBUG_VM_LOW_PRINT(std::to_string(line));
		this->execute_line(line);

		if (1 == this->exec_stack.size()) {
			// dead return value
			assert(false);
			// TODO: check it is not void
			const VMValue& last_val = this->exec_stack.top();
			if (TYPE_VOID != last_val.type) {
				WARN_PRINT("throwing away an unused value at the stack: " +
					std::to_string(this->exec_stack.top()));
			}
			this->clean_stack();
		}
		else if (3 == this->exec_stack.size()) {
			// assignment

			// the returned value
			VMValue val = this->exec_stack.top();
			this->exec_stack.pop();

			// the assignment operator '='
			VMValue asgn = this->exec_stack.top();
			if (TYPE_TOKEN != asgn.type || "=" != *static_cast<const std::string*>(asgn.get_ptr())) {
				call_dispatch_with_self(val, "delete");
				throw VMException("dangling code or invalid token: " + std::to_string(asgn) +
					" (expecting '=')");
			}
			this->exec_stack.pop();
			call_dispatch_with_self(asgn, "delete");

			// the target variable
			VMValue var_name = this->exec_stack.top();
			if (TYPE_TOKEN != var_name.type) {
				call_dispatch_with_self(val, "delete");
				throw VMException("dangling code or invalid token: " + std::to_string(asgn) +
					" (expecting '=')");
			}
			this->exec_stack.pop();
			assert(this->exec_stack.empty());
			std::string var_name_str = *static_cast<const std::string*>(var_name.get_ptr());
			call_dispatch_with_self(var_name, "delete");

			// TODO: check var_name_str is a good variable name
			this->save_to_storage(var_name_str, val);
		}
		else {
			assert(this->exec_stack.size() > 1);
			std::string dangling = std::to_string(this->exec_stack);
			this->clean_stack();
			throw VMException("dangling code in a CODE section: " + dangling);
		}

	}

	DEBUG_VM_LOW_PRINT("VATA-CODE END");
	DEBUG_VM_LOW_PRINT("Stack: " + std::to_string(this->exec_stack));
} // run_code(ParsedSection) }}}


void Vata2::VM::VirtualMachine::execute_line(
	const Vata2::Parser::BodyLine& line)
{ // {{{
	for (const std::string& tok : line) {
		this->process_token(tok);
	}
} // execute_line(BodyLine) }}}


void Vata2::VM::VirtualMachine::process_token(
	const std::string& tok)
{ // {{{
	if (")" != tok) { // nothing special
		DEBUG_VM_LOW_PRINT("allocating memory for token " + tok);
		if (('\"' == tok[0]) && ('\"' == tok[tok.length()-1])) { // for strings
			std::string* str = new std::string(tok, 1, tok.length()-2);
			VMValue val(TYPE_STR, str);
			this->push_to_stack(val);
		} else { // for tokens
			std::string* str = new std::string(tok);
			VMValue val(TYPE_TOKEN, str);
			this->push_to_stack(val);
		}
	} else { // closing parenthesis - execute action
		assert(")" == tok);

		std::vector<VMValue> exec_vec;
		bool closed = false;
		while (!this->exec_stack.empty()) {
			const VMValue& st_top = this->exec_stack.top();
			DEBUG_VM_LOW_PRINT("top of stack type: " + st_top.type);
			if (TYPE_TOKEN == st_top.type) {
				assert(nullptr != st_top.get_ptr());
				const std::string& val = *static_cast<const std::string*>(st_top.get_ptr());
				DEBUG_VM_LOW_PRINT("top of stack token value: " + val);
				if ("(" == val) {
					closed = true;
					call_dispatch_with_self(st_top, "delete");
					this->exec_stack.pop();   // NEEDS TO BE AFTER THE CALL!!
					break;
				}
				else { /* do nothing */ }
			}
			else { /* do nothing */ }

			exec_vec.insert(exec_vec.begin(), st_top);
			this->exec_stack.pop();
		}

		auto clean_exec_vec = [&exec_vec]()
			{
				// deallocate elements in exec_vec
				for (const auto& val : exec_vec) {
					DEBUG_VM_LOW_PRINT("exec_vec: deleting " + std::to_string(val));
					call_dispatch_with_self(val, "delete");
				}
			};

		// below here, we should have a try block, catch exceptions, and deallocate
		// the memory taken by the content of the execution stack
		try {
			if (!closed) {
				throw VMException("mismatched parenthesis");
			}

			this->exec_cmd(exec_vec);
		}
		catch (const VMException& ex) {
			DEBUG_VM_HIGH_PRINT("VM: caught the following VMException: " +
				std::to_string(ex.what()));
			// clean exec_vec and the whole stack
			DEBUG_VM_LOW_PRINT("VM: cleaning the execution vector and execution stack");
			clean_exec_vec();
			DEBUG_VM_LOW_PRINT("VM: execution vector clean");
			this->clean_stack();
			DEBUG_VM_LOW_PRINT("VM: execution stack clean");
			throw;
		}
		catch (...) {
			assert(false);
		}

		clean_exec_vec();
	}
} // process_token(string) }}}


void Vata2::VM::VirtualMachine::exec_cmd(
	const std::vector<VMValue>& exec_vec)
{ // {{{
	DEBUG_VM_HIGH_PRINT("Executing " + std::to_string(exec_vec));

	if (exec_vec.empty()) {
		throw VMException("\"()\" is not a valid function call");
	}

	// getting the function name
	const VMValue& fnc_val = exec_vec[0];
	if (TYPE_TOKEN != fnc_val.type) {
		std::string err_string = "(" + std::to_string(fnc_val);
		for (size_t i = 1; i < exec_vec.size(); ++i) {
			err_string += ", " + std::to_string(exec_vec[i]);
		}
		err_string += ")";

		throw VMException("\"" + err_string + "\" is not a valid function call");
	}

	// assert(TYPE_TOKEN == fnc_val.type);
	assert(nullptr != fnc_val.get_ptr());
	const std::string& fnc_name = *static_cast<const std::string*>(fnc_val.get_ptr());

	if (exec_vec.size() <= 1) {
		throw VMException("\"(" + fnc_name + ")\" is not a valid function call");
	}

	// constructing the arguments
	std::vector<VMValue> args;
	for (auto it = exec_vec.begin() + 1; it != exec_vec.end(); ++it) {
		if (TYPE_TOKEN == it->type) {
			const std::string& var_name = *static_cast<const std::string*>(it->get_ptr());
			VMValue val = load_from_storage(var_name);
			args.push_back(val);
		} else {
			args.push_back(*it);
		}
	}

	// getting the object type (type of the first argument of the function) and
	// removing it from the arguments
	VMValue arg1_val = args[0];
	const std::string& arg1_type = arg1_val.type;

	VMValue ret_val = find_dispatcher(arg1_type)(fnc_name, args);
	if (Vata2::TYPE_NOT_A_VALUE == ret_val.type) {
		ret_val = default_dispatch(fnc_name, args);
		if (Vata2::TYPE_NOT_A_VALUE == ret_val.type) {
			throw VMException(fnc_name + " is not a defined function");
		}
	}

	this->push_to_stack(ret_val);
} // exec_cmd(std::vector) }}}


Vata2::VM::VMValue Vata2::VM::default_dispatch(
	const VMFuncName&  func_name,
	const VMFuncArgs&  func_args)
{ // {{{
	DEBUG_VM_HIGH_PRINT("calling function \"" + func_name + "\" for default dispatcher");

	if ("return" == func_name) {
		if (func_args.size() != 1) {
			throw VMException("\"return\" requires 1 argument (" +
				std::to_string(func_args.size()) + " provided)");
		}

		VMValue ret_val = call_dispatch_with_self(func_args[0], "copy");
		if (Vata2::TYPE_NOT_A_VALUE == ret_val.type) {
			throw VMException("The type \"" + func_args[0].type +
				"\" does not implement the \"copy\" operation");
		}
		return ret_val;
	}

	if ("load_file" == func_name) {
		if (func_args.size() != 1) {
			throw VMException("\"load_file\" requires 1 argument (" +
				std::to_string(func_args.size()) + " provided)");
		}

		if (func_args[0].type != TYPE_STR) {
			throw VMException("\"load_file\" requires 1 argument of the type \"" +
				std::string(TYPE_STR) + "\"; an argument of the type \"" + func_args[0].type +
				"\" provided instead");
		}

		const std::string& filename = *(static_cast<const std::string*>(func_args[0].get_ptr()));
		DEBUG_VM_HIGH_PRINT("loading file " + filename);
		std::fstream fs(filename, std::ios::in);
		if (!fs) {
			throw VMException("could not open file \"" + filename + "\"");
		}

		// TODO: handle keepQuotes?
		Parser::Parsed prs = Parser::parse_vtf(fs);
		if (prs.size() != 1) {
			throw VMException("load_file loaded a file with " + std::to_string(prs.size()) +
				" sections; only 1 section per loaded file is supported in load_file calls");
		}

		// Parser::ParsedSection* sec = new Parser::ParsedSection(std::move(prs[0]));
		// TODO: handle more sections
		Parser::ParsedSection* sec = new Parser::ParsedSection(std::move(prs[0]));
		assert(prs.size() == 1);
		DEBUG_VM_HIGH_PRINT("loaded a section of the type \"" + sec->type +
			"\" from file " + filename);
		VMValue sec_val(TYPE_PARSEC, sec);
		VMFuncArgs args = {sec_val};
		VMDispatcherFunc dispatch = Vata2::VM::find_dispatcher(sec->type);
		VMValue res = dispatch("construct", args);

		return res;
	}

	return VMValue(Vata2::TYPE_NOT_A_VALUE, nullptr);
} // default_dispatch() }}}


void Vata2::VM::VirtualMachine::push_to_stack(VMValue val)
{ // {{{
	DEBUG_VM_LOW_PRINT("VM: pushing " + std::to_string(val) + " on the stack");
	this->exec_stack.push(val);
} // push_to_stack() }}}

void Vata2::VM::VirtualMachine::save_to_storage(
	const std::string&  name,
	VMValue             val)
{
	DEBUG_VM_HIGH_PRINT("storing a new value of " + name + ": " + std::to_string(val));
	auto iter = this->mem.find(name);
	if (this->mem.end() != iter) { // name already used
		WARN_PRINT("rewriting stored value of " + name + ": " + std::to_string(val));
	}
	this->mem[name] = val;
} // save_to_storage()

Vata2::VM::VMValue Vata2::VM::VirtualMachine::load_from_storage(
	const std::string& name) const
{ // {{{
	DEBUG_VM_LOW_PRINT("retrieving object \"" + name + "\" from the memory");
	auto it = this->mem.find(name);
	if (this->mem.end() == it){
		throw VMException("Trying to access object \'" + name + "\', which is not in the memory");
	}

	return it->second;
} // load_from_storage() }}}


void Vata2::VM::VirtualMachine::clean_stack()
{ // {{{
	while (!this->exec_stack.empty()) {
		const auto& val = this->exec_stack.top();
		DEBUG_VM_LOW_PRINT("VM: cleaning " + std::to_string(val) + " from the stack");
		call_dispatch_with_self(val, "delete");
		this->exec_stack.pop();   // NEEDS TO BE AFTER THE CALL!!
	}
} // clean_stack() }}}

