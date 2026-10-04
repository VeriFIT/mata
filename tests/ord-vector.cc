/* tests-ord-vector.cc -- tests of OrdVector
 */

#include <unordered_map>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/utils/ord-vector.hh"
#include "mata/utils/utils.hh"

using namespace mata::utils;

TEST_CASE("mata::utils::OrdVector::erase()") {
	using OrdVectorT = OrdVector<int>;
	OrdVectorT set{1, 2, 3, 4, 6};
	set.erase(3);
	CHECK(set == OrdVectorT{1, 2, 4, 6});
	set.erase(4);
	CHECK(set == OrdVectorT{1, 2, 6});
	CHECK(set.erase(5) == 0);
	set.erase(2);
	CHECK(set == OrdVectorT{1, 6});
	set.erase(1);
	set.erase(6);
	CHECK(set.empty());
	set.push_back(3);
	CHECK(set == OrdVectorT{3});
	set.erase(3);
	CHECK(set.empty());
	CHECK(set.erase(0) == 0);
	set.emplace_back(3);
	set.emplace_back(4);
	CHECK(set == OrdVectorT{3, 4});
	CHECK(set.erase(0) == 0);
}

TEST_CASE("mata::utils::OrdVector::front())") {
	OrdVector<int> vector{0, 1, 2, 3};
	CHECK(vector.front() == 0);
	vector.erase(0);
	const OrdVector<int> vector_const{vector};
	CHECK(vector_const.front() == 1);
}

TEST_CASE("mata::utils::OrdVector::intersection()") {
	using OrdVectorT = OrdVector<int>;
	OrdVectorT set1{};
	OrdVectorT set2{};

	SECTION("Empty sets") { REQUIRE(set1.intersection(set2).empty()); }

	SECTION("Sets of same lengths") {
		set1 = {1, 3, 5, 7};
		set2 = {1, 2, 5, 6};

		REQUIRE(set1.intersection(set2) == OrdVectorT{1, 5});
	}

	SECTION("Sets of different lengths") {
		set1 = {1, 3, 5, 7};
		set2 = {1, 2, 5, 7, 8};

		REQUIRE(set1.intersection(set2) == OrdVectorT{1, 5, 7});
	}

	SECTION("Empty intersection of non-empty sets") {
		set1 = {0, 3, 6};
		set2 = {1, 2, 5, 7, 8};

		REQUIRE(set1.intersection(set2).empty());
	}
}

TEST_CASE("mata::utils::OrdVector::difference()") {
	using OrdVectorT = OrdVector<int>;
	OrdVectorT set1{};
	OrdVectorT set2{};

	SECTION("Empty sets") { CHECK(set1.difference(set2).empty()); }

	SECTION("Empty rhs set") {
		set1 = {1, 2, 3};
		CHECK(set1.difference(set2) == set1);
	}

	SECTION("Empty lhs set") {
		set2 = {1, 2, 3};
		CHECK(set1.difference(set2).empty());
	}

	SECTION("filled sets") {
		set1 = {1, 2, 3};
		set2 = {1, 2, 3};
		CHECK(set1.difference(set2).empty());

		set1 = {1, 2, 3};
		set2 = {1, 3};
		CHECK(set1.difference(set2) == mata::utils::OrdVector<int>{2});

		set1 = {1, 3};
		set2 = {1, 2, 3};
		CHECK(set1.difference(set2).empty());

		set1 = {1, 2, 3};
		set2 = {3};
		CHECK(set1.difference(set2) == mata::utils::OrdVector<int>{1, 2});
	}
}

TEST_CASE("mata::utils::OrdVector::min()") {
	SECTION("Empty vector") {
		OrdVector<int> vec;
		CHECK_THROWS_AS(vec.min(), std::out_of_range);
	}

	SECTION("One element") {
		OrdVector<int> vec{42};
		CHECK(vec.min() == 42);
	}

	SECTION("Multiple elements") {
		OrdVector<int> vec{3, 2, 4, 2, 5, 99};
		CHECK(vec.min() == 2);
	}
}

