#pragma once

#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

#include "ops/eq.h"
#include "ops/hash.h"
#include "ops/ptr.h"
#include "alloc/alloc.h"

#define HASH_MAP_NAME(name, KEY, VALUE) name##_##KEY##_##VALUE

#define HashMap(KEY, VALUE) HASH_MAP_NAME(HashMap, KEY, VALUE)
#define HashMapEntry(KEY, VALUE) HASH_MAP_NAME(HashMapEntry, KEY, VALUE)

#define hash_map_new(KEY, VALUE) HASH_MAP_NAME(hash_map_new, KEY, VALUE)
#define hash_map_free(KEY, VALUE) HASH_MAP_NAME(hash_map_free, KEY, VALUE)

#define hash_map_get(KEY, VALUE) HASH_MAP_NAME(hash_map_get, KEY, VALUE)
#define hash_map_get_mut(KEY, VALUE) HASH_MAP_NAME(hash_map_get_mut, KEY, VALUE)
#define hash_map_insert(KEY, VALUE) HASH_MAP_NAME(hash_map_insert, KEY, VALUE)
#define hash_map_remove(KEY, VALUE) HASH_MAP_NAME(hash_map_remove, KEY, VALUE)

#define hash_map_entry(KEY, VALUE) HASH_MAP_NAME(hash_map_entry, KEY, VALUE)
#define hash_map_entry_get(KEY, VALUE) HASH_MAP_NAME(hash_map_entry_get, KEY, VALUE)
#define hash_map_entry_insert(KEY, VALUE) HASH_MAP_NAME(hash_map_entry_insert, KEY, VALUE)
#define hash_map_entry_remove(KEY, VALUE) HASH_MAP_NAME(hash_map_entry_remove, KEY, VALUE)

typedef enum HashMapSlotOccupancy {
    HASH_MAP_SLOT_EMPTY,
    HASH_MAP_SLOT_FULL,
    HASH_MAP_SLOT_DELETED,
} HashMapSlotOccupancy;

#define DECL_HASH_MAP(KEY, VALUE)                                               \
    typedef struct HashMap(KEY, VALUE) {                                        \
        u8* occupancies;                                                        \
        KEY* keys;                                                              \
        VALUE* values;                                                          \
        usize len;                                                              \
        usize capacity;                                                         \
    } HashMap(KEY, VALUE);                                                      \
                                                                                \
    typedef struct HashMapEntry(KEY, VALUE) {                                   \
        HashMap(KEY, VALUE)* hash_map;                                          \
        KEY key;                                                                \
        bool has_slot;                                                          \
        usize slot_index;                                                       \
        bool has_key_hash;                                                      \
        u64 key_hash;                                                           \
    } HashMapEntry(KEY, VALUE);                                                 \
                                                                                \
    HashMap(KEY, VALUE)                                                         \
    hash_map_new(KEY, VALUE)(void);                                             \
                                                                                \
    void                                                                        \
    hash_map_free(KEY, VALUE)(                                                  \
        HashMap(KEY, VALUE)* hash_map                                           \
    );                                                                          \
                                                                                \
    const VALUE*                                                                \
    hash_map_get(KEY, VALUE)(                                                   \
        HashMap(KEY, VALUE) hash_map,                                           \
        KEY key                                                                 \
    );                                                                          \
                                                                                \
    VALUE*                                                                      \
    hash_map_get_mut(KEY, VALUE)(                                               \
        HashMap(KEY, VALUE)* hash_map,                                          \
        KEY key                                                                 \
    );                                                                          \
                                                                                \
    void                                                                        \
    hash_map_insert(KEY, VALUE)(                                                \
        HashMap(KEY, VALUE)* hash_map,                                          \
        KEY key,                                                                \
        VALUE value                                                             \
    );                                                                          \
                                                                                \
    void                                                                        \
    hash_map_remove(KEY, VALUE)(                                                \
        HashMap(KEY, VALUE)* hash_map,                                          \
        KEY key                                                                 \
    );                                                                          \
                                                                                \
    HashMapEntry(KEY, VALUE)                                                    \
    hash_map_entry(KEY, VALUE)(                                                 \
        HashMap(KEY, VALUE)* hash_map,                                          \
        KEY key                                                                 \
    );                                                                          \
                                                                                \
    VALUE*                                                                      \
    hash_map_entry_get(KEY, VALUE)(                                             \
        HashMapEntry(KEY, VALUE) entry                                          \
    );                                                                          \
                                                                                \
    void                                                                        \
    hash_map_entry_insert(KEY, VALUE)(                                          \
        HashMapEntry(KEY, VALUE)* entry,                                        \
        VALUE value                                                             \
    );                                                                          \
                                                                                \
    void                                                                        \
    hash_map_entry_remove(KEY, VALUE)(                                          \
        HashMapEntry(KEY, VALUE)* entry                                         \
    );                                                                          \


