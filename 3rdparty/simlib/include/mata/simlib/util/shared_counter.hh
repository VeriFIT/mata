/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Jiri Simacek <isimacek@fit.vutbr.cz>
 *
 *  Description:
 *    Source for shared counter class.
 *
 *****************************************************************************/

#ifndef _SIMLIB_SHARED_COUNTER_HH_
#define _SIMLIB_SHARED_COUNTER_HH_

#include <cstring>
#include <vector>

#include <mata/simlib/util/caching_allocator.hh>

namespace Simlib { namespace Util {
	class SharedCounter;
}}

class Simlib::Util::SharedCounter
{
public:

	typedef CachingArrayAllocator<size_t> Allocator;
	typedef std::vector<size_t> Key;
	typedef std::vector<std::pair<size_t, size_t>> LabelMap;

private:

	struct Row {

		size_t master_;
		size_t* data_;

		Row() : master_(0), data_(nullptr) {}

	};

	const Key& key_;
	const size_t& states_;
	const LabelMap& labelMap_;
	const size_t& rowSize_;
	Allocator& allocator_;

	std::vector<Row> data_;

public:

	SharedCounter(const Key& key, const size_t& states, const LabelMap& labelMap,
		const size_t& rowSize, Allocator& allocator) : key_(key), states_(states),
		labelMap_(labelMap), rowSize_(rowSize), allocator_(allocator), data_() {}

	SharedCounter(SharedCounter& counter) : key_(counter.key_), states_(counter.states_),
		labelMap_(counter.labelMap_), rowSize_(counter.rowSize_), allocator_(counter.allocator_),
		data_() {}

	~SharedCounter() {

		for (auto& row : this->data_) {

			if (!row.data_)
				continue;

			if (!--(row.data_[this->rowSize_])) // refCount
				this->allocator_.reclaim(row.data_);

		}

	}

	void init() {

		for (auto& row : this->data_) {

			if (!row.data_)
				continue;

			if (row.data_[this->rowSize_] == 1)
				continue;

			// everything is in master
			this->allocator_.reclaim(row.data_);

			row.data_ = nullptr;

		}

	}

	size_t get(size_t label, size_t state) const {

		assert(label*this->states_ + state < this->key_.size());

		size_t index = this->key_[label*this->states_ + state];
		size_t rowIndex = index / this->rowSize_;
		size_t colIndex = index % this->rowSize_;

		assert(rowIndex < this->data_.size());

		auto& row = this->data_[rowIndex];

		if (!row.data_)
			return row.master_;

		return row.data_[colIndex];

	}

	void set(size_t label, size_t state, size_t count) {

		assert(count);
		assert(label*this->states_ + state < this->key_.size());

		size_t index = this->key_[label*this->states_ + state];
		size_t rowIndex = index / this->rowSize_;
		size_t colIndex = index % this->rowSize_;

		assert(rowIndex < this->data_.size());

		auto& row = this->data_[rowIndex];

		if (row.master_) {

			assert(row.data_);
			assert((row.data_[this->rowSize_] == 0) || (row.data_[this->rowSize_] == 1));

			row.master_ += count;
			row.data_[colIndex] = count;
			row.data_[this->rowSize_] = 1; // refCount

			return;

		}

		row.master_ = count;
		row.data_ = this->allocator_();

//		std::memset(row.data_, 0, this->row_size_*sizeof(size_t));

//		assert(row.data_[colIndex] == 0);

		row.data_[this->rowSize_] = 0; // exploit refCount
		row.data_[colIndex] = count;

	}

	size_t decr(size_t label, size_t state) {

		assert(label*this->states_ + state < this->key_.size());

		size_t index = this->key_[label*this->states_ + state];
		size_t rowIndex = index / this->rowSize_;
		size_t colIndex = index % this->rowSize_;

		assert(rowIndex < this->data_.size());

		auto& row = this->data_[rowIndex];

		assert(row.master_);

		if (!row.data_) // everything is in master
			return --row.master_;

		if ((row.master_ == row.data_[colIndex]) || (row.master_ == 2)) {

			// move everything to master

			--row.master_;

			size_t result = (row.data_[colIndex] - 1);

			if (!--(row.data_[this->rowSize_])) // refCount
				this->allocator_.reclaim(row.data_);

			row.data_ = nullptr;

			return result;

		}

		if (row.data_[this->rowSize_] > 1) { // refCount

			--(row.data_[this->rowSize_]); // refCount

			auto newData = this->allocator_();

			std::memcpy(newData, row.data_, this->rowSize_*sizeof(size_t));

			assert(newData[colIndex] == row.data_[colIndex]);

			newData[this->rowSize_] = 1; // refCount

			row.data_ = newData;

		}

		assert(row.data_[this->rowSize_] == 1);

		--row.master_;

		return --(row.data_[colIndex]);

	}

	void resize(size_t rowCount) {

		this->data_.resize(rowCount);

	}

	template <class T>
	void copy_labels(const T& labels, SharedCounter& cnt) {

		size_t sent = 0;

		std::vector<std::pair<size_t, size_t>> ranges;

		for (auto& label : labels) {

			assert(label < this->labelMap_.size());

			size_t end = std::min(cnt.data_.size(), this->labelMap_[label].second);

			if (end <= this->labelMap_[label].first)
				continue;

			ranges.push_back(std::make_pair(this->labelMap_[label].first, end));

			sent = std::max(sent, end);

		}

		this->data_.resize(sent);

		std::vector<bool> rowMask(sent, false);

		for (auto& range : ranges) {

			for (size_t i = range.first; i < range.second; ++i) {

				if (rowMask[i])
					continue;

				rowMask[i] = true;

				auto& src = cnt.data_[i];
				auto& dst = this->data_[i];

				dst.master_ = src.master_;

				if (!src.data_)
					continue;

				++(src.data_[this->rowSize_]); // refCount

				dst.data_ = src.data_;

			}

		}

	}

	friend std::ostream& operator<<(std::ostream& os, const SharedCounter& cnt) {

		for (auto& row : cnt.data_) {

			os << row.master_ << ':';

			if (row.data_) {

				for (size_t col = 0; col < cnt.rowSize_; ++col)
					os << ' ' << row.data_[col];

				os << std::endl;

			}

		}

		return os;

	}

};

#endif
