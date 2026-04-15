#pragma once

#include "bytecode/bytecode.h"
#include "util/collections/array.h"

typedef usize SymbolIndex;

typedef struct Emitter {
    Bytecode _bytecode;
    ArrayBuf(usize) _symbol_locations;
    ArrayBuf(usize) _symbol_ref_locations;
    SymbolIndex _next_symbol;
} Emitter;

Emitter emitter_new(void);
void emitter_free(Emitter* emitter);
bool emitter_finish(Emitter* emitter, Bytecode* dst);

// these are guaranteed to be 0, 1, 2, ...
SymbolIndex emit_new_symbol(Emitter* emitter);
// emits the first symbol
SymbolIndex emit_many_new_symbols(Emitter* emitter, usize n);
bool emit_symbol_location(Emitter* emitter, SymbolIndex symbol_index);

#define EmitArg(kind) EmitArg_##kind
#define EmitArg_imb Byteword
#define EmitArg_imw Word
#define EmitArg_loc SymbolIndex
#define EmitArg_var usize

#define DECL_EMIT_ARG(idx, kind, ...)                       \
    EmitArg(kind) x##idx __VA_OPT__(,)

#define DECL_EMIT(code, mnemo, ...)                         \
    void emit_##mnemo(                                      \
        Emitter* emitter __VA_OPT__(,)                      \
        FOR_ALL(DECL_EMIT_ARG __VA_OPT__(, __VA_ARGS__))    \
    );
FOR_OPERATIONS(DECL_EMIT)
#undef DECL_EMIT

#define DECL_EMIT_SYS(code, mnemo, ...)                     \
    void emit_sys_##mnemo(                                  \
        Emitter* emitter __VA_OPT__(,)                      \
        FOR_ALL(DECL_EMIT_ARG __VA_OPT__(, __VA_ARGS__))    \
    );
FOR_SYSCALLS(DECL_EMIT_SYS)
#undef DECL_EMIT_SYS

#undef DECL_EMIT_ARG

#define emit(mnemo) emit_##mnemo
#define emit_sys(mnemo) emit_sys_##mnemo
