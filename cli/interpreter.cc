// TODO: add header

#include <iostream>

#include "mata/parser/parser.hh"

int interpret_input(std::istream& is)
{
	try {
		Mata::Parser::Parsed parsed = Mata::Parser::parse_mf(is, true);
        // TODO: implement a simple CLI instead of the one based on virtual machine
	}
	catch (const std::exception& ex) {
		std::cerr << "libMATA error: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
