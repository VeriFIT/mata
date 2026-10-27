/*****************************************************************************
 *  Simlib
 *
 *  Description:
 *    Relation given by a partition and a relation on its blocks.
 *
 *****************************************************************************/

#ifndef _SIMLIB_PARTITION_RELATION_HH_
#define _SIMLIB_PARTITION_RELATION_HH_

// Standard library headers
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace Simlib
{
	namespace Util
	{
		class PartitionRelation;
	}
}

/**
 * @brief  A relation on elements 0 to @c size() - 1 stored as the partition-relation pair it is computed as
 *
 * Element @c i is related to @c j iff the block of @c i is related to the block of @c j. The memory is linear in the
 * number of elements plus the number of related block pairs, where Simlib::Util::BinaryRelation needs a bit for
 * every pair of elements.
 */
class Simlib::Util::PartitionRelation
{
	static constexpr size_t NO_BLOCK = std::numeric_limits<size_t>::max();

	std::vector<size_t> block_of_{};
	// Row b of the block relation is columns_[row_begin_[b] .. row_begin_[b + 1]), sorted.
	std::vector<size_t> row_begin_{0};
	std::vector<size_t> columns_{};

public:
	PartitionRelation() = default;

	/**
	 * @brief  Starts a relation on the elements of @p block_of; the rows of the blocks follow by @c add_row()
	 *
	 * @param[in]  block_of  The block of each element
	 */
	explicit PartitionRelation(std::vector<size_t> block_of) : block_of_(std::move(block_of)) {}

	/**
	 * @brief  Appends the row of the next block
	 *
	 * @param[in]  first  Begin of the blocks the next block is related to, in any order
	 * @param[in]  last   End of the blocks the next block is related to
	 */
	template <class Iterator>
	void add_row(Iterator first, Iterator last)
	{
		const auto row = this->columns_.insert(this->columns_.end(), first, last);
		std::sort(row, this->columns_.end());
		this->row_begin_.push_back(this->columns_.size());
	}

	size_t size() const { return this->block_of_.size(); }

	size_t blocks() const { return this->row_begin_.size() - 1; }

	size_t block_of(size_t i) const { return this->block_of_[i]; }

	/// Whether block @p i is related to block @p j.
	bool get_blocks(size_t i, size_t j) const
	{
		assert(i < this->blocks() && j < this->blocks());
		const size_t* const columns = this->columns_.data();
		return std::binary_search(columns + this->row_begin_[i], columns + this->row_begin_[i + 1], j);
	}

	/// Whether element @p i is related to element @p j.
	bool get(size_t i, size_t j) const
	{
		assert(i < this->size() && j < this->size());
		return this->get_blocks(this->block_of_[i], this->block_of_[j]);
	}

	/**
	 * @brief  Gets the projection of elements to the representatives of the classes of the symmetric fragment
	 *
	 * The same as Simlib::Util::BinaryRelation::get_symmetric_quotient_projection() of the relation on elements,
	 * for a preorder.
	 *
	 * @param[out]  quotProj  The vector mapping elements to their representatives in the quotient set, quotProj[i]
	 *                        is the least element equivalent to the i-th one
	 */
	void get_symmetric_quotient_projection(std::vector<size_t>& quotProj) const
	{
		std::vector<size_t> least(this->blocks(), NO_BLOCK);
		for (size_t i = this->size(); i-- > 0;) { least[this->block_of_[i]] = i; }

		// The least element of the class of each block: the classes are the blocks related both ways.
		std::vector<size_t> classLeast(this->blocks(), NO_BLOCK);
		for (size_t b = 0; b < this->blocks(); ++b)
		{
			for (size_t k = this->row_begin_[b]; k < this->row_begin_[b + 1]; ++k)
			{
				const size_t c = this->columns_[k];
				if (least[c] < classLeast[b] && this->get_blocks(c, b)) { classLeast[b] = least[c]; }
			}
		}

		quotProj.resize(this->size());
		for (size_t i = 0; i < this->size(); ++i)
		{
			assert(classLeast[this->block_of_[i]] <= i);
			quotProj[i] = classLeast[this->block_of_[i]];
		}
	}
};

#endif
