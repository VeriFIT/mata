#include "mata/ta/abstraction.hh"
#include "mata/ta/builder.hh"
#include "mata/ta/inclusion.hh"
#include "mata/ta/simulation.hh"
#include "mata/ta/transducer-builder.hh"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using mata::Alphabet;
using mata::OnTheFlyAlphabet;
using mata::Symbol;
using mata::ta::State;
using mata::ta::Ta;
using mata::ta::TreeTransducer;

Ta unite(const Ta& lhs, const Ta& rhs) {
	Ta result{lhs.delta.alphabet()};
	for (const State state : lhs.final) { result.final.insert(state); }
	for (const auto& [_, transitions] : lhs.delta) {
		for (const auto& transition : transitions) { result.add_transition(transition); }
	}
	const State offset{mata::nfa::to_state(lhs.num_of_states())};
	for (const State state : rhs.final) { result.final.insert(state + offset); }
	for (const auto& [_, transitions] : rhs.delta) {
		for (auto transition : transitions) {
			for (State& child : transition.children) { child += offset; }
			transition.target += offset;
			result.add_transition(std::move(transition));
		}
	}
	return result;
}

bool equivalent(const Ta& lhs, const Ta& rhs) { return is_included(lhs, rhs) && is_included(rhs, lhs); }

struct Timings {
	double post{0};
	double collapse{0};
	double reduction{0};
	double inclusion{0};
	double backtrack{0};
};

template <typename F> auto timed(double& total, F&& operation) {
	const auto start{std::chrono::steady_clock::now()};
	auto result{operation()};
	total += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	return result;
}

class Cegar {
  public:
	Cegar(
		const Ta& initial,
		const Ta& bad,
		const TreeTransducer& step,
		const bool predicates,
		Timings& timings,
		const size_t reduction_min_states
	)
		: initial_{initial},
		  bad_{bad},
		  backward_{step.invert()},
		  step_{&step},
		  use_predicates_{predicates},
		  timings_{timings},
		  reduction_min_states_{reduction_min_states} {
		predicates_ = bad;
	}

	std::string run() {
		for (refinement_ = 0;; ++refinement_) {
			const auto verdict{fixpoint()};
			if (verdict == "safe") { return "safe, " + metric(); }
			if (verdict != "spurious") { return verdict; }
			const Ta refined{unite(predicates_, refinement_candidate_)};
			if (use_predicates_ && equivalent(refined, predicates_)) { return "unknown, abstraction cycling"; }
			predicates_ = refined;
		}
	}

  private:
	Ta initial_;
	Ta bad_;
	Ta predicates_;
	TreeTransducer backward_;
	const TreeTransducer* step_{nullptr};
	std::vector<Ta> concrete_posts_;
	Ta refinement_candidate_;
	const bool use_predicates_;
	Timings& timings_;
	const size_t reduction_min_states_{std::numeric_limits<size_t>::max()};
	size_t refinement_{0};

	std::string metric() const {
		return std::string{use_predicates_ ? "predicate refinements " : "height "} + std::to_string(refinement_);
	}

	Ta abstract(const Ta& aut) const {
		if (aut.num_of_states() < reduction_min_states_) { return collapse(aut); }
		const Ta reduced{timed(timings_.reduction, [&] { return reduce_by_composed_simulation(aut); })};
		return collapse(reduced);
	}

	Ta collapse(const Ta& aut) const {
		if (use_predicates_) {
			const Ta* predicate{&predicates_};
			return timed(timings_.collapse, [&] {
				return collapse_by_predicates(aut, std::span<const Ta* const>{&predicate, 1});
			});
		}
		return timed(timings_.collapse, [&] { return collapse_by_height(aut, refinement_); });
	}

	bool real_error(Ta candidate) const {
		for (size_t index{concrete_posts_.size()}; index > 0; --index) {
			candidate = intersection(concrete_posts_[index - 1], candidate);
			if (candidate.is_lang_empty()) { return false; }
			candidate = timed(timings_.backtrack, [&] { return backward_.apply(candidate); });
		}
		return !intersection(initial_, candidate).is_lang_empty();
	}

	std::string fixpoint() {
		concrete_posts_.clear();
		Ta reached{abstract(initial_)};
		for (size_t round{0};; ++round) {
			Ta candidate{intersection(reached, bad_)};
			if (!candidate.is_lang_empty()) {
				const bool real{timed(timings_.backtrack, [&] { return real_error(candidate); })};
				if (real) {
					return "unsafe, steps: " + std::to_string(round) + ", " +
						   (use_predicates_ ? "predicate refinements " : "height ") + std::to_string(refinement_);
				}
				refinement_candidate_ = std::move(candidate);
				return "spurious";
			}
			Ta next_post{timed(timings_.post, [&] { return step_->apply(reached); })};
			Ta next{abstract(unite(reached, next_post))};
			if (timed(timings_.inclusion, [&] { return is_included(next, reached); })) { return "safe"; }
			concrete_posts_.push_back(std::move(next_post));
			reached = std::move(next);
		}
	}
};

std::string verify(
	const std::filesystem::path& directory,
	const std::string& model_prefix,
	const std::string& step_name,
	const bool predicates,
	Timings& timings,
	const size_t reduction_min_states
) {
	OnTheFlyAlphabet alphabet{};
	const Ta initial{mata::ta::builder::parse_from_mata(directory / (model_prefix + "-initial.mata"), &alphabet)};
	const Ta bad{mata::ta::builder::parse_from_mata(directory / (model_prefix + "-bad.mata"), &alphabet)};
	const TreeTransducer step{mata::ta::transducer_builder::parse_from_mata(directory / step_name, &alphabet)};
	Cegar checker{initial, bad, step, predicates, timings, reduction_min_states};
	return checker.run();
}

} // namespace

int main(int argc, char** argv) {
	try {
		const std::filesystem::path directory{argc > 1 ? argv[1] : "examples/rmc"};
		size_t reduction_min_states{16};
		if (argc > 2) {
			reduction_min_states =
				std::string_view{argv[2]} == "off" ? std::numeric_limits<size_t>::max() : std::stoul(argv[2]);
		}
		struct Model {
			const char* name;
			const char* prefix;
			const char* step;
		};
		const Model models[]{
			{"token-passing", "ta-token", "ta-token-step.mata"},
			{"token-copying", "ta-token", "ta-token-copying-step.mata"},
			{"percolate", "ta-percolate", "ta-percolate-step.mata"},
		};
		for (const bool predicates : {true, false}) {
			for (const auto& [name, prefix, step] : models) {
				Timings timings{};
				const std::string verdict{verify(directory, prefix, step, predicates, timings, reduction_min_states)};
				std::cout << name << ": " << verdict << '\n';
				std::cout << "post: " << timings.post << "\ncollapse: " << timings.collapse
						  << "\nreduction: " << timings.reduction << "\ninclusion: " << timings.inclusion
						  << "\nbacktrack: " << timings.backtrack << '\n';
			}
		}
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 2;
	}
}