#define HASH_MAP_MAX_LOAD_FACTOR (0.75)

#define IMPL_HASH_MAP_NEW(KEY, VALUE)                                           \
    HashMap(KEY, VALUE)                                                         \
    hash_map_new(KEY, VALUE)(void) {                                            \
        return (HashMap(KEY, VALUE)){                                           \
            .keys = NULL,                                                       \
            .values = NULL,                                                     \
            .len = 0,                                                           \
            .capacity = 0                                                       \
        };                                                                      \
    }                                                                           \

#define IMPL_HASH_MAP_FREE(KEY, VALUE)                                          \
    void                                                                        \
    hash_map_free(KEY, VALUE)(                                                  \
        HashMap(KEY, VALUE)* hash_map                                           \
    ) {                                                                         \
        free(hash_map->values);                                                 \
    }                                                                           \

#define IMPL_HASH_MAP_GET(KEY, VALUE)                                           \
    const VALUE*                                                                \
    hash_map_get(KEY, VALUE)(                                                   \
        HashMap(KEY, VALUE) hash_map,                                           \
        KEY key                                                                 \
    ) {                                                                         \
        if (hash_map.len == 0) {                                                \
            return NULL;                                                        \
        }                                                                       \
                                                                                \
        Hasher hasher = new_hasher();                                           \
        hash(KEY)(&hasher, key);                                                \
        u64 hash = finish_hash(hasher);                                         \
                                                                                \
        usize start_i = hash % hash_map.capacity;                               \
        usize i = start_i;                                                      \
        do {                                                                    \
            HashMapSlotOccupancy occupancy = hash_map.occupancies[i];           \
            /* we use if statements instead of a switch to be able to break
            out of the loop. */                                                 \
            if (occupancy == HASH_MAP_SLOT_EMPTY) {                             \
                break;                                                          \
            } else if (occupancy == HASH_MAP_SLOT_DELETED) {                    \
                continue;                                                       \
            }                                                                   \
                                                                                \
            KEY key2 = hash_map.keys[i];                                        \
            if (eq(KEY)(key, key2)) {                                           \
                return hash_map.values + i;                                     \
            }                                                                   \
        } while (                                                               \
            /* linear probing */                                                \
            i = (i + 1 == hash_map.capacity) ? 0 : (i + 1), i != start_i        \
        );                                                                      \
                                                                                \
        return NULL;                                                            \
    }                                                                           \

