#include "emitter/emitter.h"

Emitter emitter_new(void) {
    return (Emitter){
        ._bytecode = {
            .instructions = array_buf_new(Byteword)(),
            .rodata = array_buf_new(Byteword)(),
        },
        ._symbol_locations = array_buf_new(usize)(),
        ._symbol_ref_locations = array_buf_new(usize)(),
        ._next_symbol = 0,
    };
}

#define DECL_EMIT_ARG(idx, kind, ...)                           \
    EmitArg(kind) x##idx __VA_OPT__(,)

#define IMPL_EMIT_ARG_imb(idx)                                  \
    bytecode_write(imb)(&emitter->_bytecode, x##idx);
#define IMPL_EMIT_ARG_imw(idx)                                  \
    bytecode_write(imw)(&emitter->_bytecode, x##idx);
#define IMPL_EMIT_ARG_loc(idx)                                  \
    array_buf_push(usize)(                                      \
        &emitter->_symbol_ref_locations,                        \
        emitter->_bytecode.instructions.len                     \
    );                                                          \
    bytecode_write(loc)(&emitter->_bytecode, x##idx);
#define IMPL_EMIT_ARG_var(idx)                                  \
    bytecode_write(var)(&emitter->_bytecode, x##idx);
#define IMPL_EMIT_ARG(idx, kind, ...) IMPL_EMIT_ARG_##kind(idx)

#define IMPL_EMIT(code, mnemo, ...)                             \
    void emit_##mnemo(                                          \
        Emitter* emitter __VA_OPT__(,)                          \
        FOR_ALL(DECL_EMIT_ARG __VA_OPT__(, __VA_ARGS__))        \
    ) {                                                         \
        bytecode_write_opcode(&emitter->_bytecode, code);       \
        FOR_ALL(IMPL_EMIT_ARG __VA_OPT__(, __VA_ARGS__))        \
    }
FOR_OPERATIONS(IMPL_EMIT)

#define IMPL_EMIT_SYS(code, mnemo, ...)                         \
    void emit_sys_##mnemo(                                      \
        Emitter* emitter __VA_OPT__(,)                          \
        FOR_ALL(DECL_EMIT_ARG __VA_OPT__(, __VA_ARGS__))        \
    ) {                                                         \
        bytecode_write_opcode(&emitter->_bytecode, OP_SYS);     \
        bytecode_write_syscall(&emitter->_bytecode, code);      \
        FOR_ALL(IMPL_EMIT_ARG __VA_OPT__(, __VA_ARGS__))        \
    }
FOR_SYSCALLS(IMPL_EMIT_SYS)

void emitter_free(Emitter* emitter) {
    array_buf_free(usize)(&emitter->_symbol_locations);
    array_buf_free(usize)(&emitter->_symbol_ref_locations);
    array_buf_free(Byteword)(&emitter->_bytecode.instructions);
    array_buf_free(Byteword)(&emitter->_bytecode.rodata);
}

#define UNSET_LOCATION ((usize)-1)
bool emitter_finish(Emitter* p_emitter, Bytecode* dst) {
    Emitter emitter = *p_emitter;

    for (usize i = 0; i < emitter._symbol_ref_locations.len; i++) {
        usize ref_location = emitter._symbol_ref_locations.data[i];
        Byteword* const ref_ptr =
            emitter._bytecode.instructions.data + ref_location;
        Byteword const* reader = ref_ptr;
        usize symbol_index = bytecode_read_location(&reader);
        usize symbol_location = emitter._symbol_locations.data[symbol_index];
        if (symbol_location >= emitter._bytecode.instructions.len) {
            return false;
        }
        Byteword* writer = ref_ptr;
        bytecode_write_location_at(&writer, symbol_location);
    }

    *dst = emitter._bytecode;
    array_buf_free(usize)(&emitter._symbol_locations);
    array_buf_free(usize)(&emitter._symbol_ref_locations);

    return true;
}

SymbolIndex emit_new_symbol(Emitter* emitter) {
    SymbolIndex symbol_index = emitter->_next_symbol++;
    array_buf_push(usize)(&emitter->_symbol_locations, UNSET_LOCATION);
    return symbol_index;
}

SymbolIndex emit_many_new_symbols(Emitter* emitter, usize n) {
    SymbolIndex first = emitter->_next_symbol;
    // FIXME: optimize
    for (size_t i = 0; i < n; i++) {
        emit_new_symbol(emitter);
    }
    return first;
}

bool emit_symbol_location(Emitter* emitter, SymbolIndex symbol_index) {
    if (emitter->_symbol_locations.data[symbol_index] != UNSET_LOCATION) {
        return false;
    }
    emitter->_symbol_locations.data[symbol_index] =
        emitter->_bytecode.instructions.len;
    return true;
}
