// TODO: add header

#include <vata2/vm.hh>
#include <vata2/vm-dispatch.hh>




void Vata2::VM::VirtualMachine::run(const Vata2::Parser::Parsed& parsed)
{ // {{{
	for (const auto& parsec : parsed)
	{
		this->run(parsec);
	}
} // run(Parsed) }}}


void Vata2::VM::VirtualMachine::run(const Vata2::Parser::ParsedSection& parsec)
{ // {{{
	if (parsec.type == "CODE")
	{
		assert(false);
	}

	std::cout << parsec.type << "\n";

	VMDispatcherFunc dispatch = Vata2::VM::find_dispatcher(parsec.type);

	VMFuncArgs args = {{"Parsec", &parsec}};
	VMValue val = dispatch("construct", args);

	assert(false);
} // run(ParsedSection) }}}
