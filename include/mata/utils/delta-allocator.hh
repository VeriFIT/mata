/** @file
 * @brief Allocator of the heap blocks of @c Delta: the symbol posts of a state post and the spilled target sets.
 *
 * The backend is @c std (global @c operator @c new) or @c pool (thread-local bump chunks with size-class free lists).
 *  The environment variable @c MATA_DELTA_ALLOCATOR selects it; when it is unset or empty, the CMake option of the
 *  same name chose the default when libmata was built. The selection is read on the first allocation and fixed for
 *  the rest of the process, so every block is released by the backend that allocated it. The backend lives out of
 *  line, so header code inlined into a consumer and code inside libmata always pair the same allocation and release.
 */

#ifndef MATA_UTILS_DELTA_ALLOCATOR_HH
#define MATA_UTILS_DELTA_ALLOCATOR_HH

#include <cstddef>
#include <new>
#include <type_traits>

namespace mata::utils {

/// Allocate @p bytes (16-byte aligned) for a @c Delta block.
/// @throws std::invalid_argument when @c MATA_DELTA_ALLOCATOR is neither @c std nor @c pool on the first call.
void* delta_allocate(std::size_t bytes);
/// Release a block of @p bytes from @c delta_allocate().
void delta_deallocate(void* ptr, std::size_t bytes) noexcept;
/// Name of the selected backend: @c "std" or @c "pool". Selects it like @c delta_allocate() when not yet selected.
const char* delta_allocator_name();

/// Stateless allocator over @c delta_allocate(): all instances are interchangeable.
template <class T> struct DeltaAllocator {
	static_assert(alignof(T) <= 16);
	using value_type = T;
	using is_always_equal = std::true_type;

	DeltaAllocator() noexcept = default;
	// NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions): rebinding copies implicitly.
	template <class U> DeltaAllocator(const DeltaAllocator<U>& /*other*/) noexcept {}
	T* allocate(const std::size_t n) { return static_cast<T*>(delta_allocate(n * sizeof(T))); }
	void deallocate(T* const ptr, const std::size_t n) noexcept { delta_deallocate(ptr, n * sizeof(T)); }

	template <class U> bool operator==(const DeltaAllocator<U>& /*other*/) const noexcept { return true; }
};

} // namespace mata::utils.

#endif // MATA_UTILS_DELTA_ALLOCATOR_HH
