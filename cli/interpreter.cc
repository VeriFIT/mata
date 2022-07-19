// TODO: add header

#include <iostream>

#include <mata/parser.hh>
#include <mata/vm.hh>

int interpret_input(std::istream& is)
{
	try {
		Mata::Parser::Parsed parsed = Mata::Parser::parse_vtf(is, true);

		Mata::VM::VirtualMachine mach;
		mach.run(parsed);
	}
	catch (const std::exception& ex) {
		std::cerr << "libMATA error: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
