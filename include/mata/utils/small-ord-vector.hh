/** @file
 * @brief Ordered set of trivially copyable keys that stores small sets inline and spills larger ones to the heap.
 */

#ifndef MATA_UTILS_SMALL_ORD_VECTOR_HH
#define MATA_UTILS_SMALL_ORD_VECTOR_HH

#include "mata/utils/assert.hh"
#include "mata/utils/ord-vector.hh"

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ostream>
#include <span>
#include <type_traits>
#include <utility>

namespace mata::utils {

/**
 * @brief A set of keys kept as a sorted, duplicate-free contiguous array, like @c OrdVector, that stores up to
 *  @p InlineCapacity keys inside the object and allocates only for larger sets.
 *
 * The object is a pointer-sized buffer plus a 32-bit size word: the buffer holds either the inline keys or a pointer
 *  to a heap block, which keeps its capacity in a header in front of the keys. A set lives inline until it
 *  outgrows @p InlineCapacity or a larger capacity is reserved. Like @c std::vector, it then keeps its heap block
 *  when it shrinks, while copies are made in their final form: inline when they fit, otherwise with exact capacity.
 * On 64-bit targets the object ends with four bytes of tail padding, so a @c [[no_unique_address]] member followed
 *  by a 32-bit field packs both into 16 bytes.
 *
 * Iterators are plain pointers; any operation that grows the set may invalidate them.
 */
template <class Key, std::size_t InlineCapacity = std::max<std::size_t>(1, sizeof(std::uintptr_t) / sizeof(Key))>
class SmallOrdVector {
	static_assert(std::is_trivially_copyable_v<Key> && std::is_trivially_destructible_v<Key>);
	static_assert(InlineCapacity >= 1);

  public:
	using value_type = Key;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using reference = Key&;
	using const_reference = const Key&;
	using pointer = Key*;
	using const_pointer = const Key*;
	using iterator = Key*;
	using const_iterator = const Key*;

	static constexpr size_type inline_capacity{InlineCapacity};
	/// The size is stored in 31 bits.
	static constexpr size_type max_size{(size_type{1} << 31) - 1};

	SmallOrdVector() noexcept = default;
	explicit SmallOrdVector(const Key key) noexcept : buffer_{.inline_ = {key}}, size_{1U << 1} {}
	SmallOrdVector(std::initializer_list<Key> keys) : SmallOrdVector(keys.begin(), keys.end()) {}
	/// Construct from an unordered range of keys, which gets sorted and deduplicated.
	template <std::input_iterator InputIterator>
	SmallOrdVector(InputIterator first, InputIterator last) : SmallOrdVector{} {
		if constexpr (std::forward_iterator<InputIterator>) {
			reserve(static_cast<size_type>(std::distance(first, last)));
		}
		for (; first != last; ++first) { push_back(*first); }
		sort_and_rmdupl();
	}
	/// Copy sorted, duplicate-free keys, such as an @c OrdVector.
	explicit SmallOrdVector(const std::span<const Key> sorted_keys) : SmallOrdVector{} { assign_sorted(sorted_keys); }

	SmallOrdVector(const SmallOrdVector& other) : SmallOrdVector{} { assign_sorted(other.span()); }
	SmallOrdVector(SmallOrdVector&& other) noexcept : SmallOrdVector{} { steal(other); }
	SmallOrdVector& operator=(const SmallOrdVector& other) {
		if (this != &other) { assign_sorted(other.span()); }
		return *this;
	}
	SmallOrdVector& operator=(SmallOrdVector&& other) noexcept {
		if (this != &other) {
			release();
			steal(other);
		}
		return *this;
	}
	SmallOrdVector& operator=(const OrdVector<Key>& set) {
		assign_sorted(set);
		return *this;
	}
	~SmallOrdVector() { release(); }

	Key* data() noexcept { return on_heap() ? buffer_.block_ + header_keys : buffer_.inline_; }
	const Key* data() const noexcept { return on_heap() ? buffer_.block_ + header_keys : buffer_.inline_; }
	size_type size() const noexcept { return size_ >> 1; }
	bool empty() const noexcept { return size_ < 2; }
	size_type capacity() const noexcept { return on_heap() ? heap_capacity() : InlineCapacity; }
	/// Is the set stored inline, without a heap block?
	bool is_inline() const noexcept { return !on_heap(); }

	iterator begin() noexcept { return data(); }
	iterator end() noexcept { return data() + size(); }
	const_iterator begin() const noexcept { return data(); }
	const_iterator end() const noexcept { return data() + size(); }
	const_iterator cbegin() const noexcept { return begin(); }
	const_iterator cend() const noexcept { return end(); }
	std::span<const Key> span() const noexcept { return {data(), size()}; }

