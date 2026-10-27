/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Jiri Simacek <isimacek@fit.vutbr.cz>
 *
 *  Description:
 *    Splitting relation class.
 *
 *****************************************************************************/

#ifndef _SIMLIB_SPLITTING_RELATION_HH_
#define _SIMLIB_SPLITTING_RELATION_HH_

// Standard library headers
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <vector>

#include <mata/simlib/util/simlib.hh>

namespace Simlib
{
	namespace Util
	{
		class SplittingRelation;
	}
}

/**
 * @brief The relation on the blocks of the simulation engine.
 *
 * Every row and every column is a list of 64-bit words, sorted by their position: a pair costs about 2 bits when the
 *  relation is dense and 16 bytes when it is sparse. Words are only ever appended, as @c split() always adds the
 *  largest index; @c erase() clears a bit and leaves its word in place.
 */
class Simlib::Util::SplittingRelation {

	using Word = std::uint64_t;
	static constexpr size_t WORD_BITS = 64;

	struct Chunk {

		size_t index_; // position of the word: it holds the elements index_*WORD_BITS + [0, WORD_BITS)
		Word bits_;

	};

	using Line = std::vector<Chunk>;

	std::vector<Line> columns_;
	std::vector<Line> rows_;
	std::vector<size_t> columnSizes_;
	std::vector<size_t> rowSizes_;
	size_t size_;

	/// Adds @p element to @p line; it is larger than every element of @p line.
	static void append(Line& line, size_t element) {

		const size_t index = element / WORD_BITS;
		const Word bit = Word{1} << (element % WORD_BITS);

		assert(line.empty() || line.back().index_ <= index);

		if (!line.empty() && line.back().index_ == index) {

			assert(!(line.back().bits_ & bit));
			line.back().bits_ |= bit;

		} else {

			line.push_back(Chunk{index, bit});

		}

	}

	/// Removes @p element, present in @p line.
	static void remove(Line& line, size_t element) {

		const size_t index = element / WORD_BITS;
		auto chunk = std::lower_bound(
			line.begin(), line.end(), index, [](const Chunk& chunk, size_t i) { return chunk.index_ < i; }
		);

		assert(chunk != line.end() && chunk->index_ == index);

		const Word bit = Word{1} << (element % WORD_BITS);

		assert(chunk->bits_ & bit);
		chunk->bits_ &= ~bit;

	}

	void set(size_t row, size_t col) {

		append(this->rows_[row], col);
		append(this->columns_[col], row);
		++this->rowSizes_[row];
		++this->columnSizes_[col];

	}

public:

	/// The elements of one line in increasing order; the line may lose elements while it is iterated.
	GCC_DIAG_OFF(effc++)
	struct IteratorBase {
	GCC_DIAG_ON(effc++)
		using iterator_category = std::input_iterator_tag;
		using value_type = size_t;
		using difference_type = std::ptrdiff_t;
		using pointer = const size_t*;
		using reference = const size_t&;

		const Line* line_;
		size_t pos_;
		Word rest_; // the bits of the current word not visited yet, the current one included
		size_t fixed_; // the row (column) of a row (column) iterator
		size_t value_;

		IteratorBase(const Line* line, size_t pos, size_t fixed) : line_(line), pos_(pos), rest_(), fixed_(fixed),
			value_() { this->skip_empty(); }

		void skip_empty() {

			while (this->pos_ < this->line_->size() && !(this->rest_ = (*this->line_)[this->pos_].bits_))
				++this->pos_;

			if (this->pos_ < this->line_->size())
				this->value_ = (*this->line_)[this->pos_].index_ * WORD_BITS +
					static_cast<size_t>(std::countr_zero(this->rest_));

		}

		void next() {

			this->rest_ &= this->rest_ - 1;

			if (this->rest_) {

				this->value_ = (*this->line_)[this->pos_].index_ * WORD_BITS +
					static_cast<size_t>(std::countr_zero(this->rest_));

				return;

			}

			++this->pos_;
			this->skip_empty();

		}

