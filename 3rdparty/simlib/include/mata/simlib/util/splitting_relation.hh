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
#include <vector>

#include <mata/simlib/util/simlib.hh>
#include <mata/simlib/util/caching_allocator.hh>

namespace Simlib
{
	namespace Util
	{
		class SplittingRelation;
	}
}

class Simlib::Util::SplittingRelation {

	struct Element {

		Element* up_;
		Element* down_;
		Element* left_;
		Element* right_;
		size_t col_;
		size_t row_;

		explicit Element(size_t row = 0, size_t col = 0) : up_(), down_(), left_(), right_(), col_(col),
			row_(row) {}

	};

	std::vector<std::pair<Element*, Element*>> columns_;
	std::vector<std::pair<Element*, Element*>> rows_;
	size_t size_;

	CachingAllocator<Element> allocator_;

	Element* col_begin(size_t col) {
		return reinterpret_cast<Element*>(
			reinterpret_cast<char*>(&this->columns_[col].first) - offsetof(Element, down_)
		);
	}

	Element* col_end(size_t col) {
		return reinterpret_cast<Element*>(
			reinterpret_cast<char*>(&this->columns_[col].second) - offsetof(Element, up_)
		);
	}

	Element* row_begin(size_t row) {
		return reinterpret_cast<Element*>(
			reinterpret_cast<char*>(&this->rows_[row].first) - offsetof(Element, right_)
		);
	}

	Element* row_end(size_t row) {
		return reinterpret_cast<Element*>(
			reinterpret_cast<char*>(&this->rows_[row].second) - offsetof(Element, left_)
		);
	}

	bool check_col(size_t i) const {

		assert(i < this->size_);

		auto tmp = this->columns_[i].first;

		while (tmp != this->columns_[i].second->down_) {

			assert(tmp->up_->down_ == tmp);
			assert(tmp->down_->up_ == tmp);
			assert(tmp->left_->right_ == tmp);
			assert(tmp->right_->left_ == tmp);

			if (tmp->col_ != i)
				return false;

			if (tmp->row_ >= this->size_)
				return false;

			tmp = tmp->down_;

		}

		return true;

	}

	bool check_row(size_t i) const {

		assert(i < this->size_);

		auto tmp = this->rows_[i].first;

		while (tmp != this->rows_[i].second->right_) {

			assert(tmp->up_->down_ == tmp);
			assert(tmp->down_->up_ == tmp);
			assert(tmp->left_->right_ == tmp);
			assert(tmp->right_->left_ == tmp);

			if (tmp->row_ != i)
				return false;

			if (tmp->col_ >= this->size_)
				return false;

			tmp = tmp->right_;

		}

		return true;

	}

public:

	GCC_DIAG_OFF(effc++)
	struct IteratorBase : public std::iterator<std::input_iterator_tag, size_t> {
	GCC_DIAG_ON(effc++)

		Element* el_;

		explicit IteratorBase(Element* el) : el_(el) {}
		~IteratorBase() = default;

		bool operator==(const IteratorBase& rhs) const { return this->el_ == rhs.el_; }
		bool operator!=(const IteratorBase& rhs) const { return this->el_ != rhs.el_; }

	};

	GCC_DIAG_OFF(effc++)
	struct ColIterator : public IteratorBase {
	GCC_DIAG_ON(effc++)

		explicit ColIterator(Element* el) : IteratorBase(el) {}

		ColIterator& operator++() {

			this->el_ = this->el_->down_;
			return *this;

		}

		ColIterator operator++(int) {

			return ++ColIterator(this->el_);

		}

		const size_t& operator*() const { return this->el_->row_; }

	};

	GCC_DIAG_OFF(effc++)
	struct RowIterator : public IteratorBase {
	GCC_DIAG_ON(effc++)

		explicit RowIterator(Element* el) : IteratorBase(el) {}

		RowIterator& operator++() {

			this->el_ = this->el_->right_;
			return *this;

		}

		RowIterator operator++(int) {

			return ++RowIterator(this->el_);

		}

		const size_t& operator*() const { return this->el_->col_; }

	};

	struct Column {

		Element*& begin_;
		Element* end_;

		Column(Element*& begin, Element* end) : begin_(begin), end_(end) {}

		ColIterator begin() const { return ColIterator(this->begin_); }
		ColIterator end() const { return ColIterator(this->end_); }

	};

	struct Row {

		Element*& begin_;
		Element* end_;

		Row(Element*& begin, Element* end) : begin_(begin), end_(end) {}