#define IMPL_HASH_MAP_ENTRY(KEY, VALUE)                                         \
    HashMapEntry(KEY, VALUE)                                                    \
    hash_map_entry(KEY, VALUE)(                                                 \
        HashMap(KEY, VALUE)* p_hash_map,                                        \
        KEY key                                                                 \
    ) {                                                                         \
        HashMap(KEY, VALUE) hash_map = *p_hash_map;                             \
                                                                                \
        if (hash_map.capacity == 0) {                                           \
            return (HashMapEntry(KEY, VALUE)){                                  \
                .hash_map = p_hash_map,                                         \
                .key = key,                                                     \
                .has_slot = false,                                              \
                .has_key_hash = false,                                          \
            };                                                                  \
        }                                                                       \
                                                                                \
        Hasher hasher = new_hasher();                                           \
        hash(KEY)(&hasher, key);                                                \
        u64 hash = finish_hash(hasher);                                         \
                                                                                \
        usize start_i = hash % hash_map.capacity;                               \
        usize i = start_i;                                                      \
        usize slot_index = -1;                                                  \
        do {                                                                    \
            HashMapSlotOccupancy occupancy = hash_map.occupancies[i];           \
            /* we use if statements instead of a switch to be able to break
            out of the loop. */                                                 \
            if (occupancy == HASH_MAP_SLOT_EMPTY) {                             \
                if (slot_index == -1) slot_index = i;                           \
                break;                                                          \
            } else if (occupancy == HASH_MAP_SLOT_DELETED) {                    \
                if (slot_index == -1) slot_index = i;                           \
                continue;                                                       \
            }                                                                   \
                                                                                \
            KEY key2 = hash_map.keys[i];                                        \
            if (eq(KEY)(key, key2)) {                                           \
                return (HashMapEntry(KEY, VALUE)) {                             \
                    .hash_map = p_hash_map,                                     \
                    .key = key,                                                 \
                    .has_slot = true,                                           \
                    .slot_index = i,                                            \
                    .has_key_hash = true,                                       \
                    .key_hash = hash,                                           \
                };                                                              \
            }                                                                   \
        } while (                                                               \
            /* linear probing. */                                               \
            i = (i + 1 == hash_map.capacity) ? 0 : (i + 1), i != start_i        \
        );                                                                      \
                                                                                \
        if (                                                                    \
            slot_index == -1                                                    \
            || hash_map.len + 1 > HASH_MAP_MAX_LOAD_FACTOR * hash_map.capacity  \
        ) {                                                                     \
            return (HashMapEntry(KEY, VALUE)){                                  \
                .hash_map = p_hash_map,                                         \
                .key = key,                                                     \
                .has_slot = false,                                              \
                .has_key_hash = true,                                           \
                .key_hash = hash,                                               \
            };                                                                  \
        }                                                                       \
                                                                                \
        return (HashMapEntry(KEY, VALUE)){                                      \
            .hash_map = p_hash_map,                                             \
            .key = key,                                                         \
            .has_slot = true,                                                   \
            .slot_index = slot_index,                                           \
            .has_key_hash = true,                                               \
            .key_hash = hash,                                                   \
        };                                                                      \
    }                                                                           \

#define IMPL_HASH_MAP_ENTRY_GET(KEY, VALUE)                                     \
    VALUE*                                                                      \
    hash_map_entry_get(KEY, VALUE)(                                             \
        HashMapEntry(KEY, VALUE) entry                                          \
    ) {                                                                         \
        if (!entry.has_slot) {                                                  \
            return NULL;                                                        \
        }                                                                       \
        if (entry.hash_map->occupancies[entry.slot_index] != HASH_MAP_SLOT_FULL) {  \
            return NULL;                                                        \
        }                                                                       \
        return entry.hash_map->values + entry.slot_index;                       \
    }                                                                           \

#define IMPL_HASH_MAP_GET_MUT(KEY, VALUE)                                       \
    VALUE*                                                                      \
    hash_map_get_mut(KEY, VALUE)(                                               \
        HashMap(KEY, VALUE)* hash_map,                                          \
        KEY key                                                                 \
    ) {                                                                         \
        HashMapEntry(KEY, VALUE) entry =                                        \
            hash_map_entry(KEY, VALUE)(hash_map, key);                          \
        return hash_map_entry_get(KEY, VALUE)(entry);                           \
    }                                                                           \

