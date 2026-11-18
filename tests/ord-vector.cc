/* tests-ord-vector.cc -- tests of OrdVector
 */

#include <algorithm>
#include <compare>
#include <deque>
#include <iterator>
#include <memory>
#include <random>
#include <ranges>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "mata/nfa/plumbing.hh"
#include "mata/nft/plumbing.hh"
#include "mata/utils/ord-vector.hh"
#include "mata/utils/sparse-set.hh"
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

TEST_CASE("mata::utils::OrdVector::count()") {
	const OrdVector<int> set{1, 3, 5, 7};
	CHECK(set.count(1) == 1);
	CHECK(set.count(7) == 1);
	CHECK(set.count(0) == 0);
	CHECK(set.count(4) == 0);
	CHECK(set.count(8) == 0);
	CHECK(OrdVector<int>{}.count(1) == 0);
}

template <class T>
concept HasToVectorMut = requires(T& vec) { vec.to_vector_mut(); };

TEST_CASE("mata::utils::OrdVector::to_vector_mut()") {
	static_assert(!HasToVectorMut<const OrdVector<int>>, "a const OrdVector must not hand out its vector mutably");
	OrdVector<int> set{1, 2, 3};
	set.to_vector_mut().push_back(4);
	CHECK(set == OrdVector<int>{1, 2, 3, 4});
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

TEST_CASE("mata::utils::filter() and filter_indexes() drop the elements that do not stay - issue #769") {
	std::vector<int> values{1, 2, 3, 4, 5};
	filter(values, [](const int value) { return value % 2 == 1; });
	CHECK(values == std::vector<int>{1, 3, 5});

	std::vector<int> indexed{10, 20, 30, 40};
	filter_indexes(indexed, [](const size_t index) { return index != 1; });
	CHECK(indexed == std::vector<int>{10, 30, 40});

	std::vector<int> nothing_stays{1, 2, 3};
	filter(nothing_stays, [](const int) { return false; });
	CHECK(nothing_stays.empty());
}

TEST_CASE("mata::utils::filter() and filter_indexes() work for elements without a default constructor - issue #769") {
	struct NoDefault {
		int value;
		explicit NoDefault(const int value) : value(value) {}
		bool operator==(const NoDefault& other) const { return value == other.value; }
	};

	std::vector<NoDefault> values;
	for (const int value : {1, 2, 3, 4, 5}) { values.emplace_back(value); }
	filter(values, [](const NoDefault& element) { return element.value % 2 == 1; });
	REQUIRE(values.size() == 3);
	CHECK(values[0] == NoDefault{1});
	CHECK(values[1] == NoDefault{3});
	CHECK(values[2] == NoDefault{5});

	std::vector<NoDefault> indexed;
	for (const int value : {10, 20, 30, 40}) { indexed.emplace_back(value); }
	filter_indexes(indexed, [](const size_t index) { return index != 1; });
	REQUIRE(indexed.size() == 3);
	CHECK(indexed[0] == NoDefault{10});
	CHECK(indexed[1] == NoDefault{30});
	CHECK(indexed[2] == NoDefault{40});
}

TEST_CASE("mata::utils::OrdVector::filter() and filter_indexes() with a named predicate - issue #769") {
	OrdVector<int> vec{1, 2, 3, 4, 5};
	const auto is_odd = [](const int value) { return value % 2 == 1; };
	vec.filter(is_odd);
	CHECK(vec == OrdVector<int>{1, 3, 5});
	CHECK(vec.size() == 3);

	OrdVector<int> indexed{10, 20, 30, 40};
	const auto keeps_first_two = [](const size_t index) { return index < 2; };
	indexed.filter_indexes(keeps_first_two);
	CHECK(indexed == OrdVector<int>{10, 20});
	CHECK(indexed.size() == 2);
}

TEST_CASE("mata::utils::OrdVector::filter() and filter_indexes() take a move-only predicate") {
	OrdVector<int> vec{1, 2, 3, 4, 5};
	vec.filter([divisor = std::make_unique<int>(2)](const int value) { return value % *divisor == 1; });
	CHECK(vec == OrdVector<int>{1, 3, 5});

	OrdVector<int> indexed{10, 20, 30, 40};
	indexed.filter_indexes([limit = std::make_unique<size_t>(2)](const size_t index) { return index < *limit; });
	CHECK(indexed == OrdVector<int>{10, 20});
}

TEST_CASE("mata::utils::OrdVector::insert() with a hint - issue #769") {
	OrdVector<int> vec{1, 5, 9};

	SECTION("a correct hint inserts at the hinted place") {
		const auto [it, inserted] = vec.insert(vec.begin() + 2, 7);
		CHECK(inserted);
		CHECK(*it == 7);
		CHECK(vec == OrdVector<int>{1, 5, 7, 9});
	}

	SECTION("a correct end() hint appends") {
		const auto [it, inserted] = vec.insert(vec.end(), 10);
		CHECK(inserted);
		CHECK(*it == 10);
		CHECK(vec == OrdVector<int>{1, 5, 9, 10});
	}

	SECTION("a wrong end() hint falls back to the search and keeps the vector sorted") {
		const auto [it, inserted] = vec.insert(vec.end(), 3);
		CHECK(inserted);
		CHECK(*it == 3);
		CHECK(vec.to_vector() == std::vector<int>{1, 3, 5, 9});
	}

	SECTION("a wrong inner hint still inserts") {
		const auto [it, inserted] = vec.insert(vec.begin(), 7);
		CHECK(inserted);
		CHECK(*it == 7);
		CHECK(vec.to_vector() == std::vector<int>{1, 5, 7, 9});
	}

	SECTION("a present element is not inserted again, wherever the hint points") {
		for (const std::ptrdiff_t hint : {0, 1, 2, 3}) {
			const auto [it, inserted] = vec.insert(vec.begin() + hint, 5);
			CHECK(!inserted);
			CHECK(*it == 5);
			CHECK(vec.to_vector() == std::vector<int>{1, 5, 9});
		}
	}

	SECTION("an empty vector") {
		OrdVector<int> empty{};
		const auto [it, inserted] = empty.insert(empty.end(), 4);
		CHECK(inserted);
		CHECK(*it == 4);
		CHECK(empty == OrdVector<int>{4});
	}
}

TEST_CASE("mata::utils::is_sorted() - issue #769") {
	CHECK(mata::utils::is_sorted(std::vector<int>{}));
	CHECK(mata::utils::is_sorted(std::vector<int>{1}));
	CHECK(mata::utils::is_sorted(std::vector<int>{1, 2, 3}));
	CHECK(!mata::utils::is_sorted(std::vector<int>{1, 1}));
	CHECK(!mata::utils::is_sorted(std::vector<int>{3, 2}));
}

TEST_CASE("mata::utils::OrdVector constructors take only ranges of keys - issue #769") {
	// A scalar convertible to the key is one key, not a range to iterate.
	const OrdVector<size_t> from_int(3);
	CHECK(from_int.to_vector() == std::vector<size_t>{3});

	const std::vector<unsigned> unsorted{5, 1, 5, 3};
	const OrdVector<size_t> from_range(unsorted);
	CHECK(from_range.to_vector() == std::vector<size_t>{1, 3, 5});

	OrdVector<int> mutable_vec{2, 1};
	mutable_vec.to_vector_mut().push_back(3);
	CHECK(mutable_vec.to_vector() == std::vector<int>{1, 2, 3});
}

namespace {
/// Converts to an int only from an lvalue: a range of these is read through its references.
struct LvalueOnlyConvertible {
	int value;
	operator int() const& { return value; } // NOLINT(google-explicit-constructor)
	operator int() && = delete;
};

/// A key with relational operators and no `==` (a non-defaulted `<=>` does not provide one).
struct Rank {
	int value;
	std::strong_ordering operator<=>(const Rank& other) const { return value <=> other.value; }
};
} // namespace

TEST_CASE("mata::utils::OrdVector range constructor reads what the range yields - issue #769") {
	SECTION("elements convertible only from a reference") {
		const std::vector<LvalueOnlyConvertible> values{{3}, {1}, {3}};
		CHECK(OrdVector<int>(values).to_vector() == std::vector<int>{1, 3});
	}

	SECTION("a named filter view, whose begin() is non-const") {
		std::vector<int> values{1, 2, 3, 4};
		auto evens = values | std::views::filter([](const int v) { return v % 2 == 0; });
		CHECK(OrdVector<int>(evens).to_vector() == std::vector<int>{2, 4});
	}

	SECTION("a range whose sentinel differs from its iterator") {
		const std::vector<int> values{4, 2, 9};
		const auto counted{std::ranges::subrange(std::counted_iterator(values.begin(), 2), std::default_sentinel)};
		CHECK(OrdVector<int>(counted).to_vector() == std::vector<int>{2, 4});
	}
}

TEST_CASE("mata::utils::OrdVector::insert() works for keys with relational operators only - issue #769") {
	OrdVector<Rank> vec{mata::utils::sorted_unique, std::vector<Rank>{{1}, {5}}};
	const auto [appended, appended_new] = vec.insert(vec.end(), Rank{7});
	CHECK(appended_new);
	CHECK(appended->value == 7);
	const auto [present, present_new] = vec.insert(vec.begin() + 1, Rank{5});
	CHECK(!present_new);
	CHECK(present->value == 5);
	const auto [searched, searched_new] = vec.insert(vec.end(), Rank{1}); // Wrong hint: falls back to the search.
	CHECK(!searched_new);
	CHECK(searched->value == 1);
	CHECK(vec.size() == 3);
}

TEST_CASE("mata::BoolVector::get_elements() returns the indices of the set bits - issue #769") {
	mata::BoolVector bool_vector{0, 1, 0, 1, 1};
	std::vector<size_t> elements{7};
	mata::BoolVector::get_elements(&elements, bool_vector);
	CHECK(elements == std::vector<size_t>{1, 3, 4});

	std::vector<size_t> member_elements{7};
	bool_vector.get_elements(member_elements);
	CHECK(member_elements == std::vector<size_t>{1, 3, 4});

	const mata::BoolVector& const_bool_vector{bool_vector};
	std::vector<size_t> const_elements{};
	const_bool_vector.get_elements(const_elements);
	CHECK(const_elements == std::vector<size_t>{1, 3, 4});
}

TEST_CASE("mata::BoolVector treats every nonzero byte as set - issue #769") {
	const mata::BoolVector bool_vector{0, 2, 1};
	CHECK(bool_vector.count() == 2);
	std::vector<size_t> elements{};
	CHECK(bool_vector.get_elements(elements) == std::vector<size_t>{1, 2});
	CHECK(mata::utils::SparseSet<size_t>{bool_vector} == mata::utils::SparseSet<size_t>{1, 2});
	mata::nfa::StateSet states{};
	mata::nfa::plumbing::get_elements(&states, bool_vector);
	CHECK(states == mata::nfa::StateSet{1, 2});
	mata::nfa::StateSet nft_states{};
	mata::nft::plumbing::get_elements(&nft_states, bool_vector);
	CHECK(nft_states == mata::nfa::StateSet{1, 2});

	// An output without reserve() is filled as well.
	std::deque<size_t> deque{7};
	bool_vector.get_elements(deque);
	CHECK(deque == std::deque<size_t>{1, 2});
}

TEST_CASE("mata::utils::rename() renames every element - issue #769") {
	// Every element equals its index: each has to be renamed nonetheless.
	std::vector<size_t> values{0, 1, 2};
	mata::utils::rename(values, std::vector<size_t>{2, 0, 1});
	CHECK(values == std::vector<size_t>{2, 0, 1});

	OrdVector<size_t> ord_vector{0, 1};
	ord_vector.rename({1, 2});
	CHECK(ord_vector == OrdVector<size_t>{1, 2});

	// A renaming that reorders and merges elements keeps the vector sorted and unique.
	OrdVector<size_t> reordered{0, 1, 2};
	reordered.rename({2, 0, 0});
	CHECK(reordered.to_vector() == std::vector<size_t>{0, 2});
	CHECK(OrdVector<size_t>::difference(reordered, OrdVector<size_t>{0}) == OrdVector<size_t>{2});
}

TEST_CASE("mata::utils::OrdVector::OrdVector(VectorType&&) - issue #847") {
	using OrdVectorT = OrdVector<int>;

	SECTION("an unsorted vector with duplicates is sorted and deduplicated") {
		std::vector<int> values{5, 1, 5, 3, 1, 4};
		const OrdVectorT moved{std::move(values)};
		CHECK(moved == OrdVectorT{1, 3, 4, 5});
		CHECK(moved == OrdVectorT{std::vector<int>{5, 1, 5, 3, 1, 4}});
	}

	SECTION("an empty vector yields an empty container") {
		CHECK(OrdVectorT{std::vector<int>{}}.empty());
		CHECK(OrdVectorT{sorted_unique, std::vector<int>{}}.empty());
	}

	SECTION("the storage of the operand is adopted, not copied") {
		std::vector<int> values{1, 2, 3};
		const int* const data{values.data()};
		const OrdVectorT moved{std::move(values)};
		CHECK(moved.to_vector().data() == data);
	}

	SECTION("the sorted_unique overload adopts a sorted vector as it is") {
		std::vector<int> values{1, 3, 4, 5};
		const int* const data{values.data()};
		const OrdVectorT adopted{sorted_unique, std::move(values)};
		CHECK(adopted == OrdVectorT{1, 3, 4, 5});
		CHECK(adopted.to_vector().data() == data);
	}

	SECTION("an lvalue vector still selects the copying overload") {
		std::vector<int> values{3, 1, 2};
		const OrdVectorT copied{values};
		CHECK(copied == OrdVectorT{1, 2, 3});
		CHECK(values == std::vector<int>{3, 1, 2});
	}
}

TEST_CASE("mata::utils::OrdVector::set_union() into a buffer - issue #847") {
	using OrdVectorT = OrdVector<int>;
	OrdVectorT buffer{};

	SECTION("an empty left operand reports every element of the right one as new") {
		CHECK(OrdVectorT::set_union(OrdVectorT{}, OrdVectorT{1, 2}, buffer));
		CHECK(buffer == OrdVectorT{1, 2});
	}

	SECTION("an empty right operand adds nothing") {
		CHECK_FALSE(OrdVectorT::set_union(OrdVectorT{1, 2}, OrdVectorT{}, buffer));
		CHECK(buffer == OrdVectorT{1, 2});
	}

	SECTION("two empty operands yield an empty union") {
		CHECK_FALSE(OrdVectorT::set_union(OrdVectorT{}, OrdVectorT{}, buffer));
		CHECK(buffer.empty());
	}

	SECTION("disjoint operands interleave and report new elements") {
		CHECK(OrdVectorT::set_union(OrdVectorT{1, 3, 5}, OrdVectorT{2, 4}, buffer));
		CHECK(buffer == OrdVectorT{1, 2, 3, 4, 5});
	}

	SECTION("a right operand contained in the left one reports no new element") {
		CHECK_FALSE(OrdVectorT::set_union(OrdVectorT{1, 2, 3}, OrdVectorT{1, 3}, buffer));
		CHECK(buffer == OrdVectorT{1, 2, 3});
	}

	SECTION("overlapping operands report the elements outside the left one") {
		CHECK(OrdVectorT::set_union(OrdVectorT{1, 2, 3}, OrdVectorT{3, 4}, buffer));
		CHECK(buffer == OrdVectorT{1, 2, 3, 4});
	}

	SECTION("a cleared buffer keeps its capacity across calls") {
		OrdVectorT::set_union(OrdVectorT{1, 2, 3, 4}, OrdVectorT{5, 6}, buffer);
		const size_t capacity{buffer.to_vector().capacity()};
		buffer.clear();
		CHECK(OrdVectorT::set_union(OrdVectorT{1}, OrdVectorT{2}, buffer));
		CHECK(buffer == OrdVectorT{1, 2});
		CHECK(buffer.to_vector().capacity() == capacity);
	}
}

TEST_CASE("mata::utils::OrdVector::insert(const OrdVector&) reports growth - issue #847") {
	using OrdVectorT = OrdVector<int>;
	OrdVectorT values{1, 3};
	CHECK_FALSE(values.insert(OrdVectorT{}));
	CHECK_FALSE(values.insert(OrdVectorT{1, 3}));
	CHECK(values.insert(OrdVectorT{2, 3}));
	CHECK(values == OrdVectorT{1, 2, 3});
}

namespace {

/// Draw a value in [0, bound) from the raw generator output, portably across standard libraries.
size_t draw(std::mt19937& rng, const size_t bound) { return static_cast<size_t>(rng()) % bound; }

/// Up to @p max_size values drawn from [0, @p universe), unsorted and possibly repeated.
std::vector<int> random_values(std::mt19937& rng, const size_t max_size, const size_t universe) {
	std::vector<int> values(draw(rng, max_size + 1));
	for (int& value : values) { value = static_cast<int>(draw(rng, universe)); }
	return values;
}

std::vector<int> elements(const std::set<int>& set) { return {set.begin(), set.end()}; }

} // namespace

TEST_CASE("mata::utils::OrdVector against std::set (randomized)") {
	constexpr size_t UNIVERSE{24};
	for (unsigned seed{0}; seed < 500; ++seed) {
		std::mt19937 rng{seed};
		INFO("seed " << seed);
		const std::vector<int> lhs_values{random_values(rng, 2 + seed % 16, UNIVERSE)};
		const std::vector<int> rhs_values{random_values(rng, 2 + seed % 16, UNIVERSE)};
		const OrdVector<int> lhs{lhs_values};
		const OrdVector<int> rhs{rhs_values};
		const std::set<int> lhs_set{lhs_values.begin(), lhs_values.end()};
		const std::set<int> rhs_set{rhs_values.begin(), rhs_values.end()};
		REQUIRE(lhs.to_vector() == elements(lhs_set));
		REQUIRE(rhs.to_vector() == elements(rhs_set));
		CHECK(OrdVector<int>{lhs_set}.to_vector() == elements(lhs_set));
		CHECK(OrdVector<int>(lhs_values.begin(), lhs_values.end()).to_vector() == elements(lhs_set));

		std::set<int> expected{};
		std::ranges::set_union(lhs_set, rhs_set, std::inserter(expected, expected.end()));
		CHECK(OrdVector<int>::set_union(lhs, rhs).to_vector() == elements(expected));
		OrdVector<int> united{lhs};
		CHECK(united.insert(rhs) == (expected.size() > lhs_set.size()));
		CHECK(united.to_vector() == elements(expected));

		expected.clear();
		std::ranges::set_intersection(lhs_set, rhs_set, std::inserter(expected, expected.end()));
		CHECK(lhs.intersection(rhs).to_vector() == elements(expected));
		CHECK(OrdVector<int>::intersection(lhs, rhs).to_vector() == elements(expected));
		CHECK(lhs.is_intersection_empty_with(rhs) == expected.empty());
		CHECK(are_disjoint(lhs, rhs) == expected.empty());

		expected.clear();
		std::ranges::set_difference(lhs_set, rhs_set, std::inserter(expected, expected.end()));
		CHECK(lhs.difference(rhs).to_vector() == elements(expected));
		CHECK(OrdVector<int>::difference(lhs, rhs).to_vector() == elements(expected));

		CHECK(lhs.is_subset_of(rhs) == std::ranges::includes(rhs_set, lhs_set));
		CHECK(rhs.is_subset_of(lhs) == std::ranges::includes(lhs_set, rhs_set));
		CHECK((lhs == rhs) == (lhs_set == rhs_set));
		CHECK((lhs < rhs) == (lhs_set < rhs_set));
		if (!lhs_set.empty()) {
			CHECK(lhs.min() == *lhs_set.begin());
			CHECK(lhs.max() == *lhs_set.rbegin());
		}

		// Membership, including values just outside the drawn universe.
		for (int value{-1}; value <= static_cast<int>(UNIVERSE); ++value) {
			CHECK(lhs.contains(value) == lhs_set.contains(value));
			CHECK(lhs.count(value) == lhs_set.count(value));
			CHECK((lhs.find(value) != lhs.end()) == lhs_set.contains(value));
		}

		// A random sequence of single-element insertions and removals.
		OrdVector<int> mutated{lhs};
		std::set<int> mutated_set{lhs_set};
		for (size_t step{0}; step < 40; ++step) {
			const int value{static_cast<int>(draw(rng, UNIVERSE))};
			if (draw(rng, 2) == 0) {
				const auto [position, inserted] = mutated.insert(value);
				CHECK(inserted == mutated_set.insert(value).second);
				CHECK(*position == value);
			} else {
				CHECK(mutated.erase(value) == mutated_set.erase(value));
			}
			REQUIRE(mutated.to_vector() == elements(mutated_set));
		}

		const int modulus{static_cast<int>(1 + draw(rng, 4))};
		mutated = lhs;
		mutated_set = lhs_set;
		CHECK(mutated.erase_if([&](const int value) {
			return value % modulus == 0;
		}) == std::erase_if(mutated_set, [&](const int value) { return value % modulus == 0; }));
		CHECK(mutated.to_vector() == elements(mutated_set));
	}
}

TEST_CASE("mata::utils::filter() and filter_indexes() against std::erase_if (randomized)") {
	for (unsigned seed{0}; seed < 500; ++seed) {
		std::mt19937 rng{seed};
		INFO("seed " << seed);
		const std::vector<int> values{random_values(rng, 2 + seed % 32, 16)};
		std::vector<bool> stays(values.size());
		for (size_t index{0}; index < stays.size(); ++index) { stays[index] = draw(rng, 3) != 0; }

		const int modulus{static_cast<int>(1 + draw(rng, 4))};
		const auto is_staying = [&](const int value) { return value % modulus != 0; };
		std::vector<int> filtered{values};
		filter(filtered, is_staying);
		std::vector<int> expected{values};
		std::erase_if(expected, [&](const int value) { return !is_staying(value); });
		CHECK(filtered == expected);

		std::vector<int> index_filtered{values};
		filter_indexes(index_filtered, [&](const size_t index) { return stays[index]; });
		expected.clear();
		for (size_t index{0}; index < values.size(); ++index) {
			if (stays[index]) { expected.push_back(values[index]); }
		}
		CHECK(index_filtered == expected);

		// Through OrdVector, whose elements stay sorted.
		const OrdVector<int> ord_vector{values};
		OrdVector<int> ord_filtered{ord_vector};
		ord_filtered.filter(is_staying);
		expected = ord_vector.to_vector();
		std::erase_if(expected, [&](const int value) { return !is_staying(value); });
		CHECK(ord_filtered.to_vector() == expected);

		OrdVector<int> ord_index_filtered{ord_vector};
		ord_index_filtered.filter_indexes([&](const size_t index) { return stays[index]; });
		expected.clear();
		for (size_t index{0}; index < ord_vector.size(); ++index) {
			if (stays[index]) { expected.push_back(ord_vector.at(index)); }
		}
		CHECK(ord_index_filtered.to_vector() == expected);
	}
}
