// TODO: add header

#include <iostream>

#include <vata2/parser.hh>
#include <vata2/vm.hh>

int interpret_input(std::istream& is)
{
	try
	{
		Vata2::Parser::Parsed parsed = Vata2::Parser::parse_vtf(is);

		Vata2::VM::VirtualMachine mach;
		mach.run(parsed);

		std::cout << "VATA-CODE START\n";

		for (auto elem : parsed)
		{
			std::cout << std::to_string(elem);
		}

		std::cout << "VATA-CODE END\n";
	}
	catch (const std::exception& ex)
	{
		std::cerr << "libVATA2 error: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
