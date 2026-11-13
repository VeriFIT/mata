/** @file
 * @brief Backends of @c mata::utils::delta_allocate(): @c std and @c pool, chosen once per process.
 */

#include "mata/utils/delta-allocator.hh"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

namespace mata::utils {

namespace {

/// Thread-local bump region with one LIFO free list per 16-byte size class. Blocks of every size are carved from
///  the same region in allocation order, so a Delta built state by state lies in consecutive memory. Chunks are never
///  returned: a block freed by another thread just joins that thread's lists.
struct DeltaPool {
	static constexpr std::size_t granule{16};
	static constexpr std::size_t max_small{4'096};
	static constexpr std::size_t num_classes{max_small / granule};
	static constexpr std::size_t chunk_bytes{std::size_t{1} << 20};

	struct FreeBlock {
		FreeBlock* next;
	};

	std::byte* bump{nullptr};
	std::byte* end{nullptr};
	std::array<FreeBlock*, num_classes> free_lists{};

	static std::size_t class_of(const std::size_t bytes) noexcept { return (bytes - 1) / granule; }

	void* allocate(const std::size_t bytes) {
		if (bytes > max_small) { return ::operator new(bytes); }
		const std::size_t cls{class_of(bytes)};
		if (FreeBlock* const block{free_lists[cls]}; block != nullptr) {
			free_lists[cls] = block->next;
			return block;
		}
		const std::size_t size{(cls + 1) * granule};
		if (static_cast<std::size_t>(end - bump) < size) {
			bump = static_cast<std::byte*>(::operator new(chunk_bytes));
			end = bump + chunk_bytes;
		}
		void* const block{bump};
		bump += size;
		return block;
	}

	void deallocate(void* const ptr, const std::size_t bytes) noexcept {
		if (bytes > max_small) {
			::operator delete(ptr, bytes);
			return;
		}
		const std::size_t cls{class_of(bytes)};
		auto* const block{static_cast<FreeBlock*>(ptr)};
		block->next = free_lists[cls];
		free_lists[cls] = block;
	}
};

thread_local DeltaPool delta_pool{};

enum class DeltaBackend : std::uint8_t { Std, Pool };

DeltaBackend select_backend() {
	// Mata never modifies the environment, so reading it cannot race with a setenv() of its own.
	// NOLINTNEXTLINE(concurrency-mt-unsafe)
	const char* const value{std::getenv("MATA_DELTA_ALLOCATOR")};
	if (value == nullptr || *value == '\0') {
#if defined(MATA_DELTA_ALLOCATOR_DEFAULT_POOL)
		return DeltaBackend::Pool;
#else
		return DeltaBackend::Std;
#endif
	}
	const std::string_view name{value};
	if (name == "std") { return DeltaBackend::Std; }
	if (name == "pool") { return DeltaBackend::Pool; }
	throw std::invalid_argument{"MATA_DELTA_ALLOCATOR must be 'std' or 'pool'; got '" + std::string{name} + "'"};
}

/// Read once, so every block of the process is released by the backend that allocated it.
DeltaBackend backend() {
	static const DeltaBackend selected{select_backend()};
	return selected;
}

} // namespace

void* delta_allocate(const std::size_t bytes) {
	if (backend() == DeltaBackend::Pool) { return delta_pool.allocate(bytes); }
	return ::operator new(bytes);
}

void delta_deallocate(void* const ptr, const std::size_t bytes) noexcept {
	// A block exists only after `delta_allocate()` succeeded, so the selection is already made and cannot throw.
	if (backend() == DeltaBackend::Pool) {
		delta_pool.deallocate(ptr, bytes);
		return;
	}
	::operator delete(ptr, bytes);
}

const char* delta_allocator_name() { return backend() == DeltaBackend::Pool ? "pool" : "std"; }

} // namespace mata::utils.
