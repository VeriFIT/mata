// TODO: add header
//
// The purpose of this code is to provide an interface through which classes
// for automata types could register their 'dispatcher' functions for
// operations called from the virtual machine.  The code is deliberately made
// separate from vata2/vm.hh in order to decrease the number of dependencies.

#ifndef _VM_DISPATCH_HH_
#define _VM_DISPATCH_HH_

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


// CLOSING NAMESPACES AND GUARDS
} /* VM */
} /* Vata2 */

#endif /* _VM_DISPATCH_HH_ */
