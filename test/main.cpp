#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

#include <gen_arena.h>

#include <array>
#include <random>

struct Obj {
    uint32_t a, b, c, d;

    static constexpr uint32_t type_id() { return 0; }

    Obj() = default;

    Obj(uint32_t v) : a(v), b(v), c(v), d(v) {}

    void set(uint32_t v) {
        a = v;
        b = v;
        c = v;
        d = v;
    }

    bool operator==(const Obj& other) const {
        return a == other.a && b == other.b && c == other.c && d == other.d;
    }
};

template <>
inline constexpr uint32_t gen_arena_type_id<Obj>() { return 1; }

TEST_CASE("gen_arena_simple_test") {
    GenArena<Obj> arena;
    arena.setup(2);
    REQUIRE(arena.size() == 0);

    GenArena<Obj>::Ref a, b, c, d;
    Obj* a_ptr, * b_ptr, * c_ptr, * d_ptr;

    std::tie(a, a_ptr) = arena.insert(Obj(1));
    REQUIRE(arena.size() == 1);
    std::tie(b, b_ptr) = arena.insert(Obj(2));
    REQUIRE(arena.size() == 2);
    std::tie(c, c_ptr) = arena.insert(Obj(3));
    arena.get(b)->set(4);

    REQUIRE(arena.size() == 3);
    REQUIRE(*arena.get(a) == Obj(1));
    REQUIRE(*arena.get(b) == Obj(4));
    REQUIRE(*arena.get(c) == Obj(3));

    arena.foreach_ref([&](GenArena<Obj>::Ref ref) {
        REQUIRE(arena.get(ref) != nullptr);
    });

    {
        std::array<bool, 5> occupied = {false, false, false, false, false};
        arena.foreach_val([&](Obj& value) {
            bool in_bounds = value.a >= 0 && value.a <= 4;
            CHECK(in_bounds);
            occupied[value.a] = true;
        });
        REQUIRE(occupied[0] == false);
        REQUIRE(occupied[1] == true);
        REQUIRE(occupied[2] == false);
        REQUIRE(occupied[3] == true);
        REQUIRE(occupied[4] == true);
    }

    arena.foreach_ref_val([&](GenArena<Obj>::Ref ref, Obj& value) {
        REQUIRE(*arena.get(ref) == value);
    });

    arena.release(b);
    REQUIRE(arena.size() == 2);
    REQUIRE(*arena.get(a) == Obj(1));
    REQUIRE(arena.try_get(b) == nullptr);
    REQUIRE(*arena.get(c) == Obj(3));

    arena.release(a);
    REQUIRE(arena.size() == 1);
    REQUIRE(arena.try_get(a) == nullptr);
    REQUIRE(arena.try_get(b) == nullptr);
    REQUIRE(*arena.get(c) == Obj(3));

    std::tie(d, d_ptr) = arena.insert(5);
    REQUIRE(arena.size() == 2);
    REQUIRE(arena.try_get(a) == nullptr);
    REQUIRE(arena.try_get(b) == nullptr);
    REQUIRE(*arena.get(c) == Obj(3));
    REQUIRE(*arena.get(d) == Obj(5));

    arena.shrink();
    REQUIRE(arena.size() == 2);
    REQUIRE(arena.capacity() == 2);
    REQUIRE(arena.try_get(a) == nullptr);
    REQUIRE(arena.try_get(b) == nullptr);
    REQUIRE(*arena.get(c) == Obj(3));
    REQUIRE(*arena.get(d) == Obj(5));
}

TEST_CASE("gen_arena_complex_test_1") {
    const uint32_t test_size = 1024;
    const uint32_t delete_size = test_size / 2;
    auto arena = GenArena<Obj>();
    arena.setup(test_size);
    std::vector<GenArena<Obj>::Ref> refs(test_size);
    for (uint32_t i = 0; i < test_size; i++) {
        Obj* ptr;
        std::tie(refs[i], ptr) = arena.emplace(i);
    }
    CHECK(arena.size() == test_size);
    std::vector<uint32_t> shuffled(test_size);
    for (uint32_t i = 0; i < test_size; i++) {
        shuffled[i] = i;
    }
    std::shuffle(shuffled.begin(), shuffled.end(), std::mt19937(std::random_device()()));
    for (uint32_t i = 0; i < delete_size; i++) {
        arena.release(refs[shuffled[i]]);
    }
    CHECK(arena.size() == test_size - delete_size);

    for (uint32_t i = 0; i < delete_size; i++) {
        CHECK(!arena.is_valid_ref(refs[shuffled[i]]));
    }
    for (uint32_t i = delete_size; i < test_size; i++) {
        CHECK(arena.is_valid_ref(refs[shuffled[i]]));
        CHECK(*arena.get(refs[shuffled[i]]) == shuffled[i]);
    }

    std::vector<GenArena<Obj>::Ref> new_ids(delete_size);
    for (uint32_t i = 0; i < delete_size; i++) {
        Obj* ptr;
        std::tie(new_ids[i], ptr) = arena.emplace(shuffled[i]);
    }
    CHECK(arena.size() == test_size);
    for (int i = 0; i < delete_size; i++) {
        CHECK(arena.is_valid_ref(new_ids[i]));
        CHECK(*arena.get(new_ids[i]) == shuffled[i]);
    }
    for (int i = delete_size; i < test_size; i++) {
        CHECK(arena.is_valid_ref(refs[shuffled[i]]));
        CHECK(*arena.get(refs[shuffled[i]]) == shuffled[i]);
    }
}

TEST_CASE("gen_arena_complex_test_2") {
    uint32_t iter = 0;
    const uint32_t test_size = 1024;
    auto arena = GenArena<Obj>();
    arena.setup(test_size);
    std::vector<GenArena<Obj>::Ref> refs(test_size);
    for (uint32_t i = 0; i < test_size; i++) {
        Obj* ptr;
        std::tie(refs[i], ptr) = arena.emplace(iter);
    }
    CHECK(arena.size() == test_size);

    for (iter = 1; iter <= 10; iter++) {
        // MESSAGE("Iter ", iter);
        std::shuffle(refs.begin(), refs.end(), std::mt19937(std::random_device()()));
        const uint32_t delete_size = refs.size() / 2;
        std::vector<GenArena<Obj>::Ref> deleted(refs.end() - delete_size, refs.end());
        for (auto id: deleted) {
            arena.release(id);
        }
        for (auto id: deleted) {
            CHECK(!arena.is_valid_ref(id));
        }
        refs.resize(refs.size() - delete_size);
        for (auto id: refs) {
            CHECK(arena.is_valid_ref(id));
        }
        CHECK(arena.size() == refs.size());

        const uint32_t add_size = refs.size() / 4;
        std::vector<GenArena<Obj>::Ref> new_ids(add_size);
        for (uint32_t i = 0; i < add_size; i++) {
            Obj* ptr;
            std::tie(new_ids[i], ptr) = arena.emplace(iter);
            refs.push_back(new_ids[i]);
        }
        CHECK(arena.size() == refs.size());
        for (auto id: new_ids) {
            CHECK(arena.is_valid_ref(id));
            CHECK(*arena.get(id) == iter);
        }
    }
}