		RowIterator begin() const { return RowIterator(this->begin_); }
		RowIterator end() const { return RowIterator(this->end_); }

	};

public:

	explicit SplittingRelation(size_t maxSize) : columns_(maxSize), rows_(maxSize), size_(), allocator_() {}

	~SplittingRelation() {

		for (size_t i = 0; i < this->size_; ++i) {

			auto tmp = this->rows_[i].first;

			while (tmp != this->rows_[i].second->right_) {

				this->allocator_.reclaim(tmp);

				tmp = tmp->right_;

			}

		}

	}

	template <class Index>
	void init(const Index& index) {

		std::vector<Element*> lastV(index.size());

		for (size_t i = 0; i < index.size(); ++i)
			lastV[i] = this->col_begin(i);

		for (size_t i = 0; i < index.size(); ++i) {

			auto last = this->row_begin(i);

			Element* el;

			assert(index[i].size());

			for (auto& j: index[i]) {

				assert(j < index.size());

				el = new Element(i, j);
				el->up_ = lastV[j];
				el->left_ = last;

				lastV[j]->down_ = el;
				lastV[j] = el;

				last->right_ = el;
				last = el;

			}

			last->right_ = this->row_end(i);
			this->rows_[i].second = last; // last->right_->left_

		}

		this->size_ = index.size();

		for (size_t i = 0; i < index.size(); ++i) {

			lastV[i]->down_ = this->col_end(i);
			this->columns_[i].second = lastV[i]; // lastV[i]->down_->up_

			assert(this->check_col(i));
			assert(this->check_row(i));

		}

	}

	size_t split(size_t index) {

		assert(index < this->size_);

		size_t newIndex = this->size_;

		// copy column

		auto el = this->columns_[index].first;

		assert(el);

		auto last = this->col_begin(newIndex);

		while (el != this->col_end(index)) {

			auto tmp = this->allocator_();

			last->down_ = tmp;
			tmp->up_ = last;

			assert(el->row_ < this->size_);

			this->rows_[el->row_].second->right_ = tmp;
			tmp->left_ = this->rows_[el->row_].second;
			tmp->right_ = this->row_end(el->row_);
			this->rows_[el->row_].second = tmp; // tmp->right_->left_

			tmp->col_ = newIndex;
			tmp->row_ = el->row_;

			last = tmp;

			el = el->down_;

		}

		// put reflexivity

		el = this->allocator_();

		last->down_ = el;
		el->up_ = last;
		el->down_ = this->col_end(newIndex);
		this->columns_[newIndex].second = el; // el->down_->up_

		el->right_ = this->row_end(newIndex);
		el->col_ = newIndex;
		el->row_ = newIndex;

		// copy row

		el = this->rows_[index].first;

		assert(el);

		last = this->row_begin(newIndex);

		// we have to skip the last one here
		while (el != this->rows_[index].second) {

			auto tmp = this->allocator_();

			last->right_ = tmp;
			tmp->left_ = last;

			assert(el->col_ < this->size_);

			this->columns_[el->col_].second->down_ = tmp;
			tmp->up_ = this->columns_[el->col_].second;
			tmp->down_ = this->col_end(el->col_);
			this->columns_[el->col_].second = tmp; // tmp->down_->up_

			tmp->col_ = el->col_;
			tmp->row_ = newIndex;

			last = tmp;

			el = el->right_;

		}

		// finish reflexivity

		last->right_ = this->columns_[newIndex].second;
		this->columns_[newIndex].second->left_ = last;

		this->rows_[newIndex].second = this->columns_[newIndex].second;

		++this->size_;

		assert(this->check_col(newIndex));
		assert(this->check_row(newIndex));

		return newIndex;

	}

	Column column(size_t index) {

		assert(index < this->columns_.size());
		assert(this->check_col(index));

		return Column{this->columns_[index].first, this->col_end(index)};

	}

	Row row(size_t index) {

		assert(index < this->rows_.size());
		assert(this->check_row(index));

		return Row{this->rows_[index].first, this->row_end(index)};

	}

	void erase(IteratorBase& iter) {

		auto& el = iter.el_;

		el->up_->down_ = el->down_;
		el->down_->up_ = el->up_;
		el->left_->right_ = el->right_;
		el->right_->left_ = el->left_;

		this->allocator_.reclaim(el);

		assert(this->check_col(el->col_));
		assert(this->check_row(el->row_));

	}

	const size_t& size() const {

		return this->size_;

	}

};

#endif
