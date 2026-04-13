add_subdirectory(util)

add_subdirectory(diagnostics)

add_subdirectory(source)
add_subdirectory(tokens)
add_subdirectory(ast)
add_subdirectory(bytecode)

add_subdirectory(tokenizer)
add_subdirectory(parser)
add_subdirectory(analyzer)
add_subdirectory(generator)
add_subdirectory(assembler)
add_subdirectory(disassembler)
add_subdirectory(emitter)
add_subdirectory(vm)

target_compile_options(libcough PRIVATE "-Wno-gnu-alignof-expression")