#define IMPL_HASH_MAP_INSERT(KEY, VALUE)                                        \
    void                                                                        \
    hash_map_insert(KEY, VALUE)(                                                \
        HashMap(KEY, VALUE)* hash_map,                                          \
        KEY key,                                                                \
        VALUE value                                                             \
    ) {                                                                         \
        HashMapEntry(KEY, VALUE) entry =                                        \
            hash_map_entry(KEY, VALUE)(hash_map, key);                          \
        hash_map_entry_insert(KEY, VALUE)(&entry, value);                       \
    }                                                                           \

#define IMPL_HASH_MAP_REMOVE(KEY, VALUE)                                        \
    void                                                                        \
    hash_map_remove(KEY, VALUE)(                                                \
        HashMap(KEY, VALUE)* hash_map,                                          \
        KEY key                                                                 \
    ) {                                                                         \
        HashMapEntry(KEY, VALUE) entry =                                        \
            hash_map_entry(KEY, VALUE)(hash_map, key);                          \
        hash_map_entry_remove(KEY, VALUE)(&entry);                              \
    }                                                                           \

#define IMPL_HASH_MAP_ENTRY_INSERT(KEY, VALUE)                                  \
    static usize                                                                \
    hash_map_insert_in_empty_slot##KEY##_##VALUE(                               \
        HashMap(KEY, VALUE)* hash_map,                                          \
        KEY key,                                                                \
        u64 key_hash,                                                           \
        VALUE value                                                             \
    ) {                                                                         \
        usize i = key_hash % hash_map->capacity;                                \
        for (;; i = (i + 1 == hash_map->capacity) ? 0 : (i + 1)) {              \
            HashMapSlotOccupancy occupancy = hash_map->occupancies[i];          \
            if (occupancy == HASH_MAP_SLOT_FULL) continue;                      \
                                                                                \
            hash_map->occupancies[i] = HASH_MAP_SLOT_FULL;                      \
            hash_map->keys[i] = key;                                            \
            hash_map->values[i] = value;                                        \
            break;                                                              \
        }                                                                       \
        hash_map->len++;                                                        \
        return i;                                                               \
    }                                                                           \
                                                                                \
    void                                                                        \
    hash_map_entry_insert(KEY, VALUE)(                                          \
        HashMapEntry(KEY, VALUE)* entry,                                        \
        VALUE value                                                             \
    ) {                                                                         \
        if (entry->has_slot) {                                                  \
            if (                                                                \
                entry->hash_map->occupancies[entry->slot_index]                 \
                != HASH_MAP_SLOT_FULL                                           \
            ) {                                                                 \
                entry->hash_map->occupancies[entry->slot_index] = HASH_MAP_SLOT_FULL;   \
                entry->hash_map->keys[entry->slot_index] = entry->key;          \
            }                                                                   \
                                                                                \
            entry->hash_map->values[entry->slot_index] = value;                 \
            entry->hash_map->len++;                                             \
            return;                                                             \
        }                                                                       \
                                                                                \
        /* reallocate. */                                                       \
        usize old_capacity = entry->hash_map->capacity;                         \
        usize new_capacity = (old_capacity <= 4) ? 8 : 2 * old_capacity;        \
                                                                                \
        usize values_bufsz = new_capacity * sizeof(VALUE);                      \
        usize values_bufsz_aligned = align_up_size(values_bufsz, alignof(KEY)); \
        usize keys_bufsz = new_capacity * sizeof(KEY);                          \
        usize occupancies_bufsz = new_capacity * sizeof(u8);                    \
        usize bufsz = values_bufsz_aligned + keys_bufsz + occupancies_bufsz;    \
                                                                                \
        void* new_buf = malloc(bufsz);                                          \
        HashMap(KEY, VALUE) new_hash_map = {                                    \
            .occupancies = (u8*)(new_buf + values_bufsz_aligned + keys_bufsz),  \
            .keys = (KEY*)(new_buf + values_bufsz_aligned),                     \
            .values = (VALUE*)new_buf,                                          \
            .len = 0,                                                           \
            .capacity = new_capacity,                                           \
        };                                                                      \
        memset(new_hash_map.occupancies, 0, occupancies_bufsz);                 \
                                                                                \
        /* copy over existing entries. */                                       \
        for (usize i = 0; i < entry->hash_map->capacity; i++) {                 \
            if (entry->hash_map->occupancies[i] != HASH_MAP_SLOT_FULL) continue;\
                                                                                \
            KEY key = entry->hash_map->keys[i];                                 \
            VALUE value = entry->hash_map->values[i];                           \
                                                                                \
            Hasher hasher = new_hasher();                                       \
            hash(KEY)(&hasher, key);                                            \
            u64 key_hash = finish_hash(hasher);                                 \
                                                                                \
            hash_map_insert_in_empty_slot##KEY##_##VALUE(                       \
                &new_hash_map,                                                  \
                key,                                                            \
                key_hash,                                                       \
                value                                                           \
            );                                                                  \
        }                                                                       \
                                                                                \
        /* replace hash map. */                                                 \
        hash_map_free(KEY, VALUE)(entry->hash_map);                             \
        *entry->hash_map = new_hash_map;                                        \
                                                                                \
        /* insert new entry. */                                                 \
        u64 key_hash;                                                           \
        if (entry->has_key_hash) {                                              \
            key_hash = entry->key_hash;                                         \
        } else {                                                                \
            Hasher hasher = new_hasher();                                       \
            hash(KEY)(&hasher, entry->key);                                     \
            key_hash = finish_hash(hasher);                                     \
        }                                                                       \
                                                                                \
        usize i = hash_map_insert_in_empty_slot##KEY##_##VALUE(                 \
            entry->hash_map,                                                    \
            entry->key,                                                         \
            key_hash,                                                           \
            value                                                               \
        );                                                                      \
                                                                                \
        entry->has_slot = true;                                                 \
        entry->slot_index = i;                                                  \
        entry->has_key_hash = true;                                             \
        entry->key_hash = key_hash;                                             \
    }                                                                           \

