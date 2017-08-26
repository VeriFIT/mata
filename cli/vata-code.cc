// TODO: add header

#include <cstdlib>
#include <cstring>
#include <fstream>

#include <vata2/util.hh>

#include "../3rdparty/args.hxx"

/// declaration of external identifiers
extern const char* VATA_VERSION;
extern const char* VATA_GIT_SHA;
extern const char* VATA_GIT_DESCRIBE;

void interpret_input(const std::istream& is);


/// The entry point
int main(int argc, const char* argv[])
{
	args::ArgumentParser parser("A CLI interface to the libVATA2 automata library");
	args::HelpFlag flag_help(parser, "help", "Display this help menu", {'h', "help"});
	args::Flag flag_version(parser, "version", "test flag", {'v', "version"});
	args::Positional<std::string> pos_inputfile(parser,
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

	if (flag_version)
	{
		std::cout << "vata-code version " << VATA_VERSION;
		std::cout << " (" << VATA_GIT_DESCRIBE << ")";

		size_t git_sha_len = std::strlen(VATA_GIT_SHA);
		std::string git_sha_crop =
			(git_sha_len < 8)? VATA_GIT_SHA : std::string(VATA_GIT_SHA, 8);

		std::cout << " [git: " << git_sha_crop << "]";
		std::cout << "\n";
	}
	else if (pos_inputfile)
	{
		std::string filename = args::get(pos_inputfile);
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
