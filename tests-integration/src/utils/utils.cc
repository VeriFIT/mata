#include "utils.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/types.hh"
#include "mata/parser/inter-aut.hh"
#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace {
bool has_bitvector_alphabet(const std::vector<mata::IntermediateAut>& inter_auts) {
	return std::ranges::any_of(inter_auts, [](const mata::IntermediateAut& inter_aut) {
		return inter_aut.alphabet_type == mata::IntermediateAut::AlphabetType::Bitvector;
	});
}

/// `builder::construct()` reads the operands of a transition as literal symbol names, so a bitvector
///  automaton has to be mintermized before it is constructed. Without mintermization the benchmark
///  would silently measure a different automaton, hence this combination is rejected.
bool reject_unmintermized_bitvector(const std::vector<mata::IntermediateAut>& inter_auts, const bool mintermize) {
	if (mintermize or !has_bitvector_alphabet(inter_auts)) { return false; }
	std::cerr << "error: the input uses a bitvector alphabet, which cannot be constructed without "
				 "mintermization; set `MINTERMIZE_AUTOMATA` to `true` for such inputs\n";
	return true;
}

/// Announce on stderr, once per process, which build produced this binary. A benchmark run against
///  a Debug `libmata.a` measures assertion checks at `-O0`, and nothing in the result CSV would
///  otherwise say so. stderr, not stdout: stdout carries the `name: seconds` lines pycobench parses.
struct BuildTypeAnnouncement {
	BuildTypeAnnouncement() {
#ifdef NDEBUG
		const std::string_view assertions{};
#else
		const std::string_view assertions{" (assertions enabled; not comparable with a Release run)"};
#endif
		std::cerr << "build: " << MATA_BENCH_BUILD_TYPE << assertions << "\n";
	}
};

const BuildTypeAnnouncement build_type_announcement{};
} // namespace

int load_automaton(
	const std::string& filename, Nfa& aut, mata::OnTheFlyAlphabet& alphabet, const bool mintermize_automata
) {
	std::vector<std::string> filenames{filename};
	std::vector<Nfa> auts;
	if (load_automata(filenames, auts, alphabet, mintermize_automata) != EXIT_SUCCESS) { return EXIT_FAILURE; }
	aut = std::move(auts[0]);
	return EXIT_SUCCESS;
}

int load_automata(
	std::vector<std::string>& filenames,
	std::vector<Nfa>& auts,
	mata::OnTheFlyAlphabet& alphabet,
	const bool mintermize_automata
) {
	// The inputs the fast reader of explicit alphabets reads are constructed right away; the others go through the
	//  general reader, and those with a bitvector alphabet are mintermized together.
	std::vector<std::optional<Nfa>> loaded;
	std::vector<mata::IntermediateAut> inter_auts;
	TIME_BEGIN(parsing);
	try {
		for (const std::string& filename : filenames) {
			std::ifstream file{filename};
			if (!file) {
				std::cerr << "Could not open file \'" << filename << "'\n";
				return EXIT_FAILURE;
			}
			const std::string input{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
			loaded.push_back(mata::nfa::builder::parse_explicit_from_mata(input, &alphabet, nullptr, true));
			if (loaded.back().has_value()) { continue; }
			if (load_intermediate_automaton(filename, inter_auts) != EXIT_SUCCESS) {
				std::cerr << "Could not load intermediate autotomaton from \'" << filename << "'\n";
				return EXIT_FAILURE;
			}
			if (!inter_auts.back().is_bitvector()) {
				loaded.back() = mata::nfa::builder::construct(inter_auts.back(), &alphabet);
				inter_auts.pop_back();
			}
		}
	} catch (const std::exception& ex) {
		std::cerr << "error: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}
	TIME_END(parsing);
	if (reject_unmintermized_bitvector(inter_auts, mintermize_automata)) { return EXIT_FAILURE; }
	try {
		std::vector<mata::IntermediateAut> mintermized;
		if (!inter_auts.empty()) {
			mata::Mintermization mintermization;
			TIME_BEGIN(mintermization);
			mintermized = mintermization.mintermize(inter_auts);
			TIME_END(mintermization);
		}
		auto next_mintermized{mintermized.begin()};
		for (std::optional<Nfa>& aut : loaded) {
			if (aut.has_value()) {
				auts.push_back(std::move(*aut));
			} else {
				// Mintermization always rewrites the alphabet to Explicit (see Mintermization::mintermize()).
				assert(next_mintermized->alphabet_type == mata::IntermediateAut::AlphabetType::Explicit);
				auts.push_back(mata::nfa::builder::construct(*next_mintermized++, &alphabet));
			}
		}
		return EXIT_SUCCESS;
	} catch (const std::exception& ex) {
		std::cerr << "error: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}
}

int load_intermediate_automaton(const std::string& filename, std::vector<mata::IntermediateAut>& out_inter_auts) {
	std::fstream fs(filename, std::ios::in);
	if (!fs) {
		std::cerr << "Could not open file \'" << filename << "'\n";
		return EXIT_FAILURE;
	}

	mata::parser::Parsed parsed;
	try {
		parsed = mata::parser::parse_mf(fs, true);
		fs.close();

		if (parsed.size() != 1) { throw std::runtime_error("The number of sections in the input file is not 1\n"); }
		if (!parsed[0].type.starts_with(TYPE_NFA)) {
			std::cout << (parsed[0].type) << std::endl;
			throw std::runtime_error("The type of input automaton is not NFA\n");
		}

		std::vector<mata::IntermediateAut> inter_auts = mata::IntermediateAut::parse_from_mf(parsed);
		out_inter_auts.push_back(inter_auts[0]);
	} catch (const std::exception& ex) {
		fs.close();
		std::cerr << "error: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
