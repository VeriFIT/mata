/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Jiri Simacek <isimacek@fit.vutbr.cz>
 *
 *  Description:
 *    Header file for smart set.
 *
 *****************************************************************************/

#ifndef _SIMLIB_SMART_SET_HH_
#define _SIMLIB_SMART_SET_HH_

#include <mata/simlib/util/convert.hh>
#include <mata/simlib/util/simlib.hh>

// Standard library headers
#include <algorithm>
#include <ostream>
#include <vector>

namespace Simlib
{
	namespace Util
	{
		class SmartSet;
	}
}

class Simlib::Util::SmartSet
{
public:

	typedef size_t Key;

private:

	struct Element
	{
		Element* next;
		Key key;
		size_t count;

		explicit Element(
			const Key&         key,
			size_t             count = 0) :
			next(nullptr),
			key(key),
			count(count)
		{ }
	};

	GCC_DIAG_OFF(effc++)
	class Iterator {
	GCC_DIAG_ON(effc++)

	private:

		const Element* element_;

	public:
        using iterator_category = std::input_iterator_tag;
        using value_type = int;
        using difference_type = int;
        using pointer = int*;
        using reference = int&;

        explicit Iterator(const Element* element) :
			element_(element)
		{ }

		Iterator& operator++()
		{
			assert(nullptr != element_);

			element_ = element_->next;
			return *this;
		}

		Iterator operator++(int) const
		{
			return ++Iterator(element_);
		}

		const Key& operator*()
		{
			assert(nullptr != element_);

			return element_->key;
		}

		bool operator==(const Iterator& rhs) const
		{
			return element_ == rhs.element_;
		}

		bool operator!=(const Iterator& rhs) const
		{
			return element_ != rhs.element_;
		}
	};

public:

	typedef Iterator iterator;

private:

	Element head_;
	Element* last_;
	size_t size_;

	std::vector<Element*> index_;

protected:

	size_t& insert(const Key& key)
	{
		assert(key < index_.size());

		Element*& prev = index_[key];

		if (nullptr == prev)
		{
			prev = last_;
			prev->next = new Element(key);
			last_ = prev->next;

			++size_;
		}

		assert(key == prev->next->key);

		return prev->next->count;
	}

	void erase(Element*& prev)
	{
		assert(nullptr != prev);

		--size_;
		Element* el = prev->next;

		assert(nullptr != el);

		prev->next = el->next;

		if (nullptr != prev->next)
		{
			assert(prev->next->key < index_.size());
			assert(index_[prev->next->key] == el);

			index_[prev->next->key] = prev;
		}

		delete el;
		prev = nullptr;
	}

public:

	explicit SmartSet(
		size_t         range = 0) :
		head_(Key(), 0),
		last_(&head_), size_(0),
		index_(range, nullptr)
	{ }

	SmartSet(const SmartSet& s) :
		head_(Key(), 0),
		last_(&head_),
		size_(s.size_),
		index_(s.index_.size(), nullptr)
	{
		for (const Element* el = s.head_.next; nullptr != el; el = el->next)
		{
			index_[el->key] = last_;
			last_->next = new Element(el->key, el->count);
			last_ = last_->next;
		}
	}

	SmartSet& operator=(const SmartSet& s)
	{
		if (this != &s)
		{
			assert(nullptr == head_.next);

			std::fill(index_.begin(), index_.end(), nullptr);
			index_.resize(s.index_.size(), nullptr);
			last_ = &head_;

			for (const Element* el = s.head_.next; nullptr != el; el = el->next)
			{
				index_[el->key] = last_;
				last_->next = new Element(el->key, el->count);
				last_ = last_->next;
			}

			size_ = s.size();
		}

		return *this;
	}

	~SmartSet()
	{
		this->clear();
	}

	void assign_flat(const SmartSet& s)
	{
		this->clear();
		assert(nullptr == head_.next);
		assert(std::all_of(index_.cbegin(), index_.cend(), [](const Element* elem){return nullptr == elem;}));

		index_.resize(s.index_.size(), nullptr);
		last_ = &head_;

		for (const Element* el = s.head_.next; nullptr != el; el = el->next)
		{
			index_[el->key] = last_;
			last_->next = new Element(el->key, 1);
			last_ = last_->next;
		}

		size_ = s.size();
	}

	SmartSet::iterator begin() const
	{
		return SmartSet::Iterator(head_.next);
	}

	SmartSet::iterator end() const
	{
		return SmartSet::Iterator(nullptr);
	}

	bool contains(const Key& key) const
	{
		assert(key < index_.size());

		if (nullptr == index_[key])
		{
			return false;
		}

		assert(index_[key]->next);
		assert(index_[key]->next->key == key);

		return true;
	}

	size_t count(const Key& key) const
	{
		assert(key < index_.size());

		if (nullptr == index_[key])
		{
			return 0;
		}

		assert(index_[key]->next);
		assert(index_[key]->next->key == key);

		return index_[key]->next->count;
	}

	void init(const Key& key, size_t count)
	{
		if (count > 0)
		{
			this->insert(key) = count;
			return;
		}

		assert(key < index_.size());

		Element*& prev = index_[key];

		if (nullptr != prev)
		{
			this->erase(prev);
		}
	}

	void add(const Key& key)
	{
		++this->insert(key);
	}

	void remove(const Key& key)
	{
		assert(key < index_.size());

		Element*& prev = index_[key];

		if (nullptr == prev)
		{
			return;
		}

		Element* el = prev->next;

		assert(nullptr != el);
		assert(key == el->key);

		if (el->count == 1)
		{
			this->erase(prev);
		}
		else
		{
			--el->count;
		}
	}

	void remove_strict(const Key& key)
	{
		assert(key < index_.size());
		Element*& prev = index_[key];
		assert(nullptr != prev);

		Element* el = prev->next;
		assert(key == el->key);

		if (el->count == 1)
		{
			this->erase(prev);
		}
		else
		{
			--el->count;
		}
	}

	bool empty() const
	{
		return head_.next == nullptr;
	}

	size_t size() const
	{
		return size_;
	}

	void clear()
	{
		for (Element* el = head_.next; nullptr != el; )
		{
			Element* tmp = el;
			el = el->next;

			assert(tmp->key < index_.size());

			index_[tmp->key] = nullptr;
			delete tmp;
		}

		last_ = &head_;
		head_.next = nullptr;
		size_ = 0;
	}

	friend std::ostream& operator<<(std::ostream& os, const SmartSet& s)
	{
		os << '{';

		for (const Element* el = s.head_.next; nullptr != el; el = el->next)
		{
			os << ' ' << el->key << ':' << el->count;
		}

		return os << " }";
	}
};

#endif
