#pragma once

/**
 * A generational arena.
 * The container isn't templated with the item type (uses void*), 
 * and item type size/alignment can be specified at runtime.
 * Implemented without any dependencies on the STL (only imports <stdint.h>, <stdlib.h>, and optionally <assert.h>)
 */

#include "gen_arena_config.h"

// TODO: Should this function go to gen_arena_config.h instead?
#ifdef GEN_ARENA_FORCE_DECLARE_TYPE_ID_FUN

template <class T>
constexpr uint32_t gen_arena_type_id();

#ifdef GEN_ARENA_USE_POLYMORPHIC_TYPES
template <class T>
constexpr uint32_t gen_arena_subtype_id_end();
#endif

#else

template <class T>
constexpr uint32_t gen_arena_type_id() { return UINT32_MAX; }

#ifdef GEN_ARENA_USE_POLYMORPHIC_TYPES
template <class T>
constexpr uint32_t gen_arena_subtype_id_end() { return UINT32_MAX; }
#endif

#endif

template <class T>
inline T* gen_arena_new_array(size_t size) {
    return static_cast<T*>(gen_arena_aligned_alloc(sizeof(T) * size, alignof(T)));
}

template <class T>
inline void gen_arena_delete_array(T* ptr) {
    gen_arena_aligned_free(ptr);
}

template <int index_bits,
        int typeid_bits,
        int generation_bits>
struct GenArenaConfig {
    static constexpr int IndexBits = index_bits;
    static constexpr int TypeIdBits = typeid_bits;
    static constexpr int GenerationBits = generation_bits;
};

using GenArenaDefaultConfig = GenArenaConfig<32, 8, 24>;

enum class GenArenaResult {
    Ok,
    OutOfMemory,
    OutOfVirtualAllocMemory,
    ResizeInvalid,
    RefInvalid,
};


struct GenArenaMetadata {
    uint32_t dense_to_sparse;
};

template <class Config = GenArenaDefaultConfig>
struct GenArenaRef {
    uint32_t index: Config::IndexBits;
    uint32_t type_id: Config::TypeIdBits;
    uint32_t generation: Config::GenerationBits;
};

template <class Config>
class GenArenaRaw {
public:
    using Ref = GenArenaRef<Config>;

private:
    static constexpr uint32_t NIL = 0xffffffff;

    void* _items;
    GenArenaMetadata* _metadata;
    Ref* _free_list;

    uint32_t _item_size;
    uint32_t _free_list_size;
    uint32_t _capacity;

    uint32_t _tid;

    uint32_t _tsize;
    uint32_t _talign;

    uint32_t _free_list_front;
    uint32_t _free_list_back;

public:
    GenArenaResult setup(uint32_t initial_capacity, uint32_t tsize, uint32_t talign, uint32_t tid) {
        _item_size = 0;
        _free_list_size = 0;
        _capacity = initial_capacity;

        _tid = tid;

        _tsize = tsize;
        _talign = talign;

        _free_list_front = NIL;
        _free_list_back = NIL;

        if (initial_capacity == 0) {
            _items = nullptr;
            _metadata = nullptr;
            _free_list = nullptr;
        } else {
            _items = gen_arena_aligned_alloc(_tsize * _capacity, _talign);
            if (_items == nullptr) return GenArenaResult::OutOfMemory;

            _metadata = gen_arena_new_array<GenArenaMetadata>(_capacity);
            if (_metadata == nullptr) return GenArenaResult::OutOfMemory;

            _free_list = gen_arena_new_array<Ref>(_capacity);
            if (_free_list == nullptr) return GenArenaResult::OutOfMemory;
        }
        return GenArenaResult::Ok;
    }

    void release() {
        gen_arena_aligned_free(_items);
        gen_arena_aligned_free(_free_list);
        gen_arena_aligned_free(_metadata);

        _items = nullptr;
        _free_list = nullptr;
        _metadata = nullptr;

        _item_size = 0;
        _free_list_size = 0;
        _capacity = 0;

        _free_list_front = NIL;
        _free_list_back = NIL;
    }

