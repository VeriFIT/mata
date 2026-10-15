/**
 * Benchmark of the regex conversion and of its post-processing steps, each with its own timer.
 *
 * For every regex, times the direct epsilon-free conversion and the explicit epsilon translation (both including
 *  parsing and compiling the regex with RE2), then remove_epsilon() of the explicit translation, trim() of the direct
 *  conversion, and simulation reduction of the trimmed direct conversion. Pass regexes as arguments, or run without
 *  arguments for a built-in set of epsilon-rich, literal-heavy and optional-heavy regexes.
 */

#include "utils/utils.hh"

#include "mata/parser/re2parser.hh"

#include <vector>

namespace {

std::vector<std::string> default_regexes() {
	std::string literal;
	for (size_t index{0}; index < 2'000; ++index) { literal.push_back(static_cast<char>('a' + index % 26)); }
	std::string optionals;
	for (size_t index{0}; index < 300; ++index) { optionals += "a?"; }
	return {
		"(?:[a-z]b|c[d-f]){1000}",		   literal, optionals + "b", "(?:a?b?c?){100}d", "(?:((a)*)*b|(c(d)*)){200}",
		"(?:(?:ab|cd)*?e+?(?:f|g|)){200}",
	};
}

} // namespace

int main(int argc, char* argv[]) {
	std::vector<std::string> regexes{argv + 1, argv + argc};
	if (regexes.empty()) { regexes = default_regexes(); }

	// Setting precision of the times to fixed points and 4 decimal places
	std::cout << std::fixed << std::setprecision(4);

	using mata::parser::Re2Options;
	constexpr mata::Symbol epsilon{306};
	for (const std::string& regex : regexes) {
		std::cout << "regex: " << (regex.size() > 60 ? regex.substr(0, 57) + "..." : regex) << "\n";

		TIME_BEGIN(convert_with_epsilon);
		const Nfa with_epsilon{mata::parser::create_nfa(
			regex, Re2Options{.keep_epsilon = true, .epsilon = epsilon, .trim = false, .reduce = false}
		)};
		TIME_END(convert_with_epsilon);

		TIME_BEGIN(remove_epsilon);
		const Nfa epsilon_removed{remove_epsilon(with_epsilon, epsilon)};
		TIME_END(remove_epsilon);

		TIME_BEGIN(convert_direct);
		Nfa direct{mata::parser::create_nfa(regex, Re2Options{.trim = false, .reduce = false})};
		TIME_END(convert_direct);
		const size_t direct_states{direct.num_of_states()};
		const size_t direct_transitions{direct.delta.num_of_transitions()};

		TIME_BEGIN(trim);
		direct.trim();
		TIME_END(trim);

		TIME_BEGIN(reduce);
		const Nfa reduced{reduce(direct)};
		TIME_END(reduce);

		std::cout << "epsilon translation states: " << with_epsilon.num_of_states()
				  << "\nepsilon translation transitions: " << with_epsilon.delta.num_of_transitions()
				  << "\ndirect states: " << direct_states << "\ndirect transitions: " << direct_transitions
				  << "\ntrimmed states: " << direct.num_of_states() << "\nreduced states: " << reduced.num_of_states()
				  << "\n";
	}
	return EXIT_SUCCESS;
}
