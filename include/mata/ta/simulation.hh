/** @file
 * @brief Simulation relations and language-preserving reduction of ranked tree automata.
 */

#ifndef MATA_TA_SIMULATION_HH
#define MATA_TA_SIMULATION_HH

#include "mata/simlib/util/binary_relation.hh"
#include "mata/ta/ta.hh"

namespace mata::ta {

enum class SimulationDirection {
	Downward,
	Upward,
};

/**
 * Computes the maximal simulation in @p direction.
 *
 * The relation uses the same convention as NFA simulation: @c get(q, r) means that @p r simulates @p q. Upward
 * simulation is parameterized by the maximal downward simulation.
 */
Simlib::Util::BinaryRelation compute_simulation(const Ta& aut, SimulationDirection direction);

/**
 * Quotients mutual simulation classes and trims the result.
 *
 * Only downward simulation is a sound stand-alone reduction. Passing @c SimulationDirection::Upward throws
 * @c std::invalid_argument; use @c reduce_by_composed_simulation() to include upward information safely.
 */
Ta reduce_by_simulation(const Ta& aut, SimulationDirection direction = SimulationDirection::Downward);

/// Computes downward and downward-parameterized upward simulation, then quotients their composed mutual classes.
Ta reduce_by_composed_simulation(const Ta& aut);

} // namespace mata::ta

#endif // MATA_TA_SIMULATION_HH
