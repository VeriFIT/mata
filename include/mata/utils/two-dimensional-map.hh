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
 * @tparam TrackInverted Whether to track inverted indices for the first and second dimensions.
 *         To use this feature correctly, the mapping has to be reversible.
 * @tparam MaxMatrixSize Maximum size of the matrix before switching to vector of unordered maps.
 */
template <typename T, bool TrackInverted = true, size_t MaxMatrixSize = 50'000'000>
class TwoDimensionalMap {
    static_assert(std::is_unsigned_v<T>, "ProductStorage requires an unsigned type");

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
        : is_large_(first_dim_size * second_dim_size > MaxMatrixSize),
          first_dim_size_(first_dim_size), second_dim_size_(second_dim_size)
    {
        assert(first_dim_size < std::numeric_limits<T>::max());
        assert(second_dim_size < std::numeric_limits<T>::max());
        if (!is_large_) {
            matrix_storage_ = MatrixStorage(first_dim_size, InvertedStorage(second_dim_size, std::numeric_limits<T>::max()));
        } else {
            vec_map_storage_ = VecMapStorage(first_dim_size);
        }
        if constexpr (TrackInverted) {
            first_dim_inverted_.resize(first_dim_size + second_dim_size);
            second_dim_inverted_.resize(first_dim_size + second_dim_size);
        }
    }

    /**
     * @brief Get the value associated with a pair of keys.
     * @param first First key.
     * @param second Second key.
     * @return The value associated with the pair, or std::numeric_limits<T>::max() if not found.
     */
    T get(const T first, const T second) const {
        if (!is_large_) {
            return matrix_storage_[first][second];
        }

        auto it = vec_map_storage_[first].find(second);
        if (it == vec_map_storage_[first].end()) {
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
        if (!is_large_) {
            matrix_storage_[first][second] = value;
        } else {
            vec_map_storage_[first][second] = value;
        }
        if constexpr (TrackInverted) {
            first_dim_inverted_.resize(value + 1);
            second_dim_inverted_.resize(value + 1);
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
        return first_dim_inverted_[value];
    }

    /**
     * @brief Get the second inverted index for a @p value.
     * This is only available if track_inverted is true.
     */
    T get_second_inverted(const T value) const {
        static_assert(TrackInverted, "get_second_inverted only available if track_inverted is true");
        return second_dim_inverted_[value];
    }

private:
    const bool is_large_;
    const size_t first_dim_size_;
    const size_t second_dim_size_;
    MatrixStorage matrix_storage_{};
    std::vector<std::unordered_map<T, T>> vec_map_storage_{};
    InvertedStorage first_dim_inverted_{};
    InvertedStorage second_dim_inverted_{};

};

} // namespace mata::utils

#endif // MATA_UTILS_TWO_DIMENSIONAL_MAP_HH
