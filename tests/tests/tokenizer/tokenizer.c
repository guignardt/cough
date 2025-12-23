#include <string.h>

#include "tokenizer/tokenizer.h"

#include "tests/common.h"

int main(int argc, char const* argv[]) {
    char const* source = "() : :: = := -> => ; ! | & ^ hello let fn \n false true";
    TestReporter reporter = test_reporter_new();

    TokenStream tokens;
    tokenize(
        (String){ .data = source, .len = strlen(source) },
        &reporter.base,
        &tokens
    );

    assert(reporter.error_codes.len == 0);

    assert(tokens.tokens.len == 18);
    for (usize i = 0; i < tokens.tokens.len; i++) {
        assert(tokens.tokens.data[i].kind == i);
    }

    return 0;
}
