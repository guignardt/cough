function(cough_test name path)
    add_executable(${name} ${path} ${CMAKE_SOURCE_DIR}/tests/tests/common.c)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/lib ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(${name} PRIVATE libcough)
    add_test(NAME ${name} COMMAND $<TARGET_FILE:${name}>)
endfunction()

cough_test(test_array_buf collections/array_buf.c)
cough_test(test_string_buf collections/string_buf.c)
cough_test(test_hash_map collections/hash_map.c)

cough_test(test_tokenizer tokenizer/tokenizer.c)

cough_test(test_ast_constant_fn ast/constant_fn.c)
cough_test(test_ast_identity_fn ast/identity_fn.c)
cough_test(test_ast_functions ast/functions.c)
cough_test(test_ast_expressions ast/expressions.c)
cough_test(test_ast_numbers ast/numbers.c)

cough_test(test_generator_functions generator/functions.c)
cough_test(test_generator_expression generator/expression.c)

cough_test(test_assembler assembler/assembler.c)

cough_test(test_vm_function_call vm/function_call.c)
cough_test(test_vm_conditional vm/conditional.c)
cough_test(test_vm_fibonacci vm/fibonacci.c)
cough_test(test_vm_expression vm/expression.c)
