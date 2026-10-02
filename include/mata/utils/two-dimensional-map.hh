/**
 * @file two-dimensional-map.hh
 * @brief Implementation of a two-dimensional map from pairs to single values.
 */

#ifndef MATA_UTILS_TWO_DIMENSIONAL_MAP_HH
#define MATA_UTILS_TWO_DIMENSIONAL_MAP_HH

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "mata/utils/assert.hh"
#include "mata/utils/utils.hh"

namespace mata::utils {

/**
 * @brief A two-dimensional map that can be used to store pairs of values and their associated value.
 * This class can handle both small and large maps efficiently.
 *
 * Values are stored in a matrix of @c Cell cells (4 Bytes each, unless @p T is smaller). The matrix rows are
 * allocated lazily: a row is allocated and zeroed when the first value is inserted into it, so the memory
 * footprint is proportional to the number of first-dimension keys actually used, not to the size of the first
 * dimension. A cell holds the stored value incremented by one; zero means that no value is stored.
 *
 * The largest matrix we are brave enough to allocate is @p MaxMatrixBytes Bytes, 200 MB by default, which is
 * 50 million cells of 4 Bytes. For larger dimensions we do not allocate a matrix, but use a vector of
 * unordered maps (vec_map_storage_). The unordered_map seems to be about twice slower.
 *
 * @tparam T Type of the values stored in the map. Must be an unsigned type.
 * @tparam TrackInverted Whether to track inverted indices for the first and second dimensions.
 *         To use this feature correctly, the mapping has to be reversible.
 * @tparam MaxMatrixBytes Maximum size of the matrix in Bytes before switching to vector of unordered maps.
 */
template <typename T, bool TrackInverted = true, size_t MaxMatrixBytes = 200'000'000> class TwoDimensionalMap {
	static_assert(std::is_unsigned_v<T>, "TwoDimensionalMap requires an unsigned type");

  public:
	using Map = std::unordered_map<std::pair<T, T>, T, PairHash<T, T>>;
	/// A matrix cell stores the value incremented by one; zero denotes a missing value.
	using Cell = std::conditional_t<(sizeof(T) < sizeof(uint32_t)), T, uint32_t>;
	using Row = std::unique_ptr<Cell[]>;
	using MatrixStorage = std::vector<Row>;
	using VecMapStorage = std::vector<std::unordered_map<T, T>>;
	using InvertedStorage = std::vector<T>;

	/// Returned by get() when no value is stored for the pair of keys.
	static constexpr T no_value{std::numeric_limits<T>::max()};
	/// The largest value a matrix cell can hold.
	static constexpr T max_value{static_cast<T>(std::numeric_limits<Cell>::max() - 1)};

	/**
	 * @brief Constructor for TwoDimensionalMap.
	 * @param first_dim_size Size of the first dimension.
	 * @param second_dim_size Size of the second dimension.
	 */
	TwoDimensionalMap(const size_t first_dim_size, const size_t second_dim_size)
		: is_large_{is_matrix_too_large(first_dim_size, second_dim_size)},
		  first_dim_size_{first_dim_size},
		  second_dim_size_{second_dim_size} {
		MATA_ASSERT(first_dim_size_ < std::numeric_limits<T>::max());
		MATA_ASSERT(second_dim_size_ < std::numeric_limits<T>::max());
		// Only the vector of rows is allocated here. The rows themselves are allocated on first insert().
		if (!is_large_) {
			matrix_storage_ = MatrixStorage(first_dim_size_);
		} else {
			vec_map_storage_ = VecMapStorage(first_dim_size_);
		}
	}

	/**
	 * @brief Get the value associated with a pair of keys.
	 * @param first First key.
	 * @param second Second key.
	 * @return The value associated with the pair, or no_value (std::numeric_limits<T>::max()) if not found.
	 */
	T get(const T first, const T second) const {
		MATA_ASSERT(first < first_dim_size_);
		MATA_ASSERT(second < second_dim_size_);
		if (!is_large_) {
			const Row& row{matrix_storage_[first]};
			if (row == nullptr) { return no_value; }
			const Cell cell{row[second]};
			return cell == 0 ? no_value : static_cast<T>(cell - 1);
		}

		const std::unordered_map<T, T>& map{vec_map_storage_[first]};
		const auto it{map.find(second)};
		if (it == map.end()) { return no_value; }
		return it->second;
	}

	/**
	 * @brief Insert a value associated with a pair of keys.
	 * @param first First key.
	 * @param second Second key.
	 * @param value Value to associate with the pair.
	 */
	void insert(const T first, const T second, const T value) {
		MATA_ASSERT(first < first_dim_size_);
		MATA_ASSERT(second < second_dim_size_);
		if (!is_large_) {
			MATA_ASSERT(value <= max_value, "value {} does not fit into a matrix cell", value);
			Row& row{matrix_storage_[first]};
			// std::make_unique<Cell[]>() value-initialises the cells, marking the whole row as empty.
			if (row == nullptr) { row = std::make_unique<Cell[]>(second_dim_size_); }
			row[second] = static_cast<Cell>(value + 1);
		} else {
			vec_map_storage_[first][second] = value;
		}
		if constexpr (TrackInverted) {
			// The inverted arrays only ever grow: resizing them to value + 1 unconditionally would drop the
			// entries of all larger values inserted before.
			if (first_dim_inverted_.size() <= value) {
				first_dim_inverted_.resize(value + 1);
				second_dim_inverted_.resize(value + 1);
			}
			first_dim_inverted_[value] = first;
			second_dim_inverted_[value] = second;
		}
	}

	/**
	 * @brief Get the first inverted index for a @p value.
	 * This is only available if track_inverted is true.
	 */
	T get_first_inverted(const T value) const {
		static_assert(TrackInverted, "get_first_inverted only available if track_inverted is true");
		MATA_ASSERT(value < first_dim_inverted_.size());
		return first_dim_inverted_[value];
	}

	/**
	 * @brief Get the second inverted index for a @p value.
	 * This is only available if track_inverted is true.
	 */
	T get_second_inverted(const T value) const {
		static_assert(TrackInverted, "get_second_inverted only available if track_inverted is true");
		MATA_ASSERT(value < second_dim_inverted_.size());
		return second_dim_inverted_[value];
	}

  private:
	/// Whether a matrix of @p first_dim_size x @p second_dim_size cells would exceed @c MaxMatrixBytes.
	static constexpr bool is_matrix_too_large(const size_t first_dim_size, const size_t second_dim_size) noexcept {
		constexpr size_t max_num_of_cells{MaxMatrixBytes / sizeof(Cell)};
		// Division instead of multiplication: first_dim_size * second_dim_size can overflow.
		return second_dim_size != 0 && first_dim_size > max_num_of_cells / second_dim_size;
	}

	const bool is_large_;
	const size_t first_dim_size_;
	const size_t second_dim_size_;
	MatrixStorage matrix_storage_{};
	VecMapStorage vec_map_storage_{};
	InvertedStorage first_dim_inverted_{};
	InvertedStorage second_dim_inverted_{};
};

} // namespace mata::utils

#endif // MATA_UTILS_TWO_DIMENSIONAL_MAP_HH
