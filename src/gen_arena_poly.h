#pragma once

#include <utility>

#include <gen_arena_raw.h>

template <class T, class Config = GenArenaDefaultConfig>
class GenArenaTypedPolyRef : public GenArenaRef<Config> {
};

template <class Config = GenArenaDefaultConfig>
class GenArenaPoly
{
private:
    GenArenaRaw<Config>* _arenas;
    uint32_t _max_num_types;

public:
    template <class T>
    using Ref = GenArenaTypedPolyRef<T, Config>;

    friend void swap(GenArenaPoly& a1, GenArenaPoly& a2) {
        using std::swap;
        swap(a1._arenas, a2._arenas);
        swap(a1._max_num_types, a2._max_num_types);
    }

    GenArenaPoly(uint32_t max_num_types) noexcept {
        _arenas = static_cast<GenArenaRaw<Config>*>(
                gen_arena_aligned_alloc(sizeof(GenArenaRaw<Config>) * max_num_types, alignof(GenArenaRaw<Config>)));
        _max_num_types = max_num_types;
    }

    ~GenArenaPoly() noexcept {
        if (_arenas) gen_arena_aligned_free(_arenas);
    }

    GenArenaPoly(const GenArenaPoly& other) = delete;
    GenArenaPoly& operator=(const GenArenaPoly& other) = delete;

    GenArenaPoly(GenArenaPoly&& other) noexcept {
        std::swap(*this, other);
    }

    GenArenaPoly& operator=(GenArenaPoly&& other) noexcept {
        std::swap(*this, other);
        return *this;
    }

    template <class T>
    GenArenaResult setup(uint32_t capacity) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].setup(capacity, sizeof(T), alignof(T), tid);
    }

    template <class T>
    GenArenaResult resize(uint32_t new_capacity) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].resize(new_capacity);
    }

    template <class T>
    GenArenaResult shrink() {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].shrink();
    }

    template <class T>
    void release() {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].release();
    }

    template <class T>
    uint32_t size() const {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].size();
    }

    template <class T>
    uint32_t capacity() const {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].capacity();
    }

    template <class T>
    const T* item_buf() const {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].item_buf();
    }

    template <class T>
    T* item_buf() {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].item_buf();
    }

    template <class T>
    std::pair<Ref<T>, T*> insert(const T& item) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        Ref<T> ref;
        void* ptr;
        GenArenaResult res = _arenas[tid].insert_empty(ptr, ref);
        if (res == GenArenaResult::Ok) {
            new (ptr) T(item);
        }
        else {
            if (res == GenArenaResult::OutOfMemory) {
                gen_arena_log("GenArena error in insert(const T&): out of memory! (tid = %d, size = %d, capacity = %d)", tid, size(), capacity());
            }
            else {
                gen_arena_log("GenArena error in insert(const T&): unknown");
            }
            ref.index = 0;
            ref.type_id = tid;
            ref.generation = 0;
            ptr = nullptr;
        }
        return {ref, static_cast<T*>(ptr)};
    }

    template <class T>
    std::pair<Ref<T>, T*> insert(T&& item) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        Ref<T> ref;
        void* ptr;
        GenArenaResult res = _arenas[tid].insert_empty(ptr, ref);
        if (res == GenArenaResult::Ok) {
            new (ptr) T(std::forward<T>(item));
        }
        else {
            if (res == GenArenaResult::OutOfMemory) {
                gen_arena_log("GenArena error in insert(const T&): out of memory! (tid = %d, size = %d, capacity = %d)", tid, size(), capacity());
            }
            else {
                gen_arena_log("GenArena error in insert(const T&): unknown");
            }
            ref.index = 0;
            ref.type_id = tid;
            ref.generation = 0;
            ptr = nullptr;
        }
        return {ref, static_cast<T*>(ptr)};
    }

    template <class T, class... Args>
    std::pair<Ref<T>, T*> emplace(Args&&... args) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        Ref<T> ref;
        void* ptr;
        GenArenaResult res = _arenas[tid].insert_empty(ptr, ref);
        if (res == GenArenaResult::Ok) {
            new(ptr) T(args...);
        } else {
            if (res == GenArenaResult::OutOfMemory) {
                gen_arena_log("GenArena error in emplace(...): out of memory! (size = %d, capacity = %d)", size(),
                              capacity());
            } else {
                gen_arena_log("GenArena error in emplace(...): unknown");
            }
            ref.index = 0;
            ref.type_id = gen_arena_type_id<T>();
            ref.generation = 0;
            ptr = nullptr;
        }
        return {ref, static_cast<T*>(ptr)};
    }

    template <class T>
    void release(Ref<T> ref) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        GenArenaResult res = _arenas[tid].release_with_deleter(ref, [](void* ptr) {
            static_cast<T*>(ptr)->~T();
        });

        if (res != GenArenaResult::Ok) {
            if (res == GenArenaResult::RefInvalid) {
                gen_arena_log("GenArena error in release(Ref): ref invalid! (tid = %d, index = %d, generation %d)",
                              tid, (uint32_t) ref.index, (uint32_t) ref.generation);
            }
            else {
                gen_arena_log("GenArena error in release(Ref): unknown");
            }
        }
    }

    template <class T>
    bool is_valid_ref(Ref<T> ref) const {
#ifdef GEN_ARENA_USE_POLYMORPHIC_TYPES
        gen_arena_assert(ref.type_id < _max_num_types);
        return _arenas[ref.type_id].is_valid_ref(ref);
#else
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].is_valid_ref(ref);
#endif
    }

    template <class T>
    const T* get(Ref<T> ref) const {
#ifdef GEN_ARENA_USE_POLYMORPHIC_TYPES
        gen_arena_assert(ref.type_id < _max_num_types);
        return static_cast<const T*>(_arenas[ref.type_id].get(ref));
#else
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return static_cast<const T*>(_arenas[tid].get(ref));
#endif
    }

    template <class T>
    T* get(Ref<T> ref) {
#ifdef GEN_ARENA_USE_POLYMORPHIC_TYPES
        gen_arena_assert(ref.type_id < _max_num_types);
        return static_cast<T*>(_arenas[ref.type_id].get(ref));
#else
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return static_cast<T*>(_arenas[tid].get(ref));
#endif
    }

    template <class T>
    const T* try_get(Ref<T> ref) const {
#ifdef GEN_ARENA_USE_POLYMORPHIC_TYPES
        gen_arena_assert(ref.type_id < _max_num_types);
        return static_cast<const T*>(_arenas[ref.type_id].get(ref));
#else
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return static_cast<const T*>(_arenas[tid].get(ref));
#endif
    }

    template <class T>
    T* try_get(Ref<T> ref) {
#ifdef GEN_ARENA_USE_POLYMORPHIC_TYPES
        gen_arena_assert(ref.type_id < _max_num_types);
        return static_cast<const T*>(_arenas[ref.type_id].get(ref));
#else
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return static_cast<const T*>(_arenas[tid].get(ref));
#endif
    }

    template <class T>
    uint32_t get_item_idx(Ref<T> ref) const {
#ifdef GEN_ARENA_USE_POLYMORPHIC_TYPES
        gen_arena_assert(ref.type_id < _max_num_types);
        return _arenas[ref.type_id].get_item_idx(ref);
#else
        constexpr uint32_t tid = gen_arena_type_id<T>();
        gen_arena_assert(tid < _max_num_types);
        return _arenas[tid].get_item_idx(ref);
#endif
    }

    template <class T, class Fun>
    void foreach(Fun&& fun) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        auto& arena = _arenas[tid];
        T* items = static_cast<T*>(arena.item_buf());
        for (uint32_t i = 0; i < arena.size(); i++) {
            auto& val = items[i];
            fun(val);
        }
    }

    template <class T, class Fun>
    void foreach_with_ref(Fun&& fun) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        auto& arena = _arenas[tid];
        T* items = static_cast<T*>(arena.item_buf());
        GenArenaMetadata* metadata = arena.metadata_buf();
        auto* free_list = arena.free_list_buf();
        for (uint32_t i = 0; i < arena.size(); i++) {
            uint32_t index = metadata[i].dense_to_sparse;
            Ref<T> ref;
            ref.index = index;
            ref.type_id = tid;
            ref.generation = free_list[index].generation;
            auto& val = items[i];
            fun(ref, val);
        }
    }

