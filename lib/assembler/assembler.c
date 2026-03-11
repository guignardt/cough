#include <ctype.h>

#include "assembler/assembler.h"
#include "collections/hash_map.h"
#include "emitter/emitter.h"

DECL_HASH_MAP(Mnemonic, u8)
IMPL_HASH_MAP(Mnemonic, u8)
DECL_HASH_MAP(String, usize)
IMPL_HASH_MAP(String, usize)

typedef struct Assembler {
    String text;
    usize position;
    HashMap(Mnemonic, u8) mnemonic_opcodes;
    HashMap(Mnemonic, u8) mnemonic_syscalls;
    HashMap(String, usize) symbols;
    Emitter emitter;
    Reporter* reporter;
} Assembler;

static Assembler assembler_new(String assembly, Reporter* reporter);
static Result assemble_all(Assembler* assembler);
static Result assemble_one(Assembler* assembler);
static void assembler_free(Assembler* assembler);
static Result assembler_finish(Assembler* assembler, Bytecode* dst);

static Result parse_imb(Assembler* assembler, EmitArg(imb)* dst);
static Result parse_imw(Assembler* assembler, EmitArg(imw)* dst);
static Result parse_loc(Assembler* assembler, EmitArg(loc)* dst);
static Result parse_var(Assembler* assembler, EmitArg(var)* dst);

static Result parse_symbol_name(
    Assembler* assembler,
    SymbolIndex* dst,
    Range* range
);

static void unexpected_eof(Assembler* assembler);
static void invalid_instruction(Assembler* assembler, Range source);
static void invalid_syscall(Assembler* assembler, Range source);
static void invalid_arg(Assembler* assembler, Range source);
static void invalid_symbol(Assembler* assembler, Range source);
static void duplicate_symbol(Assembler* assembler, Range source);
static void undefined_symbol(Assembler* assembler);

Result assemble(String assembly, Reporter* reporter, Bytecode* dst) {
    Assembler assembler = assembler_new(assembly, reporter);
    if (assemble_all(&assembler) != OK) {
        assembler_free(&assembler);
        return ERROR;
    }
    if (assembler_finish(&assembler, dst) != OK) {
        assembler_free(&assembler);
        return ERROR;
    }
    return OK;
}

static HashMap(Mnemonic, u8) mnemonic_table(const Mnemonic* mnemonics, usize len) {
    HashMap(Mnemonic, u8) table = hash_map_new(Mnemonic, u8)();
    for (usize i = 0; i < len; i++) {
        Mnemonic mnemonic = mnemonics[i];
        hash_map_insert(Mnemonic, u8)(&table, mnemonic, i);
    }
    return table;
}

static Assembler assembler_new(String text, Reporter* reporter) {
    return (Assembler){
        .text = text,
        .position = 0,
        .mnemonic_opcodes = mnemonic_table(instruction_mnemonics, OPCODES_LEN),
        .mnemonic_syscalls = mnemonic_table(syscall_mnemonics, SYSCALLS_LEN),
        .symbols = hash_map_new(String, usize)(),
        .emitter = emitter_new(),
        .reporter = reporter,
    };
}

static void assembler_free(Assembler* assembler) {
    hash_map_free(Mnemonic, u8)(&assembler->mnemonic_opcodes);
    hash_map_free(Mnemonic, u8)(&assembler->mnemonic_syscalls);
    hash_map_free(String, usize)(&assembler->symbols);
    emitter_free(&assembler->emitter);
}

