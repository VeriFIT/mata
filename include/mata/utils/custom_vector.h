#pragma once

/**
 * @file custom_vector.h
 * @brief High-performance std::vector-like container with contiguous storage.
 *
 * Design goals:
 * - Keep API close to std::vector for common operations.
 * - Prioritize speed for push/insert/erase using low-level storage control.
 * - Use power-of-two growth for very fast amortized append-heavy workloads.
 * - Provide fast paths for trivially copyable/destructible element types.
 */

#include <algorithm>
#include <bit>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T, typename Allocator = std::allocator<T>, bool EnableSBO = true>
class custom_vector {
public:
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    using alloc_traits = std::allocator_traits<allocator_type>;

    static constexpr size_type sbo_capacity_for_type() {
        if constexpr (sizeof(T) <= 8) {
            return 16;
        }
        else if constexpr (sizeof(T) <= 16) {
            return 8;
        }
        else if constexpr (sizeof(T) <= 32) {
            return 4;
        }
        else if constexpr (sizeof(T) <= 64) {
            return 2;
        }
        else {
            return 0;
        }
    }

    static constexpr size_type sbo_capacity = EnableSBO ? sbo_capacity_for_type() : 0;

    pointer data_ = nullptr;
    size_type size_ = 0;
    size_type capacity_ = 0;
    [[no_unique_address]] allocator_type alloc_;
    alignas(T) std::byte sbo_storage_[(sbo_capacity > 0) ? (sbo_capacity * sizeof(T)) : 1]{};

    // Customizable relocatability detection.
    // Use the library trait when available, otherwise fall back to a conservative
    // definition: trivially move-constructible + trivially destructible.
#if defined(__cpp_lib_is_trivially_relocatable)
    template <typename U>
    struct custom_vector_is_relocatable : std::bool_constant<std::is_trivially_relocatable<U>::value> {};
#else
    template <typename U>
    struct custom_vector_is_relocatable : std::bool_constant<(std::is_trivially_move_constructible_v<U> && std::is_trivially_destructible_v<U>)> {};
#endif

    static constexpr bool is_trivial_move_block_v = std::is_trivially_copyable_v<T>;
    static constexpr bool prefer_move_relocation_v =
        std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>;
    static constexpr bool can_realloc_trivial_v =
        (is_trivial_move_block_v || custom_vector_is_relocatable<T>::value) &&
        std::is_same_v<allocator_type, std::allocator<T>> &&
        (alignof(T) <= alignof(std::max_align_t));

    pointer sbo_data() noexcept {
        if constexpr (sbo_capacity > 0) {
            return reinterpret_cast<pointer>(sbo_storage_);
        }
        else {
            return nullptr;
        }
    }

    const_pointer sbo_data() const noexcept {
        if constexpr (sbo_capacity > 0) {
            return reinterpret_cast<const_pointer>(sbo_storage_);
        }
        else {
            return nullptr;
        }
    }

    bool is_sbo_pointer(const_pointer ptr) const noexcept {
        if constexpr (sbo_capacity > 0) {
            return ptr == sbo_data();
        }
        else {
            (void)ptr;
            return false;
        }
    }

    bool using_sbo() const noexcept {
        return is_sbo_pointer(data_);
    }

    void reset_to_sbo_empty() noexcept {
        if constexpr (sbo_capacity > 0) {
            data_ = sbo_data();
            capacity_ = sbo_capacity;
        }
        else {
            data_ = nullptr;
            capacity_ = 0;
        }
        size_ = 0;
    }

    static size_type next_power_of_two(size_type n) {
        return n <= 1 ? 1 : std::bit_ceil(n);
    }

    static size_type max_size_for_allocator(const allocator_type& alloc) noexcept {
        return alloc_traits::max_size(alloc);
    }

    pointer allocate_raw(size_type cap) {
        if (cap == 0) {
            return nullptr;
        }

        if constexpr (can_realloc_trivial_v) {
            void* raw = std::malloc(cap * sizeof(T));
            if (raw == nullptr) {
                throw std::bad_alloc();
            }
            return static_cast<pointer>(raw);
        }

        return alloc_traits::allocate(alloc_, cap);
    }

    void deallocate_raw(pointer ptr, size_type cap) noexcept {
        if (ptr == nullptr || is_sbo_pointer(ptr)) {
            return;
        }

        if constexpr (can_realloc_trivial_v) {
            (void)cap;
            std::free(static_cast<void*>(ptr));
            return;
        }

        alloc_traits::deallocate(alloc_, ptr, cap);
    }