#ifdef GEN_ARENA_USE_POLYMORPHIC_TYPES
    template <class T, class Fun>
    void foreach_val_poly(Fun&& fun) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        constexpr uint32_t tid_child_begin = gen_arena_subtype_id_begin<T>();
        constexpr uint32_t tid_child_end = gen_arena_subtype_id_end<T>();
        for (uint32_t cur_tid = tid_child_begin; cur_tid < tid_child_end; cur_tid++) {
            auto& arena = _arenas[cur_tid];
            T* items = static_cast<T*>(arena.item_buf());
            for (uint32_t i = 0; i < arena.size(); i++) {
                auto& val = items[i];
                fun(val);
            }
        }
    }
    template <class T, class Fun>
    void foreach_ref_val_poly(Fun&& fun) {
        constexpr uint32_t tid = gen_arena_type_id<T>();
        constexpr uint32_t tid_child_begin = gen_arena_subtype_id_begin<T>();
        constexpr uint32_t tid_child_end = gen_arena_subtype_id_end<T>();
        for (uint32_t cur_tid = tid_child_begin; cur_tid < tid_child_end; cur_tid++) {
            auto& arena = _arenas[cur_tid];
            T* items = static_cast<T*>(arena.item_buf());
            GenArenaMetadata* metadata = arena.metadata_buf();
            auto* free_list = arena.free_list_buf();
            for (uint32_t i = 0; i < arena.size(); i++) {
                uint32_t index = metadata[i].dense_to_sparse;
                Ref<T> ref;
                ref.index = index;
                ref.type_id = tid;
                ref.generation = free_list[index].generation;
                auto& val = items[i];
                fun(val);
            }
        }
    }
#endif
};