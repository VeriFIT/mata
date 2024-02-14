// Automaton A
#define FILL_WITH_AUT_A(x) \
    x.initial = {1, 3}; \
	x.final = {5}; \
	x.delta.add(1, 'a', 3); \
	x.delta.add(1, 'a', 10); \
	x.delta.add(1, 'b', 7); \
	x.delta.add(3, 'a', 7); \
	x.delta.add(3, 'b', 9); \
	x.delta.add(9, 'a', 9); \
	x.delta.add(7, 'b', 1); \
	x.delta.add(7, 'a', 3); \
	x.delta.add(7, 'c', 3); \
	x.delta.add(10, 'a', 7); \
	x.delta.add(10, 'b', 7); \
	x.delta.add(10, 'c', 7); \
	x.delta.add(7, 'a', 5); \
	x.delta.add(5, 'a', 5); \
	x.delta.add(5, 'c', 9); \

// Automaton B
#define FILL_WITH_AUT_B(x) \
	x.initial = {4}; \
	x.final = {2, 12}; \
	x.delta.add(4, 'c', 8); \
	x.delta.add(4, 'a', 8); \
	x.delta.add(8, 'b', 4); \
	x.delta.add(4, 'a', 6); \
	x.delta.add(4, 'b', 6); \
	x.delta.add(6, 'a', 2); \
	x.delta.add(2, 'b', 2); \
	x.delta.add(2, 'a', 0); \
	x.delta.add(0, 'a', 2); \
	x.delta.add(2, 'c', 12); \
	x.delta.add(12, 'a', 14); \
	x.delta.add(14, 'b', 12); \

// Automaton C
// the same as B, but with small symbols
#define FILL_WITH_AUT_C(x) \
	x.initial = {4}; \
	x.final = {2, 12}; \
	x.delta.add(4, 3, 8); \
	x.delta.add(4, 1, 8); \
	x.delta.add(8, 2, 4); \
	x.delta.add(4, 1, 6); \
	x.delta.add(4, 2, 6); \
	x.delta.add(6, 1, 2); \
	x.delta.add(2, 2, 2); \
	x.delta.add(2, 1, 0); \
	x.delta.add(0, 1, 2); \
	x.delta.add(2, 3, 12); \
	x.delta.add(12, 1, 14); \
	x.delta.add(14, 2, 12);   \

// Automaton D // shomewhat larger
#define FILL_WITH_AUT_D(x) \
    x.initial = {0}; \
    x.final = {3}; \
    x.delta.add(0, 46, 0); \
    x.delta.add(0, 47, 0); \
    x.delta.add(0, 58, 0); \
    x.delta.add(0, 58, 1); \
    x.delta.add(0, 64, 1); \
    x.delta.add(0, 64, 1); \
    x.delta.add(0, 82, 2); \
    x.delta.add(0, 92, 2); \
    x.delta.add(0, 98, 2); \
    x.delta.add(0, 100, 1);\
    x.delta.add(0, 103, 0);\
    x.delta.add(0, 109, 0);\
    x.delta.add(0, 110, 1);\
    x.delta.add(0, 111, 2);\
    x.delta.add(0, 114, 0);\
    x.delta.add(1, 47, 2); \
    x.delta.add(2, 47, 3); \
    x.delta.add(3, 46, 2); \
    x.delta.add(3, 47, 0); \
    x.delta.add(3, 58, 2); \
    x.delta.add(3, 64, 3); \
    x.delta.add(3, 82, 2); \
    x.delta.add(3, 92, 0); \
    x.delta.add(3, 98, 2); \
    x.delta.add(3, 100, 2);\
    x.delta.add(3, 103, 3);\
    x.delta.add(3, 109, 2);\
    x.delta.add(3, 110, 3);\
    x.delta.add(3, 111, 2);\
    x.delta.add(3, 114, 3);\

// Automaton E
#define FILL_WITH_AUT_E(x) \
	x.initial = {1, 3}; \
	x.final = {4}; \
	x.delta.add(1, 'b', 2); \
	x.delta.add(2, 'a', 4); \
	x.delta.add(1, 'a', 3); \

