// TODO: add header

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>

#include <vata2/util.hh>
#include <vata2/vm-dispatch.hh>

#include "../3rdparty/args.hxx"

/// declaration of external identifiers
extern const char* VATA_VERSION;
extern const char* VATA_GIT_SHA;
extern const char* VATA_GIT_DESCRIBE;

int interpret_input(std::istream& is);

/// maximum level of verbosity
const unsigned MAX_VERBOSITY = 3;
const unsigned DEFAULT_VERBOSITY = 1;

/// The entry point
int main(int argc, const char* argv[])
{
	args::ArgumentParser arg_parser("A CLI interface to the libVATA2 automata library");
	args::HelpFlag flag_help(arg_parser, "help", "Display this help menu", {'h', "help"});
	args::Flag flag_version(arg_parser, "version", "Print the version of VATA",
		{'v', "version"});
	args::Flag flag_types(arg_parser, "types", "Print out info about types", {'t', "types"});
	args::Flag flag_verbose(arg_parser, "types", "Print out info about types", {'t', "types"});
	args::ValueFlag<unsigned> flag_debug(arg_parser, "level", "Debug level (from 0 to " +
		std::to_string(MAX_VERBOSITY) + ")", {'d', "debug"}, DEFAULT_VERBOSITY);
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
	} else if (flag_types) {
		try {
			Vata2::VM::VMTypeDesc desc = Vata2::VM::get_types_description();

			std::vector<std::pair<std::string, std::string>> desc_vec(desc.begin(), desc.end());
			size_t max_len = 1;
			for (auto type_desc_pair : desc_vec) {
				max_len = std::max(max_len, type_desc_pair.first.length());
			}
			size_t offset = max_len + 3;

			std::sort(desc_vec.begin(), desc_vec.end());

			for (auto type_desc_pair : desc_vec) {
				std::cout << std::left << std::setw(offset) << type_desc_pair.first;
				std::cout << type_desc_pair.second << "\n";
			}

			return EXIT_SUCCESS;
		}
		catch (const std::exception& ex) {
			std::cerr << "error while getting type information: " << ex.what() << "\n";
			return EXIT_FAILURE;
		}
	}

	unsigned verbosity = flag_debug.Get();
	if (verbosity > MAX_VERBOSITY) {
		verbosity = MAX_VERBOSITY;
	}
	Vata2::LOG_VERBOSITY = verbosity;
	DEBUG_PRINT("verbosity set to " + std::to_string(Vata2::LOG_VERBOSITY));

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