TEST_CASE("mata::utils::OrdVector::max()") {
	SECTION("Empty vector") {
		OrdVector<int> vec;
		CHECK_THROWS_AS(vec.max(), std::out_of_range);
	}

	SECTION("One element") {
		OrdVector<int> vec{42};
		CHECK(vec.max() == 42);
	}

	SECTION("Multiple elements") {
		OrdVector<int> vec{3, 2, 4, 2, 5, 99};
		CHECK(vec.max() == 99);
	}
}

TEST_CASE("mata::utils::OrdVector::at()") {
	SECTION("Empty vector") {
		OrdVector<int> vec;
		CHECK_THROWS_AS(vec.at(0), std::out_of_range);
	}

	SECTION("One element") {
		OrdVector<int> vec{42};
		CHECK(vec.at(0) == 42);
	}

	SECTION("Multiple elements") {
		OrdVector<int> vec{3, 2, 4, 2, 5, 99};
		CHECK(vec.at(0) == 2);
		CHECK(vec.at(1) == 3);
		CHECK(vec.at(2) == 4);
		CHECK(vec.at(3) == 5);
		CHECK(vec.at(4) == 99);
		CHECK_THROWS_AS(vec.at(5), std::out_of_range);
	}
}

TEST_CASE("std::hash<mata::utils::OrdVector<Key>>") {
	const OrdVector<int> vec1{1, 2, 3};
	const OrdVector<int> vec2{3, 2, 1}; // Same elements, different insertion order.
	const OrdVector<int> vec3{1, 2, 4};
	const std::hash<OrdVector<int>> hasher{};

	CHECK(hasher(vec1) == hasher(vec2));
	CHECK(hasher(vec1) != hasher(vec3));

	// Usable as an unordered_set element without any extra Hash argument, since it is a legitimate
	//  program-defined std::hash specialization (unlike hashing std::set/std::vector directly, see #628).
	std::unordered_set<OrdVector<int>> set{vec1, vec2, vec3};
	CHECK(set.size() == 2);
}

TEST_CASE("mata::utils::SetHash and mata::utils::VectorHash") {
	// #628: mata must not specialize std::hash for std::set<A>/std::vector<A> for arbitrary A, since A may not be a
	//  program-defined type. SetHash/VectorHash are the replacement, usable as the explicit Hash argument of
	//  unordered containers.
	const SetHash<int> set_hasher{};
	CHECK(set_hasher(std::set<int>{1, 2, 3}) == set_hasher(std::set<int>{3, 2, 1}));
	CHECK(set_hasher(std::set<int>{1, 2, 3}) != set_hasher(std::set<int>{1, 2, 4}));

	const VectorHash<int> vector_hasher{};
	CHECK(vector_hasher(std::vector<int>{1, 2, 3}) == vector_hasher(std::vector<int>{1, 2, 3}));
	CHECK(vector_hasher(std::vector<int>{1, 2, 3}) != vector_hasher(std::vector<int>{3, 2, 1}));

	const std::unordered_map<std::vector<int>, int, VectorHash<int>> map{{{1, 2, 3}, 42}};
	CHECK(map.at({1, 2, 3}) == 42);
}

TEST_CASE("mata::utils::PairHash") {
	// #628: mata must not specialize std::hash for std::pair<A, B> for arbitrary A/B, since A/B may not be a
	//  program-defined type. PairHash is the replacement, usable as the explicit Hash argument of unordered
	//  containers.
	const PairHash<int, int> pair_hasher{};
	CHECK(pair_hasher({1, 2}) == pair_hasher({1, 2}));
	CHECK(pair_hasher({1, 2}) != pair_hasher({2, 1}));

	const std::unordered_map<std::pair<int, int>, int, PairHash<int, int>> map{{{1, 2}, 42}};
	CHECK(map.at({1, 2}) == 42);
}

