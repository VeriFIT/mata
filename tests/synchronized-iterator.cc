#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/utils/ord-vector.hh"
#include "mata/utils/synchronized-iterator.hh"
#include "mata/utils/utils.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::SynchronizedIterator") {
	SECTION("synchronized_universal_iterator, basic functionality") {
		SynchronizedUniversalIterator<OrdVector<int>::const_iterator> iu;

		// Basic functionality, position[0] gets emptied first
		OrdVector<int> v1{1, 2, 4};
		OrdVector<int> v2{0, 1, 3, 5};
		OrdVector<int> v3{0, 1, 2, 4};

		push_back(iu, v1);
		push_back(iu, v2);
		push_back(iu, v3);

		REQUIRE(iu.advance());
		auto current = iu.get_current();
		REQUIRE(current.size() == 3);
		REQUIRE(*current[0] == 1);
		REQUIRE(*current[1] == 1);
		REQUIRE(*current[2] == 1);
		REQUIRE(!iu.advance());

		iu.reset();

		// Empty after reset
		REQUIRE(!iu.advance());

		// Basic functionality, position[0] does not get emptied first
		v1 = {1, 2, 3, 4, 5};
		v2 = {0, 1, 3};
		v3 = {1, 2, 3};

		push_back(iu, v1);
		push_back(iu, v2);
		push_back(iu, v3);

		REQUIRE(iu.advance());
		current = iu.get_current();
		REQUIRE(current.size() == 3);
		REQUIRE(*current[0] == 1);
		REQUIRE(*current[1] == 1);
		REQUIRE(*current[2] == 1);
		REQUIRE(iu.advance());
		current = iu.get_current();
		REQUIRE(current.size() == 3);
		REQUIRE(*current[0] == 3);
		REQUIRE(*current[1] == 3);
		REQUIRE(*current[2] == 3);
		REQUIRE(!iu.advance());
	}

	SECTION("synchronized_universal_iterator, corner cases") {
		SynchronizedUniversalIterator<OrdVector<int>::const_iterator> iu;

		// Empty iterator
		REQUIRE(!iu.advance());
		REQUIRE(!iu.advance());
		auto current = iu.get_current();
		REQUIRE(current.empty());

		OrdVector<int> v1{};
		OrdVector<int> v2{1};
		OrdVector<int> v3{};

		push_back(iu, v1);
		push_back(iu, v2);
		push_back(iu, v3);

		REQUIRE(!iu.advance());

		// Empty after reset
		iu.reset();
		REQUIRE(!iu.advance());
		REQUIRE(!iu.advance());
		current = iu.get_current();
		REQUIRE(current.empty());

		// Only empty vectors
		push_back(iu, v1);

		REQUIRE(!iu.advance());

		push_back(iu, v3);
		iu.reset();

		REQUIRE(!iu.advance());

		// Insert the same vector twice
		OrdVector<int> v4{1, 2};
		OrdVector<int> v5{2};

		push_back(iu, v4);
		push_back(iu, v4);
		push_back(iu, v5);

		REQUIRE(iu.advance());
		current = iu.get_current();
		REQUIRE(*current[0] == 2);
		REQUIRE(*current[1] == 2);
		REQUIRE(*current[2] == 2);
		REQUIRE(!iu.advance());
	}

	SECTION("synchronized_universal_iterator, three inputs where input 0 moves past input 1 - issue #769") {
		// All three inputs live in one buffer with input 0 first, so the outcome does not depend on where separate
		//  containers happen to be allocated. Only 9 is common to all three.
		const std::vector<int> buffer{1, 2, 5, 9, /* input 1 */ 2, 9, /* input 2 */ 5, 9};
		SynchronizedUniversalIterator<std::vector<int>::const_iterator> sync_iterator;
		sync_iterator.push_back(buffer.begin(), buffer.begin() + 4);
		sync_iterator.push_back(buffer.begin() + 4, buffer.begin() + 6);
		sync_iterator.push_back(buffer.begin() + 6, buffer.end());

		REQUIRE(sync_iterator.advance());
		const auto& current = sync_iterator.get_current();
		REQUIRE(current.size() == 3);
		CHECK(*current[0] == 9);
		CHECK(*current[1] == 9);
		CHECK(*current[2] == 9);
		CHECK(!sync_iterator.advance());
	}

	SECTION("SynchronizedExistentialIterator, basic functionality") {
		SynchronizedExistentialIterator<OrdVector<int>::const_iterator> ie;

		// Basic functionality
		OrdVector<int> v1{1, 2};
		OrdVector<int> v2{0, 3};
		OrdVector<int> v3{0, 1, 2, 3};

		push_back(ie, v1);
		push_back(ie, v2);
		push_back(ie, v3);

		int i = 0;
		while (ie.advance()) {
			auto current = ie.get_current();
			REQUIRE(current.size() == 2);
			REQUIRE(*current[0] == i);
			REQUIRE(*current[1] == i);
			i++;
		};
		REQUIRE(i == 4);
	}

	SECTION("SynchronizedExistentialIterator, corner cases") {
		SynchronizedExistentialIterator<OrdVector<int>::const_iterator> ie;

		// Empty iterator
		REQUIRE(!ie.advance());
		REQUIRE(!ie.advance());
		auto current = ie.get_current();
		REQUIRE(current.empty());

		// Empty vectors
		OrdVector<int> v1{};
		OrdVector<int> v2{1};
		OrdVector<int> v3{};

		push_back(ie, v1);
		push_back(ie, v2);
		push_back(ie, v3);

		REQUIRE(ie.advance());
		current = ie.get_current();
		REQUIRE(*current[0] == 1);
		REQUIRE(current.size() == 1);
		REQUIRE(!ie.advance());

		// Empty after reset
		ie.reset();
		REQUIRE(!ie.advance());
		current = ie.get_current();
		REQUIRE(current.empty());
		REQUIRE(!ie.advance());
		current = ie.get_current();
		REQUIRE(current.empty());

		// Only empty vectors
		push_back(ie, v1);

		REQUIRE(!ie.advance());

		push_back(ie, v3);
		ie.reset();

		REQUIRE(!ie.advance());

		// Insert the same vector twice
		OrdVector<int> v4{1, 2};
		OrdVector<int> v5{2};

		push_back(ie, v4);
		push_back(ie, v5);
		push_back(ie, v4);

		REQUIRE(ie.advance());
		current = ie.get_current();
		REQUIRE(current.size() == 2);
		REQUIRE(*current[0] == 1);
		REQUIRE(*current[1] == 1);
		REQUIRE(ie.advance());
		current = ie.get_current();
		REQUIRE(current.size() == 3);
		REQUIRE(*current[0] == 2);
		REQUIRE(*current[1] == 2);
		REQUIRE(*current[2] == 2);
		REQUIRE(!ie.advance());
	}
}
