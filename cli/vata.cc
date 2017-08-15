// TODO: add header

#include <cstdlib>
#include <fstream>

#include <vata2/util.hh>

#include "../3rdparty/args.hxx"


void interpret_input(const std::istream& is)
{
	std::cout << is.rdbuf();
}


/// The entry point
int main(int argc, const char* argv[])
{
	args::ArgumentParser parser("An interface to the libVATA2 automata library");
	args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
	args::Positional<std::string> inputfile(parser,
		"input", "An input .vtf @CODE file; if not supplied, read from STDIN");
	parser.helpParams.showTerminator = false;

	try
	{
		parser.ParseCLI(argc, argv);
	}
	catch (args::Help)
	{
		std::cout << parser;
		return EXIT_SUCCESS;
	}
	catch (args::Error e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	if (inputfile)
	{
		std::string filename = args::get(inputfile);
		std::fstream fs(filename, std::ios::in);
		if (!fs)
		{
			std::cerr << "Could not open file \'" << filename << "'\n";
		}

		interpret_input(fs);

		fs.close();
	}
	else
	{ // the input goes from stdin
		interpret_input(std::cin);
	}

	return EXIT_SUCCESS;
}
