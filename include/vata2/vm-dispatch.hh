/* vm-dispatch.hh -- dispatcher for virtual machine function calls
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

// The purpose of this code is to provide an interface through which classes
// for automata types could register their 'dispatcher' functions for
// operations called from within the virtual machine.  The code is deliberately
// made separate from vata2/vm.hh in order to decrease the number of
// dependencies.

#ifndef _VM_DISPATCH_HH_
#define _VM_DISPATCH_HH_

// VATA headers
#include <vata2/vm.hh>

namespace Vata2
{
namespace VM
{

/// Data type for function names
using VMFuncName = std::string;
/// Data type for function arguments
using VMFuncArgs = std::vector<VMValue>;
/// Data type for dispatcher function pointer
using VMDispatcherFunc = std::function<VMValue(const VMFuncName&, const VMFuncArgs&)>;


/// registers a dispatcher function for a VATA data type
void reg_dispatcher(
	const std::string&       type_name,
	const VMDispatcherFunc&  func);


/// finds the dispatcher function for a given type
const VMDispatcherFunc& find_dispatcher(const std::string& type_name);


/// calls a dispatcher function for the given value with it as the only argument
inline VMValue call_dispatch_with_self(
	const VMValue&    val,
	const VMFuncName& func_name)
{ return find_dispatcher(val.type)(func_name, {val}); }


/// Default dispatcher function
VMValue default_dispatch(
	const VMFuncName&  func_name,
	const VMFuncArgs&  func_args);


// CLOSING NAMESPACES AND GUARDS
} /* VM */
} /* Vata2 */

#endif /* _VM_DISPATCH_HH_ */
