#include <assert.h>

#include "util/collections/array.h"

typedef struct Foo {
    i32 id;
} Foo;
DECL_ARRAY_BUF(Foo);
IMPL_ARRAY_BUF(Foo);

int main(int argc, char const* argv[]) {
    ArrayBuf(Foo) foos = array_buf_new(Foo)();
    assert(foos.len == 0);

    array_buf_push(Foo)(&foos, (Foo){ 0 });
    assert(foos.len == 1);
    array_buf_push(Foo)(&foos, (Foo){ 1 });
    array_buf_push(Foo)(&foos, (Foo){ 2 });
    array_buf_push(Foo)(&foos, (Foo){ 3 });
    array_buf_push(Foo)(&foos, (Foo){ 4 });
    assert(foos.len == 5);

    Foo elts[5] = { { 5 }, { 6 }, { 7 }, { 8 }, { 9 } };
    array_buf_extend(Foo)(&foos, elts, 5);
    assert(foos.len == 10);

    for (usize i = 0; i < foos.len; i++) {
        Foo foo = foos.data[i];
        assert(foo.id == i);
    }

    return 0;
}
