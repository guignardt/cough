#include "tokenizer/tokenizer.h"
#include "parser/parser.h"
#include "analyzer/analyzer.h"

#include "tests/common.h"

int main(int argc, char const *argv[]) {
    char const source_raw[] =
        "constant_fn :: fn _: Bool -> Bool => true;\n"
    ;
    Module module;
    AstData data;
    source_to_module(STRING_LITERAL(source_raw), &module, &data);

    assert(module.global_constants.len == 1);
    {
        ConstantDef constant = module.global_constants.data[0];
        assert(constant.name.range.start == 0);
        assert(constant.name.range.end == 11);
        assert(!constant.explicitly_typed);
        {
            Expression value = data.expressions.data[constant.value];
            assert(value.kind == EXPRESSION_FUNCTION);
            {
                Pattern input = value.as.function.input;
                assert(input.kind == PATTERN_VARIABLE);
                assert(input.as.variable.name.range.start == 18);
                assert(input.as.variable.name.range.end == 19);
                assert(input.as.variable.explicitly_typed);
                assert(input.as.variable.type_name.kind == TYPE_NAME_IDENTIFIER);
                assert(input.as.variable.type_name.range.start == 21);
                assert(input.as.variable.type_name.range.end == 25);
                assert(input.as.variable.type == TYPE_BOOL);
            }
            {
                Expression output = data.expressions.data[value.as.function.output];
                assert(output.kind == EXPRESSION_LITERAL_BOOL);
                assert(output.as.literal_bool == true);
            }
            assert(value.range.start == 15);
            assert(value.range.end == 41);
        }
    }

    return 0;
}
