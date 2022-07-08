/*****************************************************************************
 *  VATA Tree Automata Library
 *
 *  Copyright (c) 2011  Jiri Simacek <isimacek@fit.vutbr.cz>
 *
 *  Description:
 *    Shared list template header file.
 *
 *****************************************************************************/

#ifndef _VATA2_SHARED_LIST_HH_
#define _VATA2_SHARED_LIST_HH_

// insert class to proper namespace
namespace Vata2 {
	namespace Util {
		template <class T> class SharedList;
	}
}

/**
 * @brief  A shared list
 *
 * A shared list with a reference counter
 */
template <class T>
class Vata2::Util::SharedList {

private:

	struct Iterator {
		typedef std::input_iterator_tag iterator_category;
		typedef size_t value_type;
		typedef size_t* pointer;
		typedef size_t& reference;

		const SharedList* pos_;
		typename T::const_iterator iter_;

		Iterator() : pos_(nullptr), iter_() {}
		Iterator(const SharedList* pos) : pos_(pos), iter_(pos->subList_->begin()) {}

		Iterator& operator++()
		{
			// Assertions
			assert(pos_);
			assert(pos_->subList_);

			if (++iter_ != pos_->subList_->end())
				return *this;

			if ((pos_ = pos_->next_) != nullptr) {
				// Assertions
				assert(pos_->subList_);

				iter_ = pos_->subList_->begin();
			} else {
				iter_ = typename T::const_iterator();
			}

			return *this;
		}

		Iterator operator++(int) const {return ++Iterator(pos_);}
		bool operator==(const Iterator& rhs) const {return iter_ == rhs.iter_;}
		bool operator!=(const Iterator& rhs) const {return iter_ != rhs.iter_;}
		const size_t& operator*() const {return *iter_;}
	}; // struct Iterator

public:

	typedef Iterator const_iterator;

	const_iterator begin() const {return const_iterator(this);}
	const_iterator end() const {return const_iterator();}

private:

	SharedList* next_;
	T* subList_;
	size_t refCount_;

public:

	SharedList(T* subList = nullptr) :
		next_(nullptr), subList_(subList), refCount_(1)
	{ }

	void init(T* subList)
	{
		// Assertions
		assert(subList);

		subList_ = subList;
	}

	T* subList() {return subList_;}

	template <class Deleter>
	void release(const Deleter& deleter)
	{
		SharedList* elem = this, * tmp;

		while (elem && elem->refCount_ == 1) {
			tmp = elem;
			elem = elem->next_;
			deleter(tmp);
		}

		if (elem)
			--elem->counter_;
	}

	template <class Deleter>
	void unsafeRelease(const Deleter& deleter)
	{
		SharedList* elem = this;

		while (elem && elem->refCount_ == 1) {
			deleter(elem);
			elem = elem->next_;
		}

		if (elem)
			--elem->refCount_;
	}

	SharedList* copy()
	{
		++refCount_;

		return this;
	}

	template <class Allocator>
	static bool append(SharedList*& list, const typename T::value_type& v,
		Allocator& allocator)
	{
		if (!list) {
			list = allocator();
			list->next_ = nullptr;
			list->subList_->push_back(v);

			return true;
		}

		if (list->refCount_ > 1) {
			SharedList* tmp = allocator();
			tmp->next_ = list;
			list = tmp;
		}

		list->subList_->push_back(v);

		return false;
	}
};

#endif
