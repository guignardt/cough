#include <assert.h>
#include <string.h>

#include "util/collections/string.h"
#include "diagnostics/log.h"

int main(int argc, char const* argv[]) {
    StringBuf string = string_buf_new();
    assert(string.len == 0);

    string_buf_reserve(&string, 1);
    assert(string.len == 0);
    assert(!strcmp(string.data, ""));

    string_buf_push(&string, 'L');
    assert(!strcmp(string.data, "L"));
    assert(string.len == 1);

    string_buf_extend(&string, "orem ipsum");
    assert(!strcmp(string.data, "Lorem ipsum"));
    assert(string.len == 11);
    
    string_buf_push(&string, ' ');
    assert(!strcmp(string.data, "Lorem ipsum "));
    assert(string.len == 12);

    string_buf_extend(&string, "dolor sit amet, consectetur adipiscing elit");
    assert(!strcmp(string.data, "Lorem ipsum dolor sit amet, consectetur adipiscing elit"));
    assert(string.len == 55);

    StringBuf formatted = format("My favorite %s is %d!", "number", 42);
    eprintf("formatted: %s\n", formatted.data);
    assert(!strcmp(formatted.data, "My favorite number is 42!"));

    return 0;
}
