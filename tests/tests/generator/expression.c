#include "disassembler/disassembler.h"

#include "tests/common.h"

int main(int argc, char const** argv) {
    char const source[] = 
        "logical :: fn x: Bool -> Bool => !x & true | x;"
    ;
    Bytecode generated = source_to_bytecode(STRING_LITERAL(source));
    char const* assembly[] = {
        ":logical",
        "   res 1",
        "   set %0",
        "   var %0",
        "   not",
        "   sca -1",
        "   and",
        "   var %0",
        "   lor",
        "   ret"
    };
    Bytecode expected = assembly_to_bytecode(assembly, sizeof(assembly) / sizeof(char const*));
    
    eprintf("DISASSEMBLY\n");
    eprintf("%s\n", disassemble(generated).data);
    assert(generated.instructions.len == expected.instructions.len);
    assert(memcmp(generated.instructions.data, expected.instructions.data, expected.instructions.len) == 0);
    return 0;
}
