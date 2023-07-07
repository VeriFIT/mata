// TODO: add header

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>

#include "mata/utils/util.hh"

#include "../3rdparty/args.hxx"

/// declaration of external identifiers
extern const char* MATA_VERSION;
extern const char* MATA_GIT_SHA;
extern const char* MATA_GIT_DESCRIBE;

int interpret_input(std::istream& is);

/// maximum level of verbosity
const unsigned MAX_VERBOSITY = 5;
const unsigned DEFAULT_VERBOSITY = 1;

/// The entry point
int main(int argc, const char* argv[])
{
	args::ArgumentParser arg_parser("A CLI interface to the libMATA automata library");
	args::HelpFlag flag_help(arg_parser, "help", "Display this help menu", {'h', "help"});
	args::Flag flag_version(arg_parser, "version", "Print the version of MATA",
		{'v', "version"});
	args::ValueFlag<unsigned> flag_debug(arg_parser, "level", "Debug level (from 0 to " +
		std::to_string(MAX_VERBOSITY) + ")", {'d', "debug"}, DEFAULT_VERBOSITY);
	args::Positional<std::string> pos_inputfile(arg_parser,
		"input", "An input .vtf @CODE file; if not supplied, read from STDIN");
	arg_parser.helpParams.showTerminator = false;

	try {
		arg_parser.ParseCLI(argc, argv);
	}
	catch (const args::Help&) {
		std::cout << arg_parser;
		return EXIT_SUCCESS;
	}
	catch (const args::Error& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	int ret_val;

	if (flag_version) {
		std::cout << "mata-code version " << MATA_VERSION;
		std::cout << " (" << MATA_GIT_DESCRIBE << ")";

		size_t git_sha_len = std::strlen(MATA_GIT_SHA);
		std::string git_sha_crop =
			(git_sha_len < 8)? MATA_GIT_SHA : std::string(MATA_GIT_SHA, 8);

		std::cout << " [git: " << git_sha_crop << "]";
		std::cout << "\n";
		return EXIT_SUCCESS;
	}

	unsigned verbosity = flag_debug.Get();
	if (verbosity > MAX_VERBOSITY) {
		verbosity = MAX_VERBOSITY;
	}
    Mata::LOG_VERBOSITY = verbosity;
	DEBUG_PRINT("verbosity set to " + std::to_string(Mata::LOG_VERBOSITY));

	if (pos_inputfile) {
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