static Result assemble_all(Assembler* assembler) {
    bool new_thing = true;
    while(assembler->position < assembler->text.len) {
        char c = assembler->text.data[assembler->position];

        if (c == '\n') {
            new_thing = true;
            assembler->position++;
            continue;
        }
        if (isspace(c)) {
            assembler->position++;
            continue;
        }

        if (!new_thing) {
            invalid_arg(
                assembler,
                (Range){ assembler->position, assembler->position }
            );
            return ERROR;
        }

        if (c == ':') {
            SymbolIndex symbol_index;
            Range symbol_name_range;
            assembler->position++;
            if (
                parse_symbol_name(assembler, &symbol_index, &symbol_name_range)
                != OK
            ) {
                return ERROR;
            }
            if (!emit_symbol_location(&assembler->emitter, symbol_index)) {
                duplicate_symbol(assembler, symbol_name_range);
                return ERROR;
            }
        } else if (assemble_one(assembler) != OK) {
            return ERROR;
        }

        new_thing = false;
    }

    return OK;
}

static Result parse_instruction(
    Assembler* assembler,
    HashMap(Mnemonic, u8) table,
    u8* dst,
    Range* range
) {
    usize start = assembler->position;
    usize max_len = assembler->text.len - start;
    Mnemonic mnemonic = { .chars = {0} };
    usize i;
    for (i = 0; i < max_len; i++) {
        char c = assembler->text.data[start + i];
        if (!isalpha(c)) {
            break;
        }
        if (i < sizeof(mnemonic.chars)) {
            mnemonic.chars[i] = c;
        }
    }
    usize end = start + i;
    assembler->position = end;
    *range = (Range){ start, end };
    if (end - start > sizeof(mnemonic.chars)) {
        return ERROR;
    }

    u8 const* p_code = hash_map_get(Mnemonic, u8)(table, mnemonic);
    if (!p_code) {
        return ERROR;
    }
    *dst = *p_code;
    return OK;
}

static Result parse_number(
    Assembler* assembler,
    i64 min,
    i64 max,
    i64* dst,
    Range* range
) {
    usize start = assembler->position;
    i64 sign;
    if (assembler->text.data[assembler->position] == '-') {
        sign = -1;
        assembler->position++;
    } else if (assembler->text.data[assembler->position] == '+') {
        sign = 1;
        assembler->position++;
    } else {
        sign = 1;
    }
    usize digit_start = assembler->position;

    i64 val = 0;
    bool overflow = false;
    while (assembler->position < assembler->text.len) {
        char c = assembler->text.data[assembler->position];
        if (!isdigit(c)) {
            break;
        }
        if (__builtin_mul_overflow(val, 10, &val)) {
            overflow = true;
        }
        i64 digit = sign * (c - '0');
        if (__builtin_add_overflow(val, digit, &val)) {
            overflow = true;
        }
        assembler->position++;
    }

    *range = (Range){ start, assembler->position };

    if (assembler->position == digit_start) {
        return ERROR;
    }

    if (overflow || val < min || val > max) {
        return ERROR;
    }
    *dst = val;
    return OK;
}

static Result parse_single_char(Assembler* assembler, char c, Range* range) {
    if (assembler->position == assembler->text.len) {
        *range = (Range){ assembler->position, assembler->position };
        return ERROR;
    }
    *range = (Range){ assembler->position, assembler->position + 1 };
    if (assembler->text.data[assembler->position] != c) {
        return ERROR;
    }
    assembler->position++;
    return OK;
}

static Result parse_imb(Assembler* assembler, EmitArg(imb)* dst) {
    i64 number;
    Range range;
    if (parse_number(assembler, 0, UINT16_MAX, &number, &range) != OK) {
        invalid_arg(assembler, range);
        return ERROR;
    }
    *dst = number;
    return OK;
}

static Result parse_imw(Assembler* assembler, EmitArg(imw)* dst) {
    i64 number;
    Range range;
    if (parse_number(assembler, INT64_MIN, INT64_MAX, &number, &range) != OK) {
        invalid_arg(assembler, range);
        return ERROR;
    }
    dst->as_int = number;
    return OK;
}