TEST_CASE("mata::utils::OrdVector should not be polymorphic") {
	// #746: Remove virtual from OrdVector members
	// OrdVector must not have a vptr, so sizeof should equal std::vector
	CHECK(!std::is_polymorphic_v<OrdVector<int>>);
	CHECK(!std::is_polymorphic_v<OrdVector<char>>);
	CHECK(sizeof(OrdVector<int>) == sizeof(std::vector<int>));
}

TEST_CASE("mata::utils::OrdVector::insert() with a hint - issue #769") {
	OrdVector<int> vec{1, 5, 9};

	SECTION("a correct hint inserts at the hinted place") {
		const auto [it, inserted] = vec.insert(vec.begin() + 1, 7);
		CHECK(inserted);
		CHECK(*it == 7);
		CHECK(vec == OrdVector<int>{1, 5, 7, 9});
	}

	SECTION("a wrong hint falls back to the search and keeps the vector sorted") {
		// The element belongs between 1 and 5, but the hint points at the end.
		const auto [it, inserted] = vec.insert(vec.end(), 3);
		CHECK(inserted);
		CHECK(*it == 3);
		CHECK(vec == OrdVector<int>{1, 3, 5, 9});
		CHECK(mata::utils::is_sorted(vec.to_vector()));
	}

	SECTION("an end() hint does not append a duplicate out of order") {
		const auto [it, inserted] = vec.insert(vec.end(), 5);
		CHECK(!inserted);
		CHECK(*it == 5);
		CHECK(vec == OrdVector<int>{1, 5, 9});
		CHECK(mata::utils::is_sorted(vec.to_vector()));
	}
}

TEST_CASE("mata::utils::is_sorted() on an empty vector - issue #769") {
	const std::vector<int> empty{};
	CHECK(mata::utils::is_sorted(empty));
	CHECK(mata::utils::is_sorted(std::vector<int>{1}));
	CHECK(mata::utils::is_sorted(std::vector<int>{1, 2, 3}));
	CHECK(!mata::utils::is_sorted(std::vector<int>{1, 1}));
	CHECK(!mata::utils::is_sorted(std::vector<int>{3, 2}));
}

TEST_CASE("mata::utils::OrdVector::filter() accepts a named predicate - issue #769") {
	OrdVector<int> vec{1, 2, 3, 4, 5};
	auto is_odd = [](const int value) { return value % 2 == 1; };
	vec.filter(is_odd);
	CHECK(vec == OrdVector<int>{1, 3, 5});
	CHECK(vec.size() == 3);

	OrdVector<int> indexed{10, 20, 30, 40};
	auto keeps_first_two = [](const size_t index) { return index < 2; };
	indexed.filter_indexes(keeps_first_two);
	CHECK(indexed == OrdVector<int>{10, 20});
	CHECK(indexed.size() == 2);
}

TEST_CASE("mata::BoolVector::get_elements() returns the set bits - issue #769") {
	const mata::BoolVector bool_vector{0, 1, 0, 1, 1};
	std::vector<size_t> elements{};
	mata::BoolVector::get_elements(&elements, bool_vector);
	CHECK(elements == std::vector<size_t>{1, 3, 4});

	std::vector<size_t> other_elements{};
	bool_vector.get_elements(other_elements);
	CHECK(other_elements == std::vector<size_t>{1, 3, 4});
}

TEST_CASE("mata::utils::filter() shrinks the vector - issue #769") {
	std::vector<int> values{1, 2, 3, 4, 5};
	mata::utils::filter(values, [](const int value) { return value % 2 == 1; });
	// reserve() kept the dropped elements in the vector.
	CHECK(values == std::vector<int>{1, 3, 5});

	std::vector<int> indexed{10, 20, 30};
	mata::utils::filter_indexes(indexed, [](const size_t index) { return index == 1; });
	CHECK(indexed == std::vector<int>{20});
}
