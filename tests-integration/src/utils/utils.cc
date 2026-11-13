#include "utils.hh"
#include "mata/nfa/builder.hh"
#include "mata/nfa/types.hh"
#include "mata/parser/inter-aut.hh"
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace {
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
	// The inputs the fast reader of explicit alphabets reads are constructed right away. Bitvector inputs go to the
	//  bitvector reader, which mintermizes them together; the others go through the general reader.
	std::vector<std::optional<Nfa>> loaded;
	mata::nfa::builder::BitvectorNfaReader bitvector_reader;
	bool has_bitvector{false};
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
			if (bitvector_reader.add(input, true) != mata::nfa::builder::BitvectorNfaReader::Read::Rejected) {
				has_bitvector = true;
				continue;
			}
			std::vector<mata::IntermediateAut> inter_auts;
			if (load_intermediate_automaton(filename, inter_auts) != EXIT_SUCCESS) {
				std::cerr << "Could not load intermediate autotomaton from \'" << filename << "'\n";
				return EXIT_FAILURE;
			}
			loaded.back() = mata::nfa::builder::construct(inter_auts.back(), &alphabet);
		}
	} catch (const std::exception& ex) {
		std::cerr << "error: " << ex.what() << "\n";
		return EXIT_FAILURE;
	}
	TIME_END(parsing);
	// The bitvector symbols exist only as minterms, so without mintermization the benchmark would silently measure a
	//  different automaton; hence this combination is rejected.
	if (has_bitvector && !mintermize_automata) {
		std::cerr << "error: the input uses a bitvector alphabet, which cannot be constructed without "
					 "mintermization; set `MINTERMIZE_AUTOMATA` to `true` for such inputs\n";
		return EXIT_FAILURE;
	}
	try {
		std::vector<Nfa> mintermized;
		if (has_bitvector) {
			TIME_BEGIN(mintermization);
			mintermized = bitvector_reader.build(&alphabet);
			TIME_END(mintermization);
		}
		auto next_mintermized{mintermized.begin()};
		for (std::optional<Nfa>& aut : loaded) {
			auts.push_back(aut.has_value() ? std::move(*aut) : std::move(*next_mintermized++));
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