    pointer reallocate_trivial_raw(pointer ptr, size_type new_capacity) {
        if (is_sbo_pointer(ptr)) {
            void* raw = std::malloc(new_capacity * sizeof(T));
            if (raw == nullptr) {
                throw std::bad_alloc();
            }

            if (size_ > 0) {
                std::memcpy(raw, ptr, size_ * sizeof(T));
            }
            return static_cast<pointer>(raw);
        }

        if (ptr == nullptr) {
            void* raw = std::malloc(new_capacity * sizeof(T));
            if (raw == nullptr) {
                throw std::bad_alloc();
            }
            return static_cast<pointer>(raw);
        }

        void* raw = std::realloc(static_cast<void*>(ptr), new_capacity * sizeof(T));
        if (raw == nullptr) {
            throw std::bad_alloc();
        }
        return static_cast<pointer>(raw);
    }

    template <typename... Args>
    void construct_at(size_type idx, Args&&... args) {
        alloc_traits::construct(alloc_, data_ + idx, std::forward<Args>(args)...);
    }

    void destroy_at(size_type idx) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            alloc_traits::destroy(alloc_, data_ + idx);
        }
    }

    void destroy_range(size_type first, size_type last) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = first; i < last; ++i) {
                alloc_traits::destroy(alloc_, data_ + i);
            }
        }
    }

    void destroy_raw_range(pointer ptr, size_type count) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = 0; i < count; ++i) {
                alloc_traits::destroy(alloc_, ptr + i);
            }
        }
    }

    void destroy_raw_at(pointer ptr) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            alloc_traits::destroy(alloc_, ptr);
        }
    }

    void relocate_range_to_uninitialized(pointer src, size_type count, pointer dst) {
        if (count == 0) {
            return;
        }

        if constexpr (is_trivial_move_block_v) {
            std::memcpy(dst, src, count * sizeof(T));
        }
        else if constexpr (prefer_move_relocation_v) {
            std::uninitialized_move_n(src, count, dst);
        }
        else {
            std::uninitialized_copy_n(src, count, dst);
        }
    }

    bool references_live_element(const T& value) const noexcept {
        if (data_ == nullptr || size_ == 0) {
            return false;
        }

        const T* ptr = std::addressof(value);
        return ptr >= data_ && ptr < (data_ + size_);
    }

    size_type growth_capacity_for(size_type min_required) const {
        if (min_required <= capacity_) {
            return capacity_;
        }

        const size_type max_cap = max_size_for_allocator(alloc_);
        if (min_required > max_cap) {
            throw std::length_error("custom_vector exceeds max_size");
        }

        if (capacity_ == 0) {
            return next_power_of_two(min_required);
        }

        size_type proposed = capacity_;
        while (proposed < min_required) {
            if (proposed > (max_cap / 2)) {
                proposed = max_cap;
                break;
            }
            proposed *= 2;
        }

        if (proposed < min_required) {
            throw std::length_error("custom_vector exceeds max_size");
        }

        return proposed;
    }

    void reallocate_to(size_type new_capacity) {
        assert(new_capacity >= size_);

        if constexpr (can_realloc_trivial_v) {
            data_ = reallocate_trivial_raw(data_, new_capacity);
            capacity_ = new_capacity;
            return;
        }

        pointer new_data = allocate_raw(new_capacity);
        try {
            relocate_range_to_uninitialized(data_, size_, new_data);
        }
        catch (...) {
            deallocate_raw(new_data, new_capacity);
            throw;
        }

        destroy_range(0, size_);
        deallocate_raw(data_, capacity_);

        data_ = new_data;
        capacity_ = new_capacity;
    }

    template <typename... Args>
    reference reallocate_emplace_back(Args&&... args) {
        const size_type new_capacity = growth_capacity_for(size_ + 1);

        if constexpr (can_realloc_trivial_v) {
            T staged(std::forward<Args>(args)...);
            data_ = reallocate_trivial_raw(data_, new_capacity);
            capacity_ = new_capacity;
            construct_at(size_, std::move(staged));
            ++size_;
            return data_[size_ - 1];
        }

        pointer new_data = allocate_raw(new_capacity);
        bool tail_constructed = false;

        try {
            // Construct the appended element first so args that reference old storage
            // are consumed before relocating existing elements.
            alloc_traits::construct(alloc_, new_data + size_, std::forward<Args>(args)...);
            tail_constructed = true;
            relocate_range_to_uninitialized(data_, size_, new_data);
        }
        catch (...) {
            if (tail_constructed) {
                destroy_raw_at(new_data + size_);
            }
            deallocate_raw(new_data, new_capacity);
            throw;
        }

        destroy_range(0, size_);
        deallocate_raw(data_, capacity_);

        data_ = new_data;
        capacity_ = new_capacity;
        ++size_;
        return data_[size_ - 1];
    }

    template <typename U>
        requires(std::is_assignable_v<T&, U&&> || std::is_constructible_v<T, U&&>)
    void reallocate_insert_at_index(size_type index, U&& value) {
        const size_type new_capacity = growth_capacity_for(size_ + 1);

        if constexpr (can_realloc_trivial_v) {
            T staged(std::forward<U>(value));
            data_ = reallocate_trivial_raw(data_, new_capacity);
            capacity_ = new_capacity;
            std::memmove(data_ + index + 1, data_ + index, (size_ - index) * sizeof(T));
            assign_or_reconstruct(index, std::move(staged));
            ++size_;
            return;
        }

        pointer new_data = allocate_raw(new_capacity);
        bool inserted_constructed = false;
        bool prefix_constructed = false;

        try {
            alloc_traits::construct(alloc_, new_data + index, std::forward<U>(value));
            inserted_constructed = true;

            if constexpr (is_trivial_move_block_v) {
                if (index > 0) {
                    std::memcpy(new_data, data_, index * sizeof(T));
                }
                if (size_ > index) {
                    std::memcpy(new_data + index + 1, data_ + index, (size_ - index) * sizeof(T));
                }
            }
            else {
                if (index > 0) {
                    relocate_range_to_uninitialized(data_, index, new_data);
                    prefix_constructed = true;
                }

                try {
                    relocate_range_to_uninitialized(data_ + index, size_ - index, new_data + index + 1);
                }
                catch (...) {
                    if (prefix_constructed) {
                        destroy_raw_range(new_data, index);
                    }
                    throw;
                }
            }
        }
        catch (...) {
            if (inserted_constructed) {
                destroy_raw_at(new_data + index);
            }
            deallocate_raw(new_data, new_capacity);
            throw;
        }

        destroy_range(0, size_);
        deallocate_raw(data_, capacity_);

        data_ = new_data;
        capacity_ = new_capacity;
        ++size_;
    }

    void grow_if_full() {
        if (size_ < capacity_) {
            return;
        }
        reallocate_to(growth_capacity_for(size_ + 1));
    }

    template <typename U>
        requires(std::is_assignable_v<T&, U&&> || std::is_constructible_v<T, U&&>)
    void assign_or_reconstruct(size_type idx, U&& value) {
        if constexpr (std::is_assignable_v<T&, U&&>) {
            data_[idx] = std::forward<U>(value);
        }
        else {
            destroy_at(idx);
            construct_at(idx, std::forward<U>(value));
        }
    }

    template <typename U>
        requires(std::is_assignable_v<T&, U&&> || std::is_constructible_v<T, U&&>)
    void insert_at_index(size_type index, U&& value) {
        if (index > size_) {
            throw std::out_of_range("insert index out of range");
        }

        if (index == size_) {
            emplace_back(std::forward<U>(value));
            return;
        }

        if (size_ == capacity_) {
            reallocate_insert_at_index(index, std::forward<U>(value));
            return;
        }

        if constexpr (is_trivial_move_block_v) {
            std::memmove(
                data_ + index + 1,
                data_ + index,
                (size_ - index) * sizeof(T)
            );
            assign_or_reconstruct(index, std::forward<U>(value));
            ++size_;
            return;
        }

        construct_at(size_, std::move(data_[size_ - 1]));

        for (size_type i = size_ - 1; i > index; --i) {
            data_[i] = std::move(data_[i - 1]);
        }

        assign_or_reconstruct(index, std::forward<U>(value));
        ++size_;
    }

    template <typename ForwardIt>
    void construct_from_forward_range(ForwardIt first, ForwardIt last) {
        const size_type count = static_cast<size_type>(std::distance(first, last));
        if (count == 0) {
            return;
        }

        reserve(count);
        size_type constructed = 0;
        try {
            if constexpr (std::is_pointer_v<ForwardIt> &&
                          std::is_same_v<std::remove_cv_t<std::remove_pointer_t<ForwardIt>>, T> &&
                          is_trivial_move_block_v) {
                std::memcpy(data_, first, count * sizeof(T));
                constructed = count;
            }
            else {
                for (; first != last; ++first, ++constructed) {
                    construct_at(constructed, *first);
                }
            }
        }
        catch (...) {
            destroy_range(0, constructed);
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();
            throw;
        }
        size_ = count;
    }

    template <typename InputIt>
    void construct_from_input_range(InputIt first, InputIt last) {
        for (; first != last; ++first) {
            emplace_back(*first);
        }
    }

    template <typename ForwardIt>
    void assign_from_forward_range(ForwardIt first, ForwardIt last) {
        const size_type count = static_cast<size_type>(std::distance(first, last));

        if (count > capacity_) {
            custom_vector staged(first, last, alloc_);
            swap(staged);
            return;
        }

        size_type i = 0;
        for (; i < size_ && first != last; ++i, ++first) {
            assign_or_reconstruct(i, *first);
        }

        if (i < count) {
            size_type constructed = i;
            try {
                for (; first != last; ++first, ++constructed) {
                    construct_at(constructed, *first);
                }
            }
            catch (...) {
                destroy_range(i, constructed);
                throw;
            }
        }
        else {
            destroy_range(count, size_);
        }

        size_ = count;
    }

    template <typename InputIt>
    void assign_from_input_range(InputIt first, InputIt last) {
        clear();
        for (; first != last; ++first) {
            emplace_back(*first);
        }
    }

    template <typename ForwardIt>
    void reallocate_insert_range_at_index(size_type index, ForwardIt first, ForwardIt last, size_type count) {
        const size_type new_capacity = growth_capacity_for(size_ + count);

        if constexpr (can_realloc_trivial_v) {
            data_ = reallocate_trivial_raw(data_, new_capacity);
            capacity_ = new_capacity;

            std::memmove(
                data_ + index + count,
                data_ + index,
                (size_ - index) * sizeof(T)
            );

            size_type written = 0;
            for (; first != last; ++first, ++written) {
                assign_or_reconstruct(index + written, *first);
            }

            size_ += count;
            return;
        }

        pointer new_data = allocate_raw(new_capacity);
        bool prefix_constructed = false;
        bool suffix_constructed = false;
        size_type inserted_constructed = 0;

        try {
            if (index > 0) {
                relocate_range_to_uninitialized(data_, index, new_data);
                prefix_constructed = true;
            }

            for (; first != last; ++first, ++inserted_constructed) {
                alloc_traits::construct(alloc_, new_data + index + inserted_constructed, *first);
            }

            if (size_ > index) {
                relocate_range_to_uninitialized(data_ + index, size_ - index, new_data + index + count);
                suffix_constructed = true;
            }
        }
        catch (...) {
            if (suffix_constructed) {
                destroy_raw_range(new_data + index + count, size_ - index);
            }
            if (inserted_constructed > 0) {
                destroy_raw_range(new_data + index, inserted_constructed);
            }
            if (prefix_constructed) {
                destroy_raw_range(new_data, index);
            }
            deallocate_raw(new_data, new_capacity);
            throw;
        }

        destroy_range(0, size_);
        deallocate_raw(data_, capacity_);

        data_ = new_data;
        capacity_ = new_capacity;
        size_ += count;
    }

    template <typename ForwardIt>
    void insert_forward_range_at_index(size_type index, ForwardIt first, ForwardIt last) {
        const size_type count = static_cast<size_type>(std::distance(first, last));
        if (count == 0) {
            return;
        }

        if (size_ + count > capacity_) {
            reallocate_insert_range_at_index(index, first, last, count);
            return;
        }

        if constexpr (is_trivial_move_block_v) {
            std::memmove(
                data_ + index + count,
                data_ + index,
                (size_ - index) * sizeof(T)
            );

            size_type written = 0;
            for (; first != last; ++first, ++written) {
                assign_or_reconstruct(index + written, *first);
            }

            size_ += count;
            return;
        }

        custom_vector staged(first, last, alloc_);
        const size_type old_size = size_;
        const size_type tail = old_size - index;

        if (count <= tail) {
            size_type moved_to_end = 0;
            try {
                for (; moved_to_end < count; ++moved_to_end) {
                    construct_at(old_size + moved_to_end, std::move(data_[old_size - count + moved_to_end]));
                }
            }
            catch (...) {
                destroy_range(old_size, old_size + moved_to_end);
                throw;
            }

            size_ = old_size + count;
            std::move_backward(data_ + index, data_ + (old_size - count), data_ + old_size);

            for (size_type i = 0; i < count; ++i) {
                assign_or_reconstruct(index + i, std::move(staged[i]));
            }
            return;
        }

        size_type moved_tail = 0;
        try {
            for (; moved_tail < tail; ++moved_tail) {
                construct_at(index + count + moved_tail, std::move(data_[index + moved_tail]));
            }
        }
        catch (...) {
            destroy_range(index + count, index + count + moved_tail);
            throw;
        }

        size_type constructed_extra = 0;
        try {
            for (; constructed_extra < (count - tail); ++constructed_extra) {
                construct_at(old_size + constructed_extra, std::move(staged[tail + constructed_extra]));
            }
        }
        catch (...) {
            destroy_range(index + count, index + count + moved_tail);
            destroy_range(old_size, old_size + constructed_extra);
            throw;
        }

        size_ = old_size + count;
        for (size_type i = 0; i < tail; ++i) {
            assign_or_reconstruct(index + i, std::move(staged[i]));
        }
    }

