#include "tests/common.h"
#include "disassembler/disassembler.h"

int main(int argc, char const** argv) {
    char const source[] = 
        "wrap :: fn x: Bool -> Bool => identity(x);\n"
        "identity :: fn y: Bool -> Bool => y;\n"
    ;
    char const* assembly[] = {
        ":wrap",
        "   res 1",
        "   set %0",
        "   var %0",
        "   loc :identity",
        "   cal",
        "   ret",
        "",
        ":identity",
        "   res 1",
        "   set %0",
        "   var %0",
        "   ret",
    };
    Bytecode generated = source_to_module_bytecode(STRING_LITERAL(source));
    Bytecode expected = assembly_to_bytecode(assembly, sizeof(assembly) / sizeof(char const*));
    
    eprintf("DISASSEMBLY\n");
    eprintf("%s\n", disassemble(generated).data);
    assert(generated.instructions.len == expected.instructions.len);
    assert(memcmp(generated.instructions.data, expected.instructions.data, expected.instructions.len) == 0);
    return 0;
}