		bool operator==(const IteratorBase& rhs) const {
			return this->pos_ == rhs.pos_ && (this->pos_ == this->line_->size() || this->rest_ == rhs.rest_);
		}
		bool operator!=(const IteratorBase& rhs) const { return !(*this == rhs); }

		const size_t& operator*() const { return this->value_; }

	};

	/// Visits the rows related to one column.
	GCC_DIAG_OFF(effc++)
	struct ColIterator : public IteratorBase {
	GCC_DIAG_ON(effc++)

		using IteratorBase::IteratorBase;

		ColIterator& operator++() {

			this->next();
			return *this;

		}

	};

	/// Visits the columns related to one row.
	GCC_DIAG_OFF(effc++)
	struct RowIterator : public IteratorBase {
	GCC_DIAG_ON(effc++)

		using IteratorBase::IteratorBase;

		RowIterator& operator++() {

			this->next();
			return *this;

		}

	};

	struct Column {

		const Line* line_;
		size_t index_;

		ColIterator begin() const { return ColIterator(this->line_, 0, this->index_); }
		ColIterator end() const { return ColIterator(this->line_, this->line_->size(), this->index_); }

	};

	struct Row {

		const Line* line_;
		size_t index_;

		RowIterator begin() const { return RowIterator(this->line_, 0, this->index_); }
		RowIterator end() const { return RowIterator(this->line_, this->line_->size(), this->index_); }

	};

	explicit SplittingRelation(size_t maxSize) : columns_(maxSize), rows_(maxSize), columnSizes_(maxSize),
		rowSizes_(maxSize), size_() {}

	/// @p index lists the related columns of every row in increasing order.
	template <class Index>
	void init(const Index& index) {

		for (size_t i = 0; i < index.size(); ++i) {

			assert(index[i].size());

			for (auto& j: index[i]) {

				assert(j < index.size());
				this->set(i, j);

			}

		}

		this->size_ = index.size();

	}

	/// Adds the block @c size(), a copy of @p index: related to the rows and columns of @p index and to itself.
	size_t split(size_t index) {

		assert(index < this->size_);

		const size_t newIndex = this->size_;

		// Each step appends to lines it does not iterate, and every appended element is the largest of its line:
		//  the column of newIndex gets the rows of the column of index (index among them, which also relates index
		//  to newIndex), the row of newIndex the columns of the row of index (now including newIndex, skipped), and
		//  then newIndex itself.
		const Column column{&this->columns_[index], index};
		for (auto it = column.begin(); it != column.end(); ++it)
			this->set(*it, newIndex);

		const Row row{&this->rows_[index], index};
		for (auto it = row.begin(); it != row.end(); ++it) {

			if (*it != newIndex)
				this->set(newIndex, *it);

		}

		this->set(newIndex, newIndex);

		++this->size_;

		return newIndex;

	}

	Column column(size_t index) const {

		assert(index < this->size_);

		return Column{&this->columns_[index], index};

	}

	Row row(size_t index) const {

		assert(index < this->size_);

		return Row{&this->rows_[index], index};

	}

	size_t column_size(size_t index) const {

		return this->columnSizes_[index];

	}

	size_t row_size(size_t index) const {

		return this->rowSizes_[index];

	}

	/// Erases the pair at @p iter, which stays valid for its increment.
	void erase(const RowIterator& iter) {

		this->erase_at(iter, iter.fixed_, iter.value_);
		remove(this->columns_[iter.value_], iter.fixed_);

	}

	/// Erases the pair at @p iter, which stays valid for its increment.
	void erase(const ColIterator& iter) {

		this->erase_at(iter, iter.value_, iter.fixed_);
		remove(this->rows_[iter.value_], iter.fixed_);

	}

	const size_t& size() const {

		return this->size_;

	}

private:

	/// Clears the bit at @p iter in the line it iterates; the iterator keeps its own copy of the word.
	void erase_at(const IteratorBase& iter, size_t row, size_t col) {

		auto& line = const_cast<Line&>(*iter.line_);
		line[iter.pos_].bits_ &= ~(Word{1} << (iter.value_ % WORD_BITS));
		--this->rowSizes_[row];
		--this->columnSizes_[col];

	}

};

#endif