    uint32_t size() const { return _item_size; }

    uint32_t free_list_size() const { return _free_list_size; }

    uint32_t capacity() const { return _capacity; }

    uint32_t type_id() const { return _tid; }

    uint32_t type_size() const { return _tsize; }

    uint32_t type_alignment() const { return _talign; }

    const void* item_buf() const { return _items; }

    void* item_buf() { return _items; }

    const GenArenaMetadata* metadata_buf() const { return _metadata; }

    GenArenaMetadata* metadata_buf() { return _metadata; }

    const Ref* free_list_buf() const { return _free_list; }

    Ref* free_list_buf() { return _free_list; }

    GenArenaResult resize(uint32_t new_capacity) {
        if (new_capacity < _item_size) {
            return GenArenaResult::ResizeInvalid;
        }

        void* new_items = gen_arena_aligned_alloc(_tsize * new_capacity, _talign);
        if (new_items == nullptr) return GenArenaResult::OutOfMemory;

        GenArenaMetadata* new_metadata = gen_arena_new_array<GenArenaMetadata>(new_capacity);
        if (new_metadata == nullptr) return GenArenaResult::OutOfMemory;

        Ref* new_free_list = gen_arena_new_array<Ref>(new_capacity);
        if (new_free_list == nullptr) return GenArenaResult::OutOfMemory;

        memcpy(new_items, _items, _tsize * _item_size);
        memcpy(new_metadata, _metadata, sizeof(GenArenaMetadata) * _item_size);
        memcpy(new_free_list, _free_list, sizeof(Ref) * _free_list_size);

        gen_arena_aligned_free(_items);
        gen_arena_delete_array(_metadata);
        gen_arena_delete_array(_free_list);

        _items = new_items;
        _metadata = new_metadata;
        _free_list = new_free_list;
        _capacity = new_capacity;

        return GenArenaResult::Ok;
    }

    // Shrink buffers to nearest power-of-two capacity.
    GenArenaResult shrink() {
        // We really don't need to shrink when current item size is this small.
        // Besides we need to make sure gen_arena_clz(0) doesn't produce undefined behavior.
        if (_item_size <= 1) return GenArenaResult::Ok;

        // Nearest power-of-two calculation using clz() operation
        uint32_t new_capacity = 1 << (32 - gen_arena_clz(_item_size - 1));

        void* new_items = gen_arena_aligned_alloc(_tsize * new_capacity, _talign);
        if (new_items == nullptr) return GenArenaResult::OutOfMemory;

        GenArenaMetadata* new_metadata = gen_arena_new_array<GenArenaMetadata>(new_capacity);
        if (new_metadata == nullptr) return GenArenaResult::OutOfMemory;

        memcpy(new_items, _items, _tsize * _item_size);
        memcpy(new_metadata, _metadata, sizeof(GenArenaMetadata) * _item_size);

        gen_arena_aligned_free(_items);
        gen_arena_delete_array(_metadata);

        _items = new_items;
        _metadata = new_metadata;
        _capacity = new_capacity;

        return GenArenaResult::Ok;
    }

    GenArenaResult insert_empty(void*& new_item_addr, Ref& ref, uint32_t userdata = 0) {
        if (_free_list_front == NIL) {
                    gen_arena_assert(_item_size == _free_list_size);
            ref = {_free_list_size, _tid, 1};
            if (_free_list_size == _capacity) {
                if (resize(_capacity == 0 ? 1 : 2 * _capacity) == GenArenaResult::OutOfMemory) {
                    return GenArenaResult::OutOfMemory;
                }
            }
            _free_list[_free_list_size++] = ref;
        } else {
            Ref& node = _free_list[_free_list_front];
            uint32_t new_index = _free_list_front;
            _free_list_front = node.index;
            if (_free_list_front == NIL) {
                _free_list_back = NIL;
            }
            node.index = _item_size;
            ref = {new_index, _tid, node.generation};
        }

        // Insert to item buffer
        // Note that we don't need to check if we need to grow the buffer, this has already been done above
        new_item_addr = static_cast<unsigned char*>(_items) + _tsize * _item_size;

        // Insert to metadata buffer
        _metadata[_item_size].dense_to_sparse = ref.index;

        _item_size++;

        // Return the results
        return GenArenaResult::Ok;
    }

