#include <assert.h>

#include "util/collections/hash_map.h"

typedef const char* Person;

void hash_Person(Hasher* hasher, Person person);
bool eq_Person(Person a, Person b);
DECL_HASH_MAP(Person, i32);
IMPL_HASH_MAP(Person, i32);

bool value_eq(const i32* a, i32 b);

int main(int argc, char const* argv[]) {
    HashMap(Person, i32) ages = hash_map_new(Person, i32)();

    assert(ages.len == 0);
    hash_map_insert(Person, i32)(&ages, "Ada", 30);
    assert(ages.len == 1);
    hash_map_insert(Person, i32)(&ages, "Bob", 31);
    assert(ages.len == 2);
    hash_map_insert(Person, i32)(&ages, "David", 50);
    assert(ages.len == 3);
    hash_map_insert(Person, i32)(&ages, "Carl", 40);
    assert(ages.len == 4);
    hash_map_insert(Person, i32)(&ages, "Eve", 32);
    assert(ages.len == 5);

    assert(value_eq(hash_map_get(Person, i32)(ages, "Ada"), 30));
    assert(value_eq(hash_map_get(Person, i32)(ages, "Bob"), 31));
    assert(value_eq(hash_map_get(Person, i32)(ages, "Eve"), 32));
    assert(value_eq(hash_map_get(Person, i32)(ages, "Carl"), 40));
    assert(value_eq(hash_map_get(Person, i32)(ages, "David"), 50));

    hash_map_remove(Person, i32)(&ages, "Bob");
    assert(ages.len == 4);
    assert(hash_map_get(Person, i32)(ages, "Bob") == NULL);
    assert(hash_map_get_mut(Person, i32)(&ages, "Bob") == NULL);

    hash_map_insert(Person, i32)(&ages, "Lea", 33);
    assert(value_eq(hash_map_get(Person, i32)(ages, "Lea"), 33));
    assert(value_eq(hash_map_get_mut(Person, i32)(&ages, "Lea"), 33));

    HashMapEntry(Person, i32) entry =
        hash_map_entry(Person, i32)(&ages, "Ada");
    assert(value_eq(hash_map_entry_get(Person, i32)(entry), 30));

    hash_map_entry_insert(Person, i32)(&entry, 130);
    assert(value_eq(hash_map_entry_get(Person, i32)(entry), 130));
    assert(value_eq(hash_map_get(Person, i32)(ages, "Ada"), 130));

    hash_map_entry_remove(Person, i32)(&entry);
    assert(hash_map_entry_get(Person, i32)(entry) == NULL);
    assert(hash_map_get(Person, i32)(ages, "Ada") == NULL);
    assert(value_eq(hash_map_get(Person, i32)(ages, "Eve"), 32));

    hash_map_entry_insert(Person, i32)(&entry, 230);
    assert(value_eq(hash_map_entry_get(Person, i32)(entry), 230));
    assert(value_eq(hash_map_get(Person, i32)(ages, "Ada"), 230));

    hash_map_free(Person, i32)(&ages);

    return 0;
}

void hash_Person(Hasher* hasher, Person person) {
    hash_usize(hasher, strlen(person));
}

bool eq_Person(Person a, Person b) {
    return strcmp(a, b) == 0;
}

bool value_eq(const i32* a, i32 b) {
    if (a == NULL) return false;
    return *a == b;
}