public:
    custom_vector() : alloc_() {
        reset_to_sbo_empty();
    }

    explicit custom_vector(const allocator_type& alloc) noexcept
        : alloc_(alloc) {
        reset_to_sbo_empty();
    }

    template <typename InputIt>
        requires(!std::is_integral_v<InputIt>)
    custom_vector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type())
        : alloc_(alloc) {
        reset_to_sbo_empty();

        using iterator_category = typename std::iterator_traits<InputIt>::iterator_category;
        if constexpr (std::is_base_of_v<std::forward_iterator_tag, iterator_category>) {
            construct_from_forward_range(first, last);
        }
        else {
            construct_from_input_range(first, last);
        }
    }

    explicit custom_vector(size_type count, const allocator_type& alloc = allocator_type())
        : alloc_(alloc) {
        reset_to_sbo_empty();
        if (count == 0) {
            return;
        }

        reserve(count);
        size_type constructed = 0;
        try {
            for (; constructed < count; ++constructed) {
                construct_at(constructed);
            }
        }
        catch (...) {
            destroy_range(0, constructed);
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();
            throw;
        }
        size_ = count;
    }

    custom_vector(size_type count, const T& value, const allocator_type& alloc = allocator_type())
        : alloc_(alloc) {
        reset_to_sbo_empty();
        if (count == 0) {
            return;
        }

        reserve(count);
        size_type constructed = 0;
        try {
            for (; constructed < count; ++constructed) {
                construct_at(constructed, value);
            }
        }
        catch (...) {
            destroy_range(0, constructed);
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();
            throw;
        }
        size_ = count;
    }

    custom_vector(std::initializer_list<T> init, const allocator_type& alloc = allocator_type())
        : alloc_(alloc) {
        reset_to_sbo_empty();
        if (init.size() == 0) {
            return;
        }

        reserve(init.size());
        size_type constructed = 0;
        try {
            for (const T& item : init) {
                construct_at(constructed, item);
                ++constructed;
            }
        }
        catch (...) {
            destroy_range(0, constructed);
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();
            throw;
        }
        size_ = init.size();
    }

    custom_vector(const custom_vector& other)
        : alloc_(alloc_traits::select_on_container_copy_construction(other.alloc_)) {
        reset_to_sbo_empty();
        if (other.size_ == 0) {
            return;
        }

        reserve(other.size_);

        if constexpr (is_trivial_move_block_v) {
            std::memcpy(data_, other.data_, other.size_ * sizeof(T));
            size_ = other.size_;
            return;
        }

        size_type constructed = 0;
        try {
            for (; constructed < other.size_; ++constructed) {
                alloc_traits::construct(alloc_, data_ + constructed, other.data_[constructed]);
            }
        }
        catch (...) {
            destroy_range(0, constructed);
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();
            throw;
        }
        size_ = other.size_;
    }

    custom_vector(custom_vector&& other)
        : alloc_(std::move(other.alloc_)) {
        reset_to_sbo_empty();

        if (!other.using_sbo()) {
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.reset_to_sbo_empty();
            return;
        }

        if (other.size_ == 0) {
            return;
        }

        reserve(other.size_);
        size_type constructed = 0;
        try {
            for (; constructed < other.size_; ++constructed) {
                alloc_traits::construct(alloc_, data_ + constructed, std::move(other.data_[constructed]));
            }
        }
        catch (...) {
            destroy_range(0, constructed);
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();
            throw;
        }

        size_ = other.size_;
        other.clear();
    }

    ~custom_vector() {
        clear();
        deallocate_raw(data_, capacity_);
        data_ = nullptr;
        capacity_ = 0;
    }

    custom_vector& operator=(const custom_vector& other) {
        if (this == &other) {
            return *this;
        }

        if constexpr (alloc_traits::propagate_on_container_copy_assignment::value) {
            if (alloc_ != other.alloc_) {
                clear();
                deallocate_raw(data_, capacity_);
                reset_to_sbo_empty();
            }
            alloc_ = other.alloc_;
        }

        if (other.size_ > capacity_) {
            const size_type target_capacity = next_power_of_two(other.size_);
            pointer new_data = allocate_raw(target_capacity);

            try {
                if constexpr (is_trivial_move_block_v) {
                    std::memcpy(new_data, other.data_, other.size_ * sizeof(T));
                }
                else {
                    std::uninitialized_copy_n(other.data_, other.size_, new_data);
                }
            }
            catch (...) {
                deallocate_raw(new_data, target_capacity);
                throw;
            }

            clear();
            deallocate_raw(data_, capacity_);
            data_ = new_data;
            capacity_ = target_capacity;
            size_ = other.size_;
            return *this;
        }

        const size_type overlap = std::min(size_, other.size_);
        for (size_type i = 0; i < overlap; ++i) {
            data_[i] = other.data_[i];
        }

        if (other.size_ > size_) {
            size_type constructed = size_;
            try {
                for (; constructed < other.size_; ++constructed) {
                    alloc_traits::construct(alloc_, data_ + constructed, other.data_[constructed]);
                }
            }
            catch (...) {
                destroy_range(size_, constructed);
                throw;
            }
        }
        else {
            destroy_range(other.size_, size_);
        }

        size_ = other.size_;
        return *this;
    }

    custom_vector& operator=(custom_vector&& other) {
        if (this == &other) {
            return *this;
        }

        if constexpr (alloc_traits::propagate_on_container_move_assignment::value) {
            clear();
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();
            alloc_ = std::move(other.alloc_);

            if (!other.using_sbo()) {
                data_ = other.data_;
                size_ = other.size_;
                capacity_ = other.capacity_;
                other.reset_to_sbo_empty();
                return *this;
            }
        }

        if constexpr (alloc_traits::is_always_equal::value) {
            clear();
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();

            if (!other.using_sbo()) {
                data_ = other.data_;
                size_ = other.size_;
                capacity_ = other.capacity_;
                other.reset_to_sbo_empty();
                return *this;
            }
        }

        if (alloc_ == other.alloc_ && !other.using_sbo()) {
            clear();
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();

            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;

            other.reset_to_sbo_empty();
            return *this;
        }

        clear();
        reserve(other.size_);
        for (size_type i = 0; i < other.size_; ++i) {
            emplace_back(std::move(other.data_[i]));
        }
        other.clear();
        return *this;
    }

    custom_vector& operator=(std::initializer_list<T> init) {
        clear();
        reserve(init.size());
        for (const T& item : init) {
            emplace_back(item);
        }
        return *this;
    }

    void assign(size_type count, const T& value) {
        if (count > capacity_) {
            custom_vector staged(count, value, alloc_);
            swap(staged);
            return;
        }

        const size_type overlap = std::min(size_, count);
        for (size_type i = 0; i < overlap; ++i) {
            assign_or_reconstruct(i, value);
        }

        if (count > size_) {
            size_type constructed = size_;
            try {
                for (; constructed < count; ++constructed) {
                    construct_at(constructed, value);
                }
            }
            catch (...) {
                destroy_range(size_, constructed);
                throw;
            }
        }
        else {
            destroy_range(count, size_);
        }

        size_ = count;
    }

    template <typename InputIt>
        requires(!std::is_integral_v<InputIt>)
    void assign(InputIt first, InputIt last) {
        using iterator_category = typename std::iterator_traits<InputIt>::iterator_category;
        if constexpr (std::is_base_of_v<std::forward_iterator_tag, iterator_category>) {
            assign_from_forward_range(first, last);
        }
        else {
            assign_from_input_range(first, last);
        }
    }

    void assign(std::initializer_list<T> init) {
        assign(init.begin(), init.end());
    }

    allocator_type get_allocator() const noexcept {
        return alloc_;
    }

    reference operator[](size_type index) noexcept {
        return data_[index];
    }

    const_reference operator[](size_type index) const noexcept {
        return data_[index];
    }

    reference at(size_type index) {
        if (index >= size_) {
            throw std::out_of_range("custom_vector::at index out of range");
        }
        return data_[index];
    }

    const_reference at(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("custom_vector::at index out of range");
        }
        return data_[index];
    }

    reference front() {
        if (empty()) {
            throw std::out_of_range("custom_vector::front on empty vector");
        }
        return data_[0];
    }

    const_reference front() const {
        if (empty()) {
            throw std::out_of_range("custom_vector::front on empty vector");
        }
        return data_[0];
    }

    reference back() {
        if (empty()) {
            throw std::out_of_range("custom_vector::back on empty vector");
        }
        return data_[size_ - 1];
    }

    const_reference back() const {
        if (empty()) {
            throw std::out_of_range("custom_vector::back on empty vector");
        }
        return data_[size_ - 1];
    }

    pointer data() noexcept {
        return data_;
    }

    const_pointer data() const noexcept {
        return data_;
    }

    iterator begin() noexcept {
        return data_;
    }

    const_iterator begin() const noexcept {
        return data_;
    }

    const_iterator cbegin() const noexcept {
        return data_;
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator end() const noexcept {
        return data_ + size_;
    }

    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    reverse_iterator rbegin() noexcept {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end());
    }

    const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(cend());
    }

    reverse_iterator rend() noexcept {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(cbegin());
    }

    bool empty() const noexcept {
        return size_ == 0;
    }

    size_type size() const noexcept {
        return size_;
    }

    size_type capacity() const noexcept {
        return capacity_;
    }

    size_type max_size() const noexcept {
        return max_size_for_allocator(alloc_);
    }

    void reserve(size_type new_capacity) {
        if (new_capacity <= capacity_) {
            return;
        }
        reallocate_to(growth_capacity_for(new_capacity));
    }

    void shrink_to_fit() {
        if (size_ == capacity_) {
            return;
        }

        if (size_ == 0) {
            clear();
            deallocate_raw(data_, capacity_);
            reset_to_sbo_empty();
            return;
        }

        if constexpr (sbo_capacity > 0) {
            if (size_ <= sbo_capacity && !using_sbo()) {
                pointer old_data = data_;
                const size_type old_capacity = capacity_;
                pointer new_data = sbo_data();

                relocate_range_to_uninitialized(old_data, size_, new_data);
                destroy_raw_range(old_data, size_);
                deallocate_raw(old_data, old_capacity);

                data_ = new_data;
                capacity_ = sbo_capacity;
                return;
            }
        }

        reallocate_to(next_power_of_two(size_));
    }

    void clear() noexcept {
        destroy_range(0, size_);
        size_ = 0;
    }

    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            return reallocate_emplace_back(std::forward<Args>(args)...);
        }

        construct_at(size_, std::forward<Args>(args)...);
        ++size_;
        return data_[size_ - 1];
    }

    void pop_back() {
        if (empty()) {
            throw std::out_of_range("custom_vector::pop_back on empty vector");
        }

        --size_;
        destroy_at(size_);
    }

    void resize(size_type count) {
        if (count < size_) {
            destroy_range(count, size_);
            size_ = count;
            return;
        }

        if (count == size_) {
            return;
        }

        reserve(count);
        size_type constructed = size_;
        try {
            for (; constructed < count; ++constructed) {
                construct_at(constructed);
            }
        }
        catch (...) {
            destroy_range(size_, constructed);
            throw;
        }
        size_ = count;
    }

    void resize(size_type count, const value_type& value) {
        if (count < size_) {
            destroy_range(count, size_);
            size_ = count;
            return;
        }

        if (count == size_) {
            return;
        }

        reserve(count);
        size_type constructed = size_;
        try {
            for (; constructed < count; ++constructed) {
                construct_at(constructed, value);
            }
        }
        catch (...) {
            destroy_range(size_, constructed);
            throw;
        }
        size_ = count;
    }

    template <typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        if (pos < cbegin() || pos > cend()) {
            throw std::out_of_range("custom_vector::emplace position out of range");
        }

        const size_type index = static_cast<size_type>(pos - cbegin());
        if (index == size_) {
            emplace_back(std::forward<Args>(args)...);
            return begin() + static_cast<difference_type>(index);
        }

        T staged(std::forward<Args>(args)...);
        insert_at_index(index, std::move(staged));
        return begin() + static_cast<difference_type>(index);
    }

    iterator insert(const_iterator pos, const T& value) {
        if (pos < cbegin() || pos > cend()) {
            throw std::out_of_range("custom_vector::insert position out of range");
        }

        const size_type index = static_cast<size_type>(pos - cbegin());
        if (!references_live_element(value)) {
            insert_at_index(index, value);
            return begin() + static_cast<difference_type>(index);
        }

        T stable_value(value);
        insert_at_index(index, std::move(stable_value));
        return begin() + static_cast<difference_type>(index);
    }

    iterator insert(const_iterator pos, T&& value) {
        if (pos < cbegin() || pos > cend()) {
            throw std::out_of_range("custom_vector::insert position out of range");
        }

        const size_type index = static_cast<size_type>(pos - cbegin());
        if (!references_live_element(value)) {
            insert_at_index(index, std::move(value));
            return begin() + static_cast<difference_type>(index);
        }

        T stable_value(std::move(value));
        insert_at_index(index, std::move(stable_value));
        return begin() + static_cast<difference_type>(index);
    }

    template <typename InputIt>
        requires(!std::is_integral_v<InputIt>)
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        if (pos < cbegin() || pos > cend()) {
            throw std::out_of_range("custom_vector::insert position out of range");
        }

        const size_type index = static_cast<size_type>(pos - cbegin());
        using iterator_category = typename std::iterator_traits<InputIt>::iterator_category;

        if constexpr (std::is_base_of_v<std::forward_iterator_tag, iterator_category>) {
            insert_forward_range_at_index(index, first, last);
        }
        else {
            custom_vector staged(alloc_);
            for (; first != last; ++first) {
                staged.emplace_back(*first);
            }
            insert_forward_range_at_index(index, staged.begin(), staged.end());
        }

        return begin() + static_cast<difference_type>(index);
    }

    iterator erase(const_iterator pos) {
        if (pos < cbegin() || pos >= cend()) {
            throw std::out_of_range("custom_vector::erase position out of range");
        }

        const size_type index = static_cast<size_type>(pos - cbegin());
        if constexpr (is_trivial_move_block_v) {
            std::memmove(
                data_ + index,
                data_ + index + 1,
                (size_ - index - 1) * sizeof(T)
            );
            --size_;
            destroy_at(size_);
            return begin() + static_cast<difference_type>(index);
        }

        for (size_type i = index; i + 1 < size_; ++i) {
            data_[i] = std::move(data_[i + 1]);
        }

        --size_;
        destroy_at(size_);
        return begin() + static_cast<difference_type>(index);
    }

    iterator erase(const_iterator first, const_iterator last) {
        if (first < cbegin() || first > cend() || last < cbegin() || last > cend() || last < first) {
            throw std::out_of_range("custom_vector::erase range out of range");
        }

        if (first == last) {
            return begin() + static_cast<difference_type>(first - cbegin());
        }

        const size_type from = static_cast<size_type>(first - cbegin());
        const size_type to = static_cast<size_type>(last - cbegin());
        const size_type count = to - from;
        const size_type tail = size_ - to;

        if constexpr (is_trivial_move_block_v) {
            if (tail > 0) {
                std::memmove(data_ + from, data_ + to, tail * sizeof(T));
            }
            size_ -= count;
            return begin() + static_cast<difference_type>(from);
        }

        for (size_type i = 0; i < tail; ++i) {
            data_[from + i] = std::move(data_[to + i]);
        }

        const size_type old_size = size_;
        size_ -= count;
        destroy_range(size_, old_size);
        return begin() + static_cast<difference_type>(from);
    }

    template <typename Pred>
    size_type erase_if(Pred&& pred) {
        pointer first = data_;
        pointer last = data_ + size_;

        while (first != last && !pred(*first)) {
            ++first;
        }

        if (first == last) {
            return 0;
        }

        pointer write = first;

        if constexpr (is_trivial_move_block_v) {
            for (pointer read = first + 1; read != last; ++read) {
                if (!pred(*read)) {
                    *write++ = *read;
                }
            }
        }
        else {
            for (pointer read = first + 1; read != last; ++read) {
                if (!pred(*read)) {
                    *write++ = std::move(*read);
                }
            }
        }

        const size_type removed = static_cast<size_type>(last - write);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (pointer p = write; p != last; ++p) {
                alloc_traits::destroy(alloc_, p);
            }
        }
        size_ -= removed;
        return removed;
    }

    void swap(custom_vector& other) {
        if (this == &other) {
            return;
        }

        if (!using_sbo() && !other.using_sbo()) {
            if constexpr (alloc_traits::propagate_on_container_swap::value) {
                using std::swap;
                swap(alloc_, other.alloc_);
            }

            using std::swap;
            swap(data_, other.data_);
            swap(size_, other.size_);
            swap(capacity_, other.capacity_);
            return;
        }

        if constexpr (alloc_traits::propagate_on_container_swap::value) {
            using std::swap;
            swap(alloc_, other.alloc_);
        }

        custom_vector tmp(std::move(*this));
        *this = std::move(other);
        other = std::move(tmp);
    }

    bool operator==(const custom_vector& other) const {
        return size_ == other.size_ && std::equal(begin(), end(), other.begin());
    }

    bool operator!=(const custom_vector& other) const {
        return !(*this == other);
    }

    std::weak_ordering operator<=>(const custom_vector& other) const {
        const size_type common_size = std::min(size_, other.size_);

        for (size_type i = 0; i < common_size; ++i) {
            if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_pointer_v<T>) {
                const auto cmp = data_[i] <=> other.data_[i];
                if (cmp < 0) {
                    return std::weak_ordering::less;
                }
                if (cmp > 0) {
                    return std::weak_ordering::greater;
                }
            }
            else {
                if (data_[i] < other.data_[i]) {
                    return std::weak_ordering::less;
                }
                if (other.data_[i] < data_[i]) {
                    return std::weak_ordering::greater;
                }
            }
        }

        if (size_ < other.size_) {
            return std::weak_ordering::less;
        }
        if (other.size_ < size_) {
            return std::weak_ordering::greater;
        }

        return std::weak_ordering::equivalent;
    }
};

template <typename T, typename Allocator>
void swap(custom_vector<T, Allocator>& lhs, custom_vector<T, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

template <typename T, typename Allocator, bool EnableSBO, typename U>
typename custom_vector<T, Allocator, EnableSBO>::size_type erase(
    custom_vector<T, Allocator, EnableSBO>& c,
    const U& value
) {
    return c.erase_if([&value](const T& element) { return element == value; });
}

template <typename T, typename Allocator, bool EnableSBO, typename Pred>
typename custom_vector<T, Allocator, EnableSBO>::size_type erase_if(
    custom_vector<T, Allocator, EnableSBO>& c,
    Pred&& pred
) {
    return c.erase_if(std::forward<Pred>(pred));
}

template <typename T, typename Allocator = std::allocator<T>, bool EnableSBO = true>
using CustomVector = custom_vector<T, Allocator, EnableSBO>;