static Result parse_var(Assembler* assembler, EmitArg(var)* dst) {
    if (assembler->position == assembler->text.len) {
        invalid_arg(assembler, (Range){
            assembler->position,
            assembler->position
        });
        return ERROR;
    }
    if (assembler->text.data[assembler->position] != '%') {
        invalid_arg(assembler, (Range){
            assembler->position,
            assembler->position + 1
        });
        return ERROR;
    }
    assembler->position++;
    EmitArg(imb) index;
    if (parse_imb(assembler, &index) != OK) {
        return ERROR;
    }
    *dst = index;
    return OK;
}

static Result parse_loc(Assembler* assembler, EmitArg(loc)* dst) {
    Range colon_range;
    if (parse_single_char(assembler, ':', &colon_range) != OK) {
        invalid_arg(assembler, colon_range);
        return ERROR;
    }

    SymbolIndex index;
    if (parse_symbol_name(assembler, &index, NULL) != OK) {
        return ERROR;
    }
    *dst = index;
    return OK;
}

static Result parse_symbol_name(
    Assembler* assembler,
    SymbolIndex* dst,
    Range* range
) {
    usize start = assembler->position;
    while (assembler->position < assembler->text.len) {
        char c = assembler->text.data[assembler->position];
        if (!isalnum(c) && c != '_') {
            break;
        }
        assembler->position++;
    }
    if (assembler->position == start) {
        invalid_symbol(assembler, (Range){ start, start });
        return ERROR;
    }
    String symbol = {
        .data = assembler->text.data + start,
        .len = assembler->position - start,
    };

    HashMapEntry(String, usize) entry =
        hash_map_entry(String, usize)(&assembler->symbols, symbol);
    SymbolIndex const* p_index = hash_map_entry_get(String, usize)(entry);
    if (p_index) {
        *dst = *p_index;
    } else {
        SymbolIndex index = emit_new_symbol(&assembler->emitter);
        hash_map_entry_insert(String, usize)(&entry, index);
        *dst = index;
    }

    if (range) {
        *range = (Range){ start, assembler->position };
    }
    return OK;
}

static Result skip_seperator(Assembler* assembler) {
    bool skipped = false;
    while (assembler->position < assembler->text.len) {
        char c = assembler->text.data[assembler->position];
        if (isspace(c) && c != '\n') {
            skipped = true;
            assembler->position++;
        } else {
            break;
        }
    }
    if (!skipped) {
        return ERROR;
    }
    return OK;
}

typedef union Arg {
    EmitArg(imb) as_imb;
    EmitArg(imw) as_imw;
    EmitArg(loc) as_loc;
    EmitArg(var) as_var;
} Arg;

#define PARSE_ARG(idx, kind, ...)                                               \
    if (skip_seperator(assembler) != OK) {                                      \
        usize pos = assembler->position;                                        \
        invalid_arg(assembler, (Range){ pos, pos });                            \
        return ERROR;                                                           \
    }                                                                           \
    if (parse_##kind(assembler, &x##idx.as_##kind) != OK) {                     \
        return ERROR;                                                           \
    }
#define EMIT_ARG(idx, kind, ...) , x##idx.as_##kind

static Result assemble_syscall(Assembler* assembler);

static Result assemble_one(Assembler* assembler) {
    u8 opcode_raw;
    Range mnemonic_range;
    if (
        parse_instruction(
            assembler,
            assembler->mnemonic_opcodes,
            &opcode_raw,
            &mnemonic_range
        ) != OK
    ) {
        invalid_instruction(assembler, mnemonic_range);
    }

    Opcode opcode = opcode_raw;
    Arg x0, x1, x2;
    switch (opcode) {
        #define ASSEMBLE_ONE_CASE(code, mnemo, ...)                             \
            case code:                                                          \
                FOR_ALL(PARSE_ARG __VA_OPT__(, __VA_ARGS__))                    \
                emit(mnemo)(                                                    \
                    &assembler->emitter                                         \
                    FOR_ALL(EMIT_ARG __VA_OPT__(, __VA_ARGS__))                 \
                );                                                              \
                return OK;
        FOR_OPERATIONS(ASSEMBLE_ONE_CASE)

        case OP_SYS:
            skip_seperator(assembler);
            return assemble_syscall(assembler);
        
        default:
            // unreachable
            return ERROR;
    }
}

