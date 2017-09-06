// TODO: add header
// The VATA Virtual Machine

#ifndef _VATA2_VM_HH_
#define _VATA2_VM_HH_

#include <vata2/parser.hh>

namespace Vata2
{
namespace VM
{

/// Data type representing the type of a value
using VMType = std::string;
/// Data type representing a pointer to a memory holding a value
using VMPointer = const void*;

/**
 * Data type representing a value, which is composed of a type and a pointer to
 * a general memory
*/
using VMValue = std::pair<VMType, VMPointer>;

/// A dictionary mapping names to values
using VMStorage = std::unordered_map<std::string, VMValue>;

/// The virtual machine executing VATA code
class VirtualMachine
{
private:

	/// The memory assigning values to names
	VMStorage mem;

public:

	void run(const Vata2::Parser::Parsed& parsed);
	void run(const Vata2::Parser::ParsedSection& parsec);

};

// CLOSING NAMESPACES AND GUARDS
} /* VM */
} /* Vata2 */

#endif /* _VATA2_VM_HH_ */
