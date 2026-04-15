#include "bytecode/bytecode.h"
#include "util/ops/ptr.h"

IMPL_ARRAY_BUF(Byteword);

Opcode bytecode_read_opcode(const Byteword** ip) {
    return (Opcode)bytecode_read_byteword(ip);
}

Syscall bytecode_read_syscall(const Byteword** ip) {
    return (Syscall)bytecode_read_byteword(ip);
}

Byteword bytecode_read_byteword(const Byteword** ip) {
    return *((*ip)++);
}

Word bytecode_read_word(const Byteword** ip) {
    const Word* ip_word = (const Word*)align_up(*ip, alignof(Word));
    Word val = *(ip_word++);
    *ip = (const Byteword*)ip_word;
    return val;
}

usize bytecode_read_location(const Byteword** ip) {
    return bytecode_read_word(ip).as_uint;
}

usize bytecode_read_variable_index(const Byteword** ip) {
    return bytecode_read_byteword(ip);
}

void bytecode_write_opcode(Bytecode* bytecode, Opcode opcode) {
    bytecode_write_byteword(bytecode, opcode);
}

void bytecode_write_syscall(Bytecode* bytecode, Syscall syscall) {
    bytecode_write_byteword(bytecode, syscall);
}

void bytecode_write_byteword(Bytecode* bytecode, Byteword byteword) {
    array_buf_push(Byteword)(&bytecode->instructions, byteword);
}

void bytecode_write_word(Bytecode* bytecode, Word word) {
    ArrayBuf(Byteword) instructions = bytecode->instructions;
    while ((instructions.len * sizeof(Byteword)) % alignof(Word) != 0) {
        array_buf_push(Byteword)(&instructions, 0);
    }
    array_buf_extend(Byteword)(
        &instructions,
        (Byteword const*)&word,
        sizeof(Word) / sizeof(Byteword)
    );
    bytecode->instructions = instructions;
}

static void bytecode_write_word_at(Byteword** ip, Word word) {
    // FIXME: better check for alignment
    while ((uptr)(*ip) % alignof(Word) != 0) {
        **ip = 0;
        (*ip)++;
    }
    Word* ip_word = (Word*)*ip;
    *(ip_word++) = word;
    *ip = (Byteword*)ip_word;
}

void bytecode_write_location(Bytecode* bytecode, usize symbol) {
    bytecode_write_word(bytecode, (Word){ .as_uint = symbol });
}

void bytecode_write_location_at(Byteword** ip, usize symbol) {
    bytecode_write_word_at(ip, (Word){ .as_uint = symbol });
}

void bytecode_write_variable_index(Bytecode* bytecode, usize index) {
    // FIXME: check that the variable index fits in a byteword
    bytecode_write_byteword(bytecode, (Byteword)index);
}

bool eq(Mnemonic)(Mnemonic a, Mnemonic b) {
    return *(const u64*)&a == *(const u64*)&b;
}

void hash(Mnemonic)(Hasher* hasher, Mnemonic mnemo) {
    // by design, `Mnemonic` has the layout of `u64`
    union {
        Mnemonic value;
        u64 bits;
    } cast = { .value = mnemo };
    hash(u64)(hasher, cast.bits);
}

#define OPERATION_MNEMONIC(opcode, mnemo, ...) [opcode] = { #mnemo },
Mnemonic instruction_mnemonics[] = {
    FOR_OPERATIONS(OPERATION_MNEMONIC)
    [OP_SYS] = { "sys" }
};

#define SYSCALL_MNEMONIC(syscall, mnemo, ...) [syscall] = { #mnemo },
Mnemonic syscall_mnemonics[] = {
    FOR_SYSCALLS(SYSCALL_MNEMONIC)
};
