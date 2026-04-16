#include "tests/common.h"

int main(int argc, char const* argv[]) {
    String source = STRING_LITERAL(
        "annoy :: fn x: UInt -> UInt => x ^ 420;\n"
        // we don't want to be annoying >:3
        "foo :: fn x: UInt -> UInt => if true => x else => annoy(x);\n"
    );
    Bytecode generated = source_to_module_bytecode(source);

    char const* assembly[] = {
        ":annoy",
        "   res 1",
        "   set %0",
        "   var %0",
        "   sca 420",
        "   xor",
        "   ret",

        ":foo",
        "   res 1",
        "   set %0",
        "   sca -1",
        "   jze :if_false",
        "   var %0",
        "   jmp :end",
        ":if_false",
        "   var %0",
        "   loc :annoy",
        "   cal",
        ":end",
        "   ret",
    };
    Bytecode expected = assembly_to_bytecode(assembly, sizeof(assembly) / sizeof(char const*));

    eprintf("DISASSEMBLY\n");
    eprintf("%s\n", disassemble(generated).data);
    assert(generated.instructions.len == expected.instructions.len);
    assert(memcmp(generated.instructions.data, expected.instructions.data, expected.instructions.len) == 0);

    return 0;
}
