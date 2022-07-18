// TODO: add header

#include <mata/vm-dispatch.hh>

// Headers of user data types
#include <mata/nfa.hh>
#include "bool.hh"
#include "str.hh"
#include "void.hh"


// definitions
namespace
{
using std::tie;

using Mata::VM::VMDispatcherFunc;

/// the structure that contains information about a type
struct VMDispatcherStruct
{
	/// the dispatcher function
	VMDispatcherFunc func;

	/// information about a type
	std::string info;
};


/// A dictionary mapping types to dispatcher function pointers
using VMDispatcherDict = std::unordered_map<std::string, VMDispatcherStruct>;

/// init function type
using VMInitFunc = std::function<void()>;

/// the dispatch function dictionary
VMDispatcherDict dispatch_dict = { };


/// list of init functions --- ADD HERE FUNCTIONS TO BE RUN AT STARTUP
const VMInitFunc INIT_FUNCTIONS[] =
{
        Mata::Bool::init,
        Mata::Nfa::init,
        Mata::Str::init,
        Mata::Parser::init,
        Mata::Void::init,
};


bool init_dispatch_dict()
{
	// the code is run at startup so let's try to catch errors here
	try
	{
		for (const auto& init_func : INIT_FUNCTIONS)
		{
			init_func();
		}
	}
	catch (const std::exception& ex) // LCOV_EXCL_START
	{
		std::cerr << "An error occurred at startup: " << ex.what() << "\n";
		exit(EXIT_FAILURE);
	} // LCOV_EXCL_STOP

	DEBUG_PRINT("dispatcher dictionary initialized");

	return true;
}

bool is_dispatch_dict_init = init_dispatch_dict();
} // anonymous


void Mata::VM::reg_dispatcher(
	const std::string&       type_name,
	const VMDispatcherFunc&  func,
	const std::string&       info)
{ // {{{
	bool inserted;
	tie(std::ignore, inserted) = dispatch_dict.insert({type_name, {func, info}});
	if (!inserted)
	{
#ifndef NO_THROW_DISPATCHER
		throw std::runtime_error("trying to register dispatcher for \"" +
			type_name + "\", which is already registered");
#endif
	}
} // reg_dispatcher }}}


const VMDispatcherFunc& Mata::VM::find_dispatcher(
	const std::string&       type_name)
{ // {{{
	auto it = dispatch_dict.find(type_name);
	if (dispatch_dict.end() == it)
	{
		throw std::runtime_error("cannot find the dispatcher for \"" +
			type_name + "\"");
	}

	return it->second.func;
} // find_dispatcher }}}


Mata::VM::VMTypeDesc Mata::VM::get_types_description()
{ // {{{
	VMTypeDesc desc;
	for (auto type_disp_pair : dispatch_dict) {
		desc.insert({type_disp_pair.first, type_disp_pair.second.info});
	}

	return desc;
} // get_types_description }}}
