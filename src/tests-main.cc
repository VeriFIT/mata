// TODO: some header

#define CATCH_CONFIG_RUNNER
#include "../3rdparty/catch.hpp"
#include <mata/util.hh>

int main( int argc, char* argv[])
{
	// taken from
	//   https://github.com/catchorg/Catch2/blob/master/docs/own-main.md
	Catch::Session session; // There must be exactly one instance

	Mata::LOG_VERBOSITY = 0;
	int debug_lvl = 0;

	// Build a new parser on top of Catch's
	using namespace Catch::clara;
	auto cli = session.cli()                   // Get Catch's composite command line parser
		| Opt(debug_lvl, "0|1")                  // bind variable to a new option, with a hint string
				["--debug"]                          // the option names it will respond to
				("turn on/off debug mode of MATA");  // description string for the help output

	// Now pass the new composite back to Catch so it uses that
	session.cli(cli);

	// Let Catch (using Clara) parse the command line
	int returnCode = session.applyCommandLine(argc, argv);
	if (0 != returnCode) { // Indicates a command line error
		return returnCode;
	}

	// if set on the command line then 'height' is now set at this point
	if (debug_lvl > 0) {
        Mata::LOG_VERBOSITY = 100;
	}

	return session.run();
}
