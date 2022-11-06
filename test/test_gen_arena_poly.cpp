#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#include "doctest.h"

#define GEN_ARENA_USE_POLYMORPHIC_TYPES
#include <gen_arena_poly.h>

#include <tuple>

class A {
public:
    uint32_t x, y;

    A(uint32_t value) : x(value), y(value) {}
};

class B : public A {
public:
    uint32_t z;

    B(uint32_t value, uint32_t valueA) : z(value), A(valueA) {}
};

class C : public B {
public:
    uint32_t w;

    C(uint32_t value, uint32_t valueB, uint32_t valueA) : w(value), B(valueB, valueA) {}
};

class D : public A {
public:
    float f;

    D(float value, uint32_t valueA) : f(value), A(valueA) {}
};

enum Type : uint8_t {
    TYPE_A,
        TYPE_B,
            TYPE_C,
        TYPE_B_LAST,
        TYPE_D,
    TYPE_A_LAST
};

#define DECLARE_TYPE_ID(T, TYPE_ID, SUBTYPE_ID_LAST) \
template <> inline constexpr uint32_t gen_arena_type_id<T>() { return TYPE_ID; } \
template <> inline constexpr uint32_t gen_arena_subtype_id_end<T>() { return SUBTYPE_ID_LAST; }

DECLARE_TYPE_ID(A, TYPE_A, TYPE_A_LAST)
DECLARE_TYPE_ID(B, TYPE_B, TYPE_B_LAST)
DECLARE_TYPE_ID(C, TYPE_C, TYPE_C)
DECLARE_TYPE_ID(D, TYPE_D, TYPE_D)

template <class T>
using Ref = GenArenaPoly<>::Ref<T>;

TEST_CASE("gen_arena_poly_simple_test") {
    GenArenaPoly<> arena(TYPE_A_LAST);

    // Setting each arena up with different capacities, to test resize capability.
    arena.setup<A>(2);
    arena.setup<B>(4);
    arena.setup<C>(8);
    arena.setup<D>(16);

    for (uint32_t i = 0; i < 10; i++) {
        auto [refA, ptrA] = arena.emplace<A>(i);
        REQUIRE(ptrA->x == i);
        REQUIRE(ptrA->y == i);
    }

    for (uint32_t i = 0; i < 10; i++) {
        auto [refB, ptrB] = arena.emplace<B>(10 + i, i);
        REQUIRE(ptrB->x == i);
        REQUIRE(ptrB->y == i);
        REQUIRE(ptrB->z == 10 + i);
    }

    for (uint32_t i = 0; i < 10; i++) {
        auto [refC, ptrC] = arena.emplace<C>(20 + i, 10 + i, i);
        REQUIRE(ptrC->x == i);
        REQUIRE(ptrC->y == i);
        REQUIRE(ptrC->z == 10 + i);
        REQUIRE(ptrC->w == 20 + i);
    }

    for (uint32_t i = 0; i < 10; i++) {
        auto [refD, ptrD] = arena.emplace<D>(30 + i, i);
        REQUIRE(ptrD->x == i);
        REQUIRE(ptrD->f == 30 + i);
    }

    arena.foreach<A>([](A& a) {
        CHECK_LT(a.x, 10);
        CHECK_LT(a.y, 10);
    });

    arena.foreach<B>([](B& b) {
        CHECK_LT(b.x, 10);
        CHECK_LT(b.y, 10);

        CHECK_GE(b.z, 10);
        CHECK_LT(b.z, 20);
    });

    arena.foreach<C>([](C& c) {
        CHECK_LT(c.x, 10);
        CHECK_LT(c.y, 10);

        CHECK_GE(c.z, 10);
        CHECK_LT(c.z, 20);

        CHECK_GE(c.w, 20);
        CHECK_LT(c.w, 30);
    });

    arena.foreach<D>([](D& d) {
        CHECK_LT(d.x, 10);
        CHECK_LT(d.y, 10);

        CHECK_GE(d.f, 30);
        CHECK_LT(d.f, 40);
    });
}