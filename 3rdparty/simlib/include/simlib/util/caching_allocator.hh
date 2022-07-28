/*****************************************************************************
 *  Simlib
 *
 *  Copyright (c) 2011  Jiri Simacek <isimacek@fit.vutbr.cz>
 *
 *  Description:
 *    Caching allocator template header file.
 *
 *****************************************************************************/

#ifndef _SIMLIB_CACHING_ALLOCATOR_HH_
#define _SIMLIB_CACHING_ALLOCATOR_HH_

#include <cstdlib>

#include <vector>
#include <functional>

#include <unordered_set>

// insert class to proper namespace
namespace Simlib {
	namespace Util {
		template <class T, class Initializer = std::function<void(T*)>> class CachingAllocator;
		template <class T, class Initializer = std::function<void(T*)>> class CachingArrayAllocator;
	}
}

template <class T, class Initializer>
class Simlib::Util::CachingAllocator {

public:

	typedef T* Ptr;

private:

	std::vector<T*> store_;
	Initializer initializer_;

public:

	CachingAllocator() : store_(), initializer_([](T*){}) {}

	explicit CachingAllocator(Initializer initializer) : store_(), initializer_(initializer) {}

	~CachingAllocator() {

		for (auto& element : this->store_)
			delete element;

	}

	Ptr operator()() {

		Ptr ptr;

		if (!this->store_.empty()) {

			ptr = this->store_.back();
			this->store_.pop_back();

		} else {

			ptr = new T();

		}

		this->initializer_(ptr);

		return ptr;

	}

	void reclaim(Ptr ptr) {

		this->store_.push_back(ptr);

	}

};

template <class T, class Initializer>
class Simlib::Util::CachingArrayAllocator {

public:

	typedef T* Ptr;

private:

	std::vector<T*> store_;
	size_t size_;
	size_t byteSize_;
	Initializer initializer_;

public:

    __attribute__((unused)) explicit CachingArrayAllocator(size_t size) : store_(), size_(size), byteSize_(size*sizeof(T)),
		initializer_([](T*){}) {}

    __attribute__((unused)) CachingArrayAllocator(size_t size, Initializer initializer) : store_(), size_(size),
		byteSize_(size*sizeof(T)), initializer_(initializer) {}

	~CachingArrayAllocator() {

		for (auto& element : this->store_)
			::operator delete(element);

	}

	Ptr operator()() {

		Ptr ptr;

		if (!this->store_.empty()) {

			ptr = this->store_.back();

			this->store_.pop_back();

		} else {

			ptr = reinterpret_cast<T*>(::operator new(this->byteSize_));

		}

		this->initializer_(ptr);

		return ptr;

	}

	void reclaim(Ptr ptr) {

		this->store_.push_back(ptr);

	}

	const size_t& size() const {

		return this->size_;

	}

};

#endif