#define IMPL_HASH_MAP_ENTRY_REMOVE(KEY, VALUE)                                  \
    void                                                                        \
    hash_map_entry_remove(KEY, VALUE)(                                          \
        HashMapEntry(KEY, VALUE)* entry                                         \
    ) {                                                                         \
        if (!entry->has_slot) return;                                           \
        if (                                                                    \
            entry->hash_map->occupancies[entry->slot_index]                     \
            != HASH_MAP_SLOT_FULL                                               \
        ) return;                                                               \
        entry->hash_map->occupancies[entry->slot_index] = HASH_MAP_SLOT_DELETED;\
        entry->hash_map->len--;                                                 \
    }                                                                           \


#define IMPL_HASH_MAP(KEY, VALUE)                                               \
    IMPL_HASH_MAP_NEW(KEY, VALUE)                                               \
    IMPL_HASH_MAP_FREE(KEY, VALUE)                                              \
                                                                                \
    IMPL_HASH_MAP_GET(KEY, VALUE)                                               \
    IMPL_HASH_MAP_GET_MUT(KEY, VALUE)                                           \
    IMPL_HASH_MAP_INSERT(KEY, VALUE)                                            \
    IMPL_HASH_MAP_REMOVE(KEY, VALUE)                                            \
                                                                                \
    IMPL_HASH_MAP_ENTRY(KEY, VALUE)                                             \
    IMPL_HASH_MAP_ENTRY_GET(KEY, VALUE)                                         \
    IMPL_HASH_MAP_ENTRY_INSERT(KEY, VALUE)                                      \
    IMPL_HASH_MAP_ENTRY_REMOVE(KEY, VALUE)                                      \
