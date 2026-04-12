set(FLOAT_PARSER_DATA ${PROJECT_BINARY_DIR}/float_parser_data.h)
set(GEN_FLOAT_PARSER_DATA ${CMAKE_CURRENT_SOURCE_DIR}/gen_float_parser_data.py)

add_custom_command(
    OUTPUT ${FLOAT_PARSER_DATA}
    COMMAND python3 ${GEN_FLOAT_PARSER_DATA} > ${FLOAT_PARSER_DATA}
    DEPENDS ${GEN_FLOAT_PARSER_DATA}
    VERBATIM
)
add_custom_target(float_parser_data DEPENDS ${FLOAT_PARSER_DATA})

target_include_directories(libcough PRIVATE ${PROJECT_BINARY_DIR})
target_sources(libcough PRIVATE
    parser.h parser.c
    ${FLOAT_PARSER_DATA}
)
add_dependencies(libcough float_parser_data)