static Result assemble_syscall(Assembler* assembler) {
    u8 syscall_raw;
    Range mnemonic_range;
    if (
        parse_instruction(
            assembler,
            assembler->mnemonic_syscalls,
            &syscall_raw,
            &mnemonic_range
        ) != OK
    ) {
        invalid_syscall(assembler, mnemonic_range);
    }

    Syscall syscall = syscall_raw;
    Arg x0, x1, x2;
    switch (syscall) {
        #define ASSEMBLE_SYSCALL_ONE_CASE(code, mnemo, ...)                     \
            case code:                                                          \
                FOR_ALL(PARSE_ARG __VA_OPT__(, __VA_ARGS__))                    \
                emit_sys(mnemo)(                                                \
                    &assembler->emitter                                         \
                    FOR_ALL(EMIT_ARG __VA_OPT__(, __VA_ARGS__))                 \
                );                                                              \
                return OK;
        FOR_SYSCALLS(ASSEMBLE_SYSCALL_ONE_CASE)
        
        default:
            // unreachable
            return ERROR;
    }
}

static Result assembler_finish(Assembler* assembler, Bytecode* dst) {
    if (!emitter_finish(&assembler->emitter, dst)) {
        undefined_symbol(assembler);
        return ERROR;
    }
    hash_map_free(Mnemonic, u8)(&assembler->mnemonic_opcodes);
    hash_map_free(Mnemonic, u8)(&assembler->mnemonic_syscalls);
    hash_map_free(String, usize)(&assembler->symbols);
    return OK;
}

static void unexpected_eof(Assembler* assembler) {
    Reporter* reporter = assembler->reporter;
    report_start(reporter, SEVERITY_ERROR, 0);
    report_message(reporter, format("unexpected end-of-file"));
    usize len = assembler->text.len;
    report_source_code(reporter, (Range){ len, len });
    report_end(reporter);
}

static void invalid_instruction(Assembler* assembler, Range source) {
    Reporter* reporter = assembler->reporter;
    report_start(reporter, SEVERITY_ERROR, 1);
    report_message(reporter, format("invalid instruction"));
    report_source_code(reporter, source);
    report_end(reporter);
}

static void invalid_syscall(Assembler* assembler, Range source) {
    Reporter* reporter = assembler->reporter;
    report_start(reporter, SEVERITY_ERROR, 2);
    report_message(reporter, format("invalid syscall"));
    report_source_code(reporter, source);
    report_end(reporter);
}

static void invalid_arg(Assembler* assembler, Range source) {
    Reporter* reporter = assembler->reporter;
    report_start(reporter, SEVERITY_ERROR, 3);
    report_message(reporter, format("invalid argument"));
    report_source_code(reporter, source);
    report_end(reporter);
}

static void invalid_symbol(Assembler* assembler, Range source) {
    Reporter* reporter = assembler->reporter;
    report_start(reporter, SEVERITY_ERROR, 4);
    report_message(reporter, format("invalid symbol"));
    report_source_code(reporter, source);
    report_end(reporter);
}

static void duplicate_symbol(Assembler* assembler, Range source) {
    Reporter* reporter = assembler->reporter;
    report_start(reporter, SEVERITY_ERROR, 5);
    report_message(reporter, format("duplicate symbol"));
    report_source_code(reporter, source);
    report_end(reporter);
}

static void undefined_symbol(Assembler* assembler) {
    Reporter* reporter = assembler->reporter;
    report_start(reporter, SEVERITY_ERROR, 5);
    report_message(reporter, format("undefined symbol"));
    report_end(reporter);
}