    GenArenaResult insert(const void* item_addr, void*& new_item_addr, Ref& ref, uint32_t userdata = 0) {
        GenArenaResult res = insert_empty(new_item_addr, ref, userdata);
        memcpy(new_item_addr, item_addr, _tsize);
        return res;
    }

    GenArenaResult release(Ref ref) {
        // TODO: check if this gets inlined well in all major compilers
        return release_with_deleter(ref, [](void* ptr) {});
    }

    template <class Deleter>
    GenArenaResult release_with_deleter(Ref ref, Deleter&& deleter_fun) {
        if (ref.index >= _free_list_size) return GenArenaResult::RefInvalid;

        Ref& node = _free_list[ref.index];

        if (node.index >= _item_size || node.generation != ref.generation) return GenArenaResult::RefInvalid;

        uint32_t prev_index = node.index;
        node.index = NIL;
        node.generation++;

        // Free list deletion
        if (_free_list_front == NIL) {
            // If free list is empty, create new free list with one element
            _free_list_back = _free_list_front = ref.index;
        } else {
            // Else, insert like what you would do with a singly-linked list
            _free_list[_free_list_back].index = ref.index;
            _free_list_back = ref.index;
        }
        _free_list[_free_list_back].index = NIL;

        // If the item to release in the buffer isn't at the end,
        // Do a remove-swap operation to remove it. (Both the item and its metadata)
        if (prev_index != _item_size - 1) {
            // Before overriding the would-be-deleted item, call the custom deleter function.
            deleter_fun(static_cast<char*>(_items) + _tsize * prev_index);

            // Move the last item to the empty slot
            memmove(static_cast<char*>(_items) + _tsize * prev_index,
                    static_cast<char*>(_items) + _tsize * (_item_size - 1), _tsize);

            // Also do this for metadata buffer
            _metadata[prev_index] = _metadata[_item_size - 1];

            // Don't forget to update the free list for the swapped item!
            _free_list[_metadata[prev_index].dense_to_sparse].index = prev_index;
        }

        _item_size--;

        return GenArenaResult::Ok;
    }

    bool is_valid_ref(Ref ref) const {
        if (ref.index >= _free_list_size) return false;
        auto node = _free_list[ref.index];
        return node.index < _item_size && node.generation == ref.generation;
    }

    const void* get(Ref ref) const {
                gen_arena_assert(ref.index < _free_list_size);
        auto node = _free_list[ref.index];
                gen_arena_assert(node.generation == ref.generation);
                gen_arena_assert(node.index < _item_size);

        return static_cast<void*>(static_cast<char*>(_items) + _tsize * node.index);
    }

    void* get(Ref ref) {
        return const_cast<void*>(const_cast<const GenArenaRaw<Config>*>(this)->get(ref));
    }

    const void* try_get(Ref ref) const {
        auto node = _free_list[ref.index];

        if (node.generation == ref.generation) {
            return static_cast<void*>(static_cast<char*>(_items) + _tsize * node.index);
        } else {
            return nullptr;
        }
    }

    void* try_get(Ref ref) {
        return const_cast<void*>(const_cast<const GenArenaRaw<Config>*>(this)->try_get(ref));
    }

    uint32_t get_item_idx(Ref ref) const {
                gen_arena_assert(ref.index < _free_list_size);
        auto node = _free_list[ref.index];
                gen_arena_assert(node.generation == ref.generation);
                gen_arena_assert(node.index < _item_size);

        return node.index;
    }
};