	const Key& operator[](const size_type index) const {
		MATA_ASSERT(index < size());
		return data()[index];
	}
	const Key& front() const {
		MATA_ASSERT(!empty());
		return data()[0];
	}
	const Key& back() const {
		MATA_ASSERT(!empty());
		return data()[size() - 1];
	}
	const Key& min() const { return front(); }
	const Key& max() const { return back(); }

	/// Ensure room for @p capacity keys; moves the set to the heap if @p capacity exceeds the inline capacity.
	void reserve(const size_type capacity) {
		if (capacity > this->capacity()) { reallocate(capacity); }
	}
	/// Remove all keys, keeping the heap block if there is one.
	/// Keep the heap flag; clear() keeps the heap block like std::vector.
	void clear() noexcept { size_ &= heap_flag; }

	const_iterator find(const Key key) const {
		MATA_ASSERT(is_sorted_unique());
		const Key* const first{data()};
		const Key* const last{first + size()};
		const Key* const it{std::lower_bound(first, last, key)};
		return (it != last && *it == key) ? it : last;
	}
	iterator find(const Key key) { return const_cast<iterator>(std::as_const(*this).find(key)); }
	bool contains(const Key key) const { return find(key) != end(); }
	size_type count(const Key key) const { return contains(key) ? 1 : 0; }

	/**
	 * @brief Insert @p key, keeping the set sorted.
	 * @return Iterator to @p key in the set and whether it was inserted.
	 */
	std::pair<iterator, bool> insert(const Key key) {
		MATA_ASSERT(is_sorted_unique());
		const size_type n{size()};
		if (n == 0 || data()[n - 1] < key) {
			push_back(key);
			return {end() - 1, true};
		}
		Key* pos{std::lower_bound(data(), data() + n, key)};
		if (*pos == key) { return {pos, false}; }
		const auto index{static_cast<size_type>(pos - data())};
		if (n == capacity()) { reallocate(grown_capacity(n + 1)); }
		Key* const keys{data()};
		std::memmove(keys + index + 1, keys + index, (n - index) * sizeof(Key));
		keys[index] = key;
		set_size(n + 1);
		return {keys + index, true};
	}

	/**
	 * @brief Insert all of the sorted, duplicate-free @p keys.
	 * @return True if any key was inserted.
	 */
	bool insert(const std::span<const Key> keys) {
		MATA_ASSERT(is_sorted_unique());
		if (keys.empty() || keys.data() == data()) { return false; }
		const size_type n{size()};
		if (n == 0 || data()[n - 1] < keys.front()) {
			append(keys);
			return true;
		}
		// Count the union, then merge back to front in place: no temporary and at most one reallocation.
		const Key* lhs{data()};
		const Key* const lhs_end{lhs + n};
		const Key* rhs{keys.data()};
		const Key* const rhs_end{rhs + keys.size()};
		size_type union_size{n};
		while (rhs != rhs_end) {
			lhs = std::lower_bound(lhs, lhs_end, *rhs);
			if (lhs == lhs_end) {
				union_size += static_cast<size_type>(rhs_end - rhs);
				break;
			}
			if (*lhs != *rhs) { ++union_size; }
			++rhs;
		}
		if (union_size == n) { return false; }
		reserve(union_size);
		Key* const out{data()};
		auto i{static_cast<std::ptrdiff_t>(n) - 1};
		auto j{static_cast<std::ptrdiff_t>(keys.size()) - 1};
		auto w{static_cast<std::ptrdiff_t>(union_size) - 1};
		while (j >= 0) {
			if (i >= 0 && out[i] > keys[static_cast<size_type>(j)]) {
				out[w--] = out[i--];
			} else {
				if (i >= 0 && out[i] == keys[static_cast<size_type>(j)]) { --i; }
				out[w--] = keys[static_cast<size_type>(j--)];
			}
		}
		set_size(union_size);
		return true;
	}

	/// Append @p key without keeping the order; restore it with @c sort_and_rmdupl().
	void push_back(const Key key) {
		const size_type n{size()};
		if (n == capacity()) { reallocate(grown_capacity(n + 1)); }
		data()[n] = key;
		set_size(n + 1);
	}
	template <typename... Args> Key& emplace_back(Args&&... args) {
		push_back(Key(std::forward<Args>(args)...));
		return data()[size() - 1];
	}

	/// Sort the keys and drop duplicates, restoring the invariant after unordered @c push_back().
	void sort_and_rmdupl() {
		Key* const first{data()};
		Key* last{first + size()};
		if (!std::is_sorted(first, last)) { std::sort(first, last); }
		last = std::unique(first, last);
		set_size(static_cast<size_type>(last - first));
	}

