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

int interpret_input(std::istream& is);


/// The entry point
int main(int argc, const char* argv[])
{
	args::ArgumentParser arg_parser("A CLI interface to the libVATA2 automata library");
	args::HelpFlag flag_help(arg_parser, "help", "Display this help menu", {'h', "help"});
	args::Flag flag_version(arg_parser, "version", "test flag", {'v', "version"});
	args::Positional<std::string> pos_inputfile(arg_parser,
		"input", "An input .vtf @CODE file; if not supplied, read from STDIN");
	arg_parser.helpParams.showTerminator = false;

	try {
		arg_parser.ParseCLI(argc, argv);
	}
	catch (args::Help) {
		std::cout << arg_parser;
		return EXIT_SUCCESS;
	}
	catch (args::Error e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	int ret_val;

	if (flag_version) {
		std::cout << "vata-code version " << VATA_VERSION;
		std::cout << " (" << VATA_GIT_DESCRIBE << ")";

		size_t git_sha_len = std::strlen(VATA_GIT_SHA);
		std::string git_sha_crop =
			(git_sha_len < 8)? VATA_GIT_SHA : std::string(VATA_GIT_SHA, 8);

		std::cout << " [git: " << git_sha_crop << "]";
		std::cout << "\n";
		return EXIT_SUCCESS;
	} else if (pos_inputfile) {
		std::string filename = args::get(pos_inputfile);
		std::fstream fs(filename, std::ios::in);
		if (!fs) {
			std::cerr << "Could not open file \'" << filename << "'\n";
			return EXIT_FAILURE;
		}

		ret_val = interpret_input(fs);

		fs.close();
	} else { // the input goes from stdin
		ret_val = interpret_input(std::cin);
	}

	return ret_val;
}
