#pragma once

#include <stdalign.h>

#include "collections/array.h"
#include "ops/eq.h"
#include "ops/hash.h"

typedef union Word {
    void const* as_ptr;
    void* as_mut_ptr;
    u64 as_uint;
    i64 as_int;
    f64 as_float;
} Word;

typedef u16 Byteword;

typedef enum Opcode {
    /// @brief `nop` -- no operation.
    OP_NOP = 0,

    /// @brief `sys` -- execute a given syscall.
    ///
    /// @param syscall The code of the syscall (16-bit immediate).
    OP_SYS,

    // /// @brief `cas` -- static call.
    // ///
    // /// @param func the location of the first instruction of the function (64-bit immediate).
    // /// Is usually a `frm` instruction.
    // OP_CAS,

    /// @brief `cal` -- dynamic call.
    OP_CAL,

    /// @brief `res` -- reserve the specified number of 64-bit words as
    /// additional space of local variables.
    ///
    /// @param space the number of words to be added (16-bit immediate).
    OP_RES,

    /// @brief `ret` -- return from the current function.
    OP_RET,

    /// @brief `sca` -- push a 64-bit constant to the expression stack.
    ///
    /// @param val the value to be written (64-bit immediate).
    OP_SCA,
    
    /// @brief `loc` -- push a location to the expression stack.
    ///
    /// @param val the location to be written (64-bit immediate).
    OP_LOC,

    /// @brief `var` -- push a 64-bit value from a local variable to the
    /// expression stack.
    ///
    /// @param var the index of the variable.
    OP_VAR,

    /// @brief `set` -- move the value from the top of the stack to a local
    /// variable.
    ///
    /// @param var the index of the variable.
    OP_SET,
    
    /// @brief `pop` -- removes the value at the top of the stack.
    OP_POP,

    /// @brief `jmp` -- jump to the specified memory location.
    ///
    /// @param loc The location of the instruction to jump to.
    OP_JMP,

    /// @brief `jnz` -- pops the stack jump to the specified memory location if
    /// the popped value is non-zero.
    ///
    /// @param loc The location of the instruction to jump to.
    OP_JNZ,

    /// @brief `equ` -- check if two `UInt`'s are equal. Pops the two values at
    /// the top of the stack, and pops `1` if they are equal, and `0` otherwise.
    OP_EQU,

    /// @brief `equ` -- check if two `UInt`'s are different. Pops the two
    /// values at the top of the stack, and pops `1` if they are equal, and `0`
    /// otherwise.
    OP_NEU,

    /// @brief `equ` -- check if one `UInt` is greater than another. Pops the
    /// two values at the top of the stack, and pops `1` if the lower one is
    /// greater or equal to the second one, and `0` otherwise.
    OP_GEU,

    /// @brief `equ` -- check if one `UInt` is greater than another. Pops the
    /// two values at the top of the stack, and pops `1` if the lower one is
    /// greater than the second one, and `0` otherwise.
    OP_GTU,

    /// @brief `not` -- compute the bitwise not of a value.
    OP_NOT,

    /// @brief `lor` -- compute the bitwise or of a value.
    OP_LOR,

    /// @brief `and` -- compute the bitwise and of a value.
    OP_AND,

    /// @brief `adu` -- add two `UInt`s together. Pops the two values at the
    /// top of the stack and pushes their sum.
    OP_ADU,

    OPCODES_LEN,
} Opcode;

typedef enum Syscall {
    SYS_NOP,

    /// @brief `sys exit` -- exit the program with the specified exit code.
    SYS_EXIT,

    SYS_HI,
    SYS_BYE,

    /// @brief `sys dbg` -- print the content of a 64-bit local variable as a
    /// `UInt`.
    ///
    /// @param var the variable.
    SYS_DBG,

    SYSCALLS_LEN,
} Syscall;

DECL_ARRAY_BUF(Byteword);

typedef struct Bytecode {
    ArrayBuf(Byteword) rodata;
    ArrayBuf(Byteword) instructions;
} Bytecode;

Opcode bytecode_read_opcode(const Byteword** ip);
Syscall bytecode_read_syscall(const Byteword** ip);
Byteword bytecode_read_byteword(const Byteword** ip);
Word bytecode_read_word(const Byteword** ip);
usize bytecode_read_location(const Byteword** ip);
usize bytecode_read_variable_index(const Byteword** ip);

#define bytecode_read(kind) bytecode_read_##kind
#define bytecode_read_sys bytecode_read_syscall
#define bytecode_read_imb bytecode_read_byteword
#define bytecode_read_imw bytecode_read_word
#define bytecode_read_loc bytecode_read_location
#define bytecode_read_var bytecode_read_variable_index

void bytecode_write_opcode(Bytecode* bytecode, Opcode opcode);
void bytecode_write_syscall(Bytecode* bytecode, Syscall syscall);
void bytecode_write_byteword(Bytecode* bytecode, Byteword byteword);
void bytecode_write_word(Bytecode* bytecode, Word word);
void bytecode_write_variable_index(Bytecode* bytecode, usize variable_index);
void bytecode_write_location(Bytecode* bytecode, usize symbol);
void bytecode_write_location_at(Byteword** ip, usize symbol);

#define bytecode_write(kind) bytecode_write_##kind
#define bytecode_write_sys bytecode_write_syscall
#define bytecode_write_imb bytecode_write_byteword
#define bytecode_write_imw bytecode_write_word
#define bytecode_write_loc bytecode_write_location
#define bytecode_write_var bytecode_write_variable_index

#define FOR_OPERATIONS(proc)    \
    proc(OP_NOP, nop)           \
    proc(OP_CAL, cal)           \
    proc(OP_RES, res, imb)      \
    proc(OP_RET, ret)           \
    proc(OP_SCA, sca, imw)      \
    proc(OP_LOC, loc, loc)      \
    proc(OP_VAR, var, var)      \
    proc(OP_SET, set, var)      \
    proc(OP_POP, pop)           \
    proc(OP_JMP, jmp, loc)      \
    proc(OP_JNZ, jnz, loc)      \
    proc(OP_EQU, equ)           \
    proc(OP_NEU, neu)           \
    proc(OP_GEU, geu)           \
    proc(OP_GTU, gtu)           \
    proc(OP_NOT, not)           \
    proc(OP_LOR, lor)           \
    proc(OP_AND, and)           \
    proc(OP_ADU, adu)           \

#define FOR_SYSCALLS(proc)      \
    proc(SYS_NOP, nop)          \
    proc(SYS_EXIT, exit)        \
    proc(SYS_HI, hi)            \
    proc(SYS_BYE, bye)          \
    proc(SYS_DBG, dbg, var)     \

#define FOR_ALL(proc, ...)      \
    __VA_OPT__(FOR_ALL1(proc, __VA_ARGS__))
#define FOR_ALL1(proc, a, ...)  \
    proc(0, a __VA_OPT__(,0)) __VA_OPT__(FOR_ALL2(proc, __VA_ARGS__))
#define FOR_ALL2(proc, b, ...)  \
    proc(1, b __VA_OPT__(,0)) __VA_OPT__(FOR_ALL3(proc, __VA_ARGS__))
#define FOR_ALL3(proc, c, ...)  \
    proc(2, c)

typedef struct Mnemonic {
    alignas(u64) char chars[8];
} Mnemonic;
bool eq(Mnemonic)(Mnemonic a, Mnemonic b);
void hash(Mnemonic)(Hasher* hasher, Mnemonic mnemo);

extern Mnemonic instruction_mnemonics[OPCODES_LEN];
extern Mnemonic syscall_mnemonics[SYSCALLS_LEN];
