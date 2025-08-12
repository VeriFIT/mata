/**
 * @file two-dimensional-map.hh
 * @brief Implementation of a two-dimensional map from pairs to single values.
 */

#ifndef MATA_UTILS_TWO_DIMENSIONAL_MAP_HH
#define MATA_UTILS_TWO_DIMENSIONAL_MAP_HH

#include <cassert>
#include <unordered_map>

namespace mata::utils {

/**
 * @brief A two-dimensional map that can be used to store pairs of values and their associated value.
 * This class can handle both small and large maps efficiently.
 *
 * The largest matrix of pairs of we are brave enough to allocate is 50'000'000 for 8 Byte values.
 * So ten million cells is close to 100 MB.
 * If the number is larger, then we do not allocate a matrix, but use a vector of unordered maps (vec_map_storage).
 * The unordered_map seems to be about twice slower.
 *
 * @tparam T Type of the values stored in the map. Must be an unsigned type.
 * @tparam track_inverted Whether to track inverted indices for the first and second dimensions.
 *         To use this feature correctly, the mapping has to be reversible.
 * @tparam MAX_MATRIX_SIZE Maximum size of the matrix before switching to vector of unordered maps.
 */
template <typename T, bool track_inverted = true, size_t MAX_MATRIX_SIZE = 50'000'000>
class TwoDimensionalMap {
    static_assert(std::is_unsigned<T>::value, "ProductStorage requires an unsigned type");

public:
    using Map = std::unordered_map<std::pair<T, T>, T>;
    using MatrixStorage = std::vector<std::vector<T>>;
    using VecMapStorage = std::vector<std::unordered_map<T, T>>;
    using InvertedStorage = std::vector<T>;

    /**
     * @brief Constructor for TwoDimensionalMap.
     * @param first_dim_size Size of the first dimension.
     * @param second_dim_size Size of the second dimension.
     */
    TwoDimensionalMap(const size_t first_dim_size, const size_t second_dim_size)
        : is_large(first_dim_size * second_dim_size > MAX_MATRIX_SIZE),
          first_dim_size(first_dim_size), second_dim_size(second_dim_size)
    {
        assert(first_dim_size < std::numeric_limits<T>::max());
        assert(second_dim_size < std::numeric_limits<T>::max());
        if (!is_large) {
            matrix_storage = MatrixStorage(first_dim_size, InvertedStorage(second_dim_size, std::numeric_limits<T>::max()));
        } else {
            vec_map_storage = VecMapStorage(first_dim_size);
        }
        if constexpr (track_inverted) {
            first_dim_inverted.resize(first_dim_size + second_dim_size);
            second_dim_inverted.resize(first_dim_size + second_dim_size);
        }
    }

    /**
     * @brief Get the value associated with a pair of keys.
     * @param first First key.
     * @param second Second key.
     * @return The value associated with the pair, or std::numeric_limits<T>::max() if not found.
     */
    T get(const T first, const T second) const {
        if (!is_large) {
            return matrix_storage[first][second];
        }

        auto it = vec_map_storage[first].find(second);
        if (it == vec_map_storage[first].end()) {
            return std::numeric_limits<T>::max();
        }
        return it->second;
    }

    /**
     * @brief Insert a value associated with a pair of keys.
     * @param first First key.
     * @param second Second key.
     * @param value Value to associate with the pair.
     */
    void insert(const T first, const T second, const T value) {
        if (!is_large) {
            matrix_storage[first][second] = value;
        } else {
            vec_map_storage[first][second] = value;
        }
        if constexpr (track_inverted) {
            first_dim_inverted.resize(value + 1);
            second_dim_inverted.resize(value + 1);
            first_dim_inverted[value] = first;
            second_dim_inverted[value] = second;
        }
    }

    /**
     * @brief Get the first inverted index for a @p value.
     * This is only available if track_inverted is true.
     */
    T get_first_inverted(const T value) const {
        static_assert(track_inverted, "get_first_inverted only available if track_inverted is true");
        return first_dim_inverted[value];
    }

    /**
     * @brief Get the second inverted index for a @p value.
     * This is only available if track_inverted is true.
     */
    T get_second_inverted(const T value) const {
        static_assert(track_inverted, "get_second_inverted only available if track_inverted is true");
        return second_dim_inverted[value];
    }

private:
    const bool is_large;
    const size_t first_dim_size;
    const size_t second_dim_size;
    MatrixStorage matrix_storage{};
    std::vector<std::unordered_map<T, T>> vec_map_storage{};
    InvertedStorage first_dim_inverted{};
    InvertedStorage second_dim_inverted{};

};

} // namespace mata::utils

#endif // MATA_UTILS_TWO_DIMENSIONAL_MAP_HH
