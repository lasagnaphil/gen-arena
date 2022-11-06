#pragma once

#include <utility> // needed for std::move and std::swap

#include <gen_arena_raw.h>

template <class T, class Config = GenArenaDefaultConfig>
class GenArenaTypedRef : public GenArenaRef<Config> {
};

template <class T, class Config = GenArenaDefaultConfig>
class GenArena {
private:
    GenArenaRaw<Config> _raw;

public:
    using Ref = GenArenaTypedRef<T, Config>;

    friend void swap(GenArena& a1, GenArena& a2) {
        using std::swap;

        swap(a1._raw, a2._raw);
    }

    GenArena() noexcept {
        // setup() with zero capacity guarantees it will succeed without any errors.
        GenArenaResult res = setup(0);
        (void) res;
    }

    ~GenArena() noexcept {
        // release() doesn't return any errors, so it's safe to do this in the destructor.
        release();
    }

    // We delete these copy constructors for a reason: they can't be used without exceptions.
    GenArena(const GenArena& other) = delete;

    GenArena& operator=(const GenArena& other) = delete;

    // Though we'll still implement move constructors since these won't have any potential errors.
    GenArena(GenArena&& other) noexcept {
        std::swap(*this, other);
    }

    GenArena& operator=(GenArena&& other) noexcept {
        std::swap(*this, other);
        return *this;
    }

    GenArenaResult setup(uint32_t capacity) {
        return _raw.setup(capacity, sizeof(T), alignof(T), gen_arena_type_id<T>());
    }

    GenArenaResult resize(uint32_t new_capacity) {
        return _raw.resize(new_capacity);
    }

    GenArenaResult shrink() {
        return _raw.shrink();
    }

    void release() {
        _raw.release();
    }

    uint32_t size() const { return _raw.size(); }

    uint32_t capacity() const { return _raw.capacity(); }

    const T* item_buf() const { return static_cast<const T*>(_raw.item_buf()); }

    T* item_buf() { return static_cast<T*>(_raw.item_buf()); }

#ifdef GEN_ARENA_DONT_RETURN_REF_PTR_PAIR
    Ref insert(const T& item, T** out_ptr = nullptr) {
        Ref ref;
        T* ptr;
        GenArenaResult res = _raw.insert_empty(ptr, ref);
        if (res == GenArenaResult::Ok) {
            new (ptr) T(item);
        }
        else {
            if (res == GenArenaResult::OutOfMemory) {
                gen_arena_log("GenArena error in insert(T&&): out of memory! (size = %d, capacity = %d)", size(), capacity());
            }
            else {
                gen_arena_log("GenArena error in insert(T&&): unknown");
            }
            ref.index = 0;
            ref.type_id = gen_arena_type_id<T>();
            ref.generation = 0;
            ptr = nullptr;
        }
        if (out_ptr) *out_ptr = ptr;
        return ref;
    }

    Ref insert(T&& item, T** out_ptr = nullptr) {
        Ref ref;
        T* ptr;
        GenArenaResult res = _raw.insert_empty(ptr, ref);
        if (res == GenArenaResult::Ok) {
            new (ptr) T(item);
        }
        else {
            if (res == GenArenaResult::OutOfMemory) {
                gen_arena_log("GenArena error in insert(T&&): out of memory! (size = %d, capacity = %d)", size(), capacity());
            }
            else {
                gen_arena_log("GenArena error in insert(T&&): unknown");
            }
            ref.index = 0;
            ref.type_id = gen_arena_type_id<T>();
            ref.generation = 0;
            ptr = nullptr;
        }
        if (out_ptr) *out_ptr = ptr;
        return ref;
    }
#else

    std::pair<Ref, T*> insert(const T& item) {
        Ref ref;
        void* ptr;
        GenArenaResult res = _raw.insert_empty(ptr, ref);
        if (res == GenArenaResult::Ok) {
            new(ptr) T(item);
        } else {
            if (res == GenArenaResult::OutOfMemory) {
                gen_arena_log("GenArena error in insert(T&&): out of memory! (size = %d, capacity = %d)", size(),
                              capacity());
            } else {
                gen_arena_log("GenArena error in insert(T&&): unknown");
            }
            ref.index = 0;
            ref.type_id = gen_arena_type_id<T>();
            ref.generation = 0;
            ptr = nullptr;
        }
        return {ref, static_cast<T*>(ptr)};
    }

    std::pair<Ref, T*> insert(T&& item) {
        Ref ref;
        void* ptr;
        GenArenaResult res = _raw.insert_empty(ptr, ref);
        if (res == GenArenaResult::Ok) {
            new(ptr) T(std::forward<T>(item));
        } else {
            if (res == GenArenaResult::OutOfMemory) {
                gen_arena_log("GenArena error in insert(T&&): out of memory! (size = %d, capacity = %d)", size(),
                              capacity());
            } else {
                gen_arena_log("GenArena error in insert(T&&): unknown");
            }
            ref.index = 0;
            ref.type_id = gen_arena_type_id<T>();
            ref.generation = 0;
            ptr = nullptr;
        }
        return {ref, static_cast<T*>(ptr)};
    }

#endif

    template <class... Args>
    std::pair<Ref, T*> emplace(Args&& ... args) {
        Ref ref;
        void* ptr;
        GenArenaResult res = _raw.insert_empty(ptr, ref);
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

    void release(Ref ref) {
        // Release with the destructor using a custom deleter lambda
        GenArenaResult res = _raw.release_with_deleter(ref, [](void* ptr) {
            static_cast<T*>(ptr)->~T();
        });

        if (res != GenArenaResult::Ok) {
            if (res == GenArenaResult::RefInvalid) {
                gen_arena_log("GenArena error in release(Ref): ref invalid! (index = %d, generation = %d)",
                              (uint32_t) ref.index, (uint32_t) ref.generation);
            } else {
                gen_arena_log("GenArena error in release(Ref): unknown");
            }
        }
    }

    bool is_valid_ref(Ref ref) const {
        return _raw.is_valid_ref(ref);
    }

    const T* get(Ref ref) const {
        return static_cast<const T*>(_raw.get(ref));
    }

    T* get(Ref ref) {
        return static_cast<T*>(_raw.get(ref));
    }

    const T* try_get(Ref ref) const {
        return static_cast<const T*>(_raw.try_get(ref));
    }

    T* try_get(Ref ref) {
        return static_cast<T*>(_raw.try_get(ref));
    }

    uint32_t get_item_idx(Ref ref) const {
        return _raw.get_item_idx(ref);
    }

    template <class Fun>
    void foreach(Fun&& fun) {
        T* items = static_cast<T*>(_raw.item_buf());
        for (uint32_t i = 0; i < _raw.size(); i++) {
            auto& val = items[i];
            fun(val);
        }
    }

    template <class Fun>
    void foreach_with_ref(Fun&& fun) {
        T* items = static_cast<T*>(_raw.item_buf());
        GenArenaMetadata* metadata = _raw.metadata_buf();
        auto* free_list = _raw.free_list_buf();
        for (uint32_t i = 0; i < _raw.size(); i++) {
            uint32_t index = metadata[i].dense_to_sparse;
            Ref ref;
            ref.index = index;
            ref.type_id = gen_arena_type_id<T>();
            ref.generation = free_list[index].generation;
            auto& val = items[i];
            fun(ref, val);
        }
    }
};
