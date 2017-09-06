// TODO: add header

#include <vata2/vm-dispatch.hh>

// Headers of user data types
#include <vata2/nfa.hh>

using Vata2::VM::VMDispatcherFunc;
using Vata2::VM::VMType;

/// A dictionary mapping types to dispatcher function pointers
using VMDispatcherDict = std::unordered_map<VMType, VMDispatcherFunc>;

/// init function type
using VMInitFunc = std::function<void()>;

// definitions
namespace
{

/// the dispatch function dictionary
VMDispatcherDict dispatch_dict = { };


/// list of init functions --- ADD HERE FUNCTIONS TO BE RUN AT STARTUP
const VMInitFunc INIT_FUNCTIONS[] =
{
	Vata2::Nfa::init,
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
	catch (const std::exception& ex)
	{
		std::cerr << "An error occurred at startup: " << ex.what() << "\n";
		exit(EXIT_FAILURE);
	}

	return true;
}

bool is_dispatch_dict_init = init_dispatch_dict();
} // anonymous


void Vata2::VM::reg_dispatcher(
	const VMType&            type_name,
	const VMDispatcherFunc&  func)
{ // {{{
	bool inserted;
	std::tie(std::ignore, inserted) = dispatch_dict.insert({type_name, func});
	if (!inserted)
	{
		throw std::runtime_error("trying to register dispatcher for \"" +
			type_name + "\", which is already registered");
	}
} // reg_dispatcher }}}


const VMDispatcherFunc& Vata2::VM::find_dispatcher(
	const VMType&            type_name)
{ // {{{
	auto it = dispatch_dict.find(type_name);
	if (dispatch_dict.end() == it)
	{
		throw std::runtime_error("cannot find the dispatcher for \"" +
			type_name + "\"");
	}

	return it->second;
} // find_dispatcher }}}