	size_type erase(const Key key) {
		const const_iterator it{find(key)};
		if (it == end()) { return 0; }
		erase(it);
		return 1;
	}
	/// A template so that a literal @c 0 selects @c erase(Key) rather than converting to a null iterator.
	template <std::convertible_to<const_iterator> Iterator> iterator erase(const Iterator pos) {
		return erase(pos, pos + 1);
	}
	iterator erase(const const_iterator first, const const_iterator last) {
		Key* const keys{data()};
		const auto from{static_cast<size_type>(first - keys)};
		const auto to{static_cast<size_type>(last - keys)};
		std::memmove(keys + from, keys + to, (size() - to) * sizeof(Key));
		set_size(size() - (to - from));
		return keys + from;
	}
	template <class Pred> size_type erase_if(Pred&& should_erase) {
		Key* const first{data()};
		Key* const last{first + size()};
		Key* const new_last{std::remove_if(first, last, std::forward<Pred>(should_erase))};
		const auto erased{static_cast<size_type>(last - new_last)};
		set_size(static_cast<size_type>(new_last - first));
		return erased;
	}

	bool operator==(const SmallOrdVector& other) const { return std::ranges::equal(span(), other.span()); }
	friend bool operator==(const SmallOrdVector& lhs, const OrdVector<Key>& rhs) {
		return std::ranges::equal(lhs.span(), rhs);
	}
	auto operator<=>(const SmallOrdVector& other) const {
		return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
	}

	friend std::ostream& operator<<(std::ostream& os, const SmallOrdVector& set) {
		os << "{";
		for (const Key& key : set) { os << (&key == set.begin() ? " " : ", ") << key; }
		return os << "}";
	}

  private:
	/// Keys reserved at the start of a heap block to keep its capacity.
	static constexpr size_type header_keys{(sizeof(std::uint32_t) + sizeof(Key) - 1) / sizeof(Key)};

	/// The set lives on the heap when the lowest bit of @c size_ is set; the size sits in the upper bits of the
	///  same word, so a move is a copy of the buffer union plus this word.
	union Buffer {
		Key inline_[InlineCapacity];
		Key* block_; ///< Heap block: the capacity header, then the keys.
	};
	Buffer buffer_{};
	std::uint32_t size_{0};

	static constexpr std::uint32_t heap_flag{1};
	bool on_heap() const noexcept { return (size_ & heap_flag) != 0; }
	void mark_heap() noexcept { size_ |= heap_flag; }

	static Key* allocate(const size_type capacity) {
		MATA_ASSERT(capacity <= max_size);
		Key* const block{std::allocator<Key>{}.allocate(header_keys + capacity)};
		const auto stored_capacity{static_cast<std::uint32_t>(capacity)};
		std::memcpy(block, &stored_capacity, sizeof(stored_capacity));
		return block;
	}
	size_type heap_capacity() const noexcept {
		std::uint32_t capacity;
		std::memcpy(&capacity, buffer_.block_, sizeof(capacity));
		return capacity;
	}
	void release() noexcept {
		if (on_heap()) { std::allocator<Key>{}.deallocate(buffer_.block_, header_keys + heap_capacity()); }
	}
	size_type grown_capacity(const size_type needed) const noexcept { return std::max(needed, 2 * capacity()); }
	void reallocate(const size_type capacity) {
		Key* const block{allocate(capacity)};
		std::memcpy(block + header_keys, data(), size() * sizeof(Key));
		release();
		buffer_.block_ = block;
		mark_heap();
	}
	void steal(SmallOrdVector& other) noexcept {
		// `Buffer` is an implicit-lifetime type, so copying its object representation is well-defined regardless of
		//  which member is active, and it does not inspect bytes past the active representation.
		std::memcpy(&buffer_, &other.buffer_, sizeof(buffer_));
		size_ = other.size_;
		other.size_ = 0;
	}
	/// Replace the content with the sorted, duplicate-free @p keys, which must not alias this set.
	void assign_sorted(const std::span<const Key> keys) {
		MATA_ASSERT(std::ranges::adjacent_find(keys, std::ranges::greater_equal{}) == keys.end());
		const size_type n{keys.size()};
		if (n > capacity()) {
			Key* const block{allocate(n)};
			release();
			buffer_.block_ = block;
			mark_heap();
		}
		if (n != 0) { std::memcpy(data(), keys.data(), n * sizeof(Key)); }
		set_size(n);
	}
	void append(const std::span<const Key> keys) {
		const size_type n{size()};
		if (n + keys.size() > capacity()) { reallocate(grown_capacity(n + keys.size())); }
		std::memcpy(data() + n, keys.data(), keys.size() * sizeof(Key));
		set_size(n + keys.size());
	}
	bool is_sorted_unique() const {
		return std::adjacent_find(begin(), end(), [](const Key& a, const Key& b) { return !(a < b); }) == end();
	}
	void set_size(const size_type size) noexcept {
		MATA_ASSERT(size <= max_size);
		size_ = static_cast<std::uint32_t>(size << 1) | (size_ & heap_flag);
	}
}; // class SmallOrdVector.

} // namespace mata::utils.

#endif // MATA_UTILS_SMALL_ORD_VECTOR_HH
