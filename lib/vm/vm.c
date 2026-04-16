#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "util/alloc/alloc.h"
#include "vm/vm.h"
#include "vm/diagnostics.h"

Vm vm_new(VmSystem* system, Bytecode bytecode, Reporter* reporter) {
    Word* value_stack_data = malloc_or_exit(8 * sizeof(Word));
    VmValueStack value_stack = {
        .data = value_stack_data,
        .top = value_stack_data,
        .capacity = 8,
    };

    void* frame_data = malloc_or_exit(64);
    VmFrameStack frames = {
        .data = frame_data,
        .top = frame_data,
        .local_variables = frame_data,
        .capacity = 64,
    };

    return (Vm){
        .system = system,
        .reporter = reporter,
        .bytecode = bytecode,
        .ip = bytecode.instructions.data,
        .value_stack = value_stack,
        .frames = frames,
    };
}

void vm_free(Vm* vm) {
    free(vm->value_stack.data);
    free(vm->frames.data);
}

static void push(Vm* vm, Word value) {
    VmValueStack stack = vm->value_stack;
    usize len = stack.top - stack.data;
    if (len == stack.capacity) {
        stack.capacity *= 2;
        stack.data = realloc_or_exit(stack.data, stack.capacity * sizeof(Word));
        stack.top = stack.data + len;
        vm->value_stack = stack;
    }
    *(vm->value_stack.top++) = value;
}

static Word pop(Vm* vm) {
    return *(--vm->value_stack.top);
}

// reserves `size` bytes after `vm->stack.local_variables`.
static void* frames_alloc(Vm* vm, usize size) {
    VmFrameStack frames = vm->frames;
    usize len = frames.top - frames.data;
    usize min_capacity = len + size;
    if (frames.capacity >= min_capacity) {
        vm->frames.top += size;
        return frames.top;
    }
    usize new_capacity = (2 * frames.capacity >= min_capacity) ? 2 * frames.capacity : min_capacity;
    usize var_offset = (void*)frames.local_variables - frames.data;
    frames.data = realloc_or_exit(frames.data, frames.capacity);
    frames.top = frames.data + len + size;
    frames.local_variables = frames.data + var_offset;
    frames.capacity = new_capacity;
    vm->frames = frames;
    return frames.data + len;
}

typedef enum ControlFlow {
    FLOW_EXIT = 0,
    FLOW_CONTINUE = 1,
} ControlFlow;

static ControlFlow run_one(Vm* vm);

#define Arg(mnemo) Arg_##mnemo
#define Arg_imb Byteword
#define Arg_imw Word
#define Arg_loc usize
#define Arg_var usize

#define DECL_ARG(idx, mnemo, ...) , Arg(mnemo) x##idx

#define DECL_OP_FN(code, mnemo, ...)                            \
    static ControlFlow op_##mnemo(                              \
        Vm* vm                                                  \
        FOR_ALL(DECL_ARG __VA_OPT__(, __VA_ARGS__))             \
    );
FOR_OPERATIONS(DECL_OP_FN)

#define DECL_SYS_FN(code, mnemo, ...)                           \
    static ControlFlow sys_##mnemo(                             \
        Vm* vm                                                  \
        FOR_ALL(DECL_ARG __VA_OPT__(, __VA_ARGS__))             \
    );
FOR_SYSCALLS(DECL_SYS_FN)

void vm_run(Vm* vm) {
    while (true){
        if (run_one(vm) == FLOW_EXIT) {
            return;
        }
    }
}

static Opcode fetch_op(Vm* vm) {
    return bytecode_read_opcode(&vm->ip);
}

static Syscall fetch_sys(Vm* vm) {
    return bytecode_read_syscall(&vm->ip);
}

static Byteword fetch_imb(Vm* vm) {
    return bytecode_read_byteword(&vm->ip);
}

static Word fetch_imw(Vm* vm) {
    return bytecode_read_word(&vm->ip);
}

static usize fetch_loc(Vm* vm) {
    return bytecode_read_location(&vm->ip);
}

static usize fetch_var(Vm* vm) {
    return bytecode_read_variable_index(&vm->ip);
}

static ControlFlow run_one(Vm* vm) {
    Opcode opcode = fetch_op(vm);
    #define FETCH(idx, kind, ...) , fetch_##kind(vm)
    switch (opcode) {
    #define RUN_ONE_CASE(code, mnemo, ...)                  \
        case code:                                          \
            return op_##mnemo(                              \
                vm                                          \
                FOR_ALL(FETCH __VA_OPT__(, __VA_ARGS__))    \
            );
    FOR_OPERATIONS(RUN_ONE_CASE)

    case OP_SYS:;
        Syscall syscall = fetch_sys(vm);
        switch (syscall) {
        #define RUN_ONE_SYS_CASE(code, mnemo, ...)              \
            case code:                                          \
                return sys_##mnemo(                             \
                    vm                                          \
                    FOR_ALL(FETCH __VA_OPT__(, __VA_ARGS__))    \
                );
        FOR_SYSCALLS(RUN_ONE_SYS_CASE)
        default:
            report_start(vm->reporter, SEVERITY_ERROR, RE_INVALID_INSTRUCTION);
            report_message(vm->reporter, format(
                "unknown opcode `0x%"PRIu16"x` at byte 0x%zx",
                opcode,
                sizeof(Byteword) * (usize)(vm->ip - vm->bytecode.instructions.data - 1)
            ));
            report_end(vm->reporter);
            return FLOW_EXIT;
        }

    default:
        report_start(vm->reporter, SEVERITY_ERROR, RE_INVALID_INSTRUCTION);
        report_message(vm->reporter, format(
            "unknown syscall code `0x%"PRIu16"x` at byte 0x%zx",
            opcode,
            sizeof(Byteword) * (usize)(vm->ip - vm->bytecode.instructions.data - 1)
        ));
        report_end(vm->reporter);
        return FLOW_EXIT;
    }
}

static ControlFlow op_nop(Vm* vm) {
    return FLOW_CONTINUE;
}

static ControlFlow op_cal(Vm* vm) {
    usize loc = pop(vm).as_uint;
    VmFrame frame = {
        .return_ip = vm->ip,
        .return_local_variables = (void*)vm->frames.local_variables - vm->frames.data,
    };
    *(VmFrame*)frames_alloc(vm, sizeof(frame)) = frame;
    vm->frames.local_variables = vm->frames.top;
    vm->ip = vm->bytecode.instructions.data + loc;
    return FLOW_CONTINUE;
}

static ControlFlow op_res(Vm* vm, Byteword nregs) {
    Word* local_variables = frames_alloc(vm, nregs * sizeof(Word));
    memset(local_variables, 0, nregs * sizeof(Word));
    return FLOW_CONTINUE;
}

static ControlFlow op_ret(Vm* vm) {
    VmFrame* frame = (VmFrame*)vm->frames.local_variables - 1;
    vm->frames.top = (void*)frame;
    vm->frames.local_variables = (Word*)(vm->frames.data + frame->return_local_variables);
    vm->ip = frame->return_ip;
    return FLOW_CONTINUE;
}

static ControlFlow op_sca(Vm* vm, Word val) {
    push(vm, val);
    return FLOW_CONTINUE;
}

static ControlFlow op_loc(Vm* vm, usize val) {
    push(vm, (Word){ .as_uint = val });
    return FLOW_CONTINUE;
}

static ControlFlow op_var(Vm* vm, usize var_index) {
    push(vm, vm->frames.local_variables[var_index]);
    return FLOW_CONTINUE;
}

static ControlFlow op_set(Vm* vm, usize var_index) {
    vm->frames.local_variables[var_index] = pop(vm);
    return FLOW_CONTINUE;
}

static ControlFlow op_pop(Vm* vm) {
    pop(vm);
    return FLOW_CONTINUE;
}

static ControlFlow op_jmp(Vm* vm, usize dst) {
    vm->ip = vm->bytecode.instructions.data + dst;
    return FLOW_CONTINUE;
}

static ControlFlow op_jnz(Vm* vm, usize dst) {
    if (pop(vm).as_uint != 0) {
        vm->ip = vm->bytecode.instructions.data + dst;
    }
    return FLOW_CONTINUE;
}

static ControlFlow op_jze(Vm* vm, usize dst) {
    if (pop(vm).as_uint == 0) {
        vm->ip = vm->bytecode.instructions.data + dst;
    }
    return FLOW_CONTINUE;
}

static ControlFlow op_not(Vm* vm) {
    push(vm, (Word){ .as_uint = ~pop(vm).as_uint });
    return FLOW_CONTINUE;
}

// LHS is lower value, RHS is upper value
#define IMPL_BINARY_COMPARISON(mnemo, type, op)                 \
    static ControlFlow op_##mnemo(Vm* vm) {                     \
        Word rhs = pop(vm);                                     \
        Word lhs = pop(vm);                                     \
        push(vm, (Word){                                        \
            .as_uint = -(lhs.as_##type op rhs.as_##type)        \
        });                                                     \
        return FLOW_CONTINUE;                                   \
    }

IMPL_BINARY_COMPARISON(equ, uint, ==)
IMPL_BINARY_COMPARISON(neu, uint, !=)
IMPL_BINARY_COMPARISON(geu, uint, >=)
IMPL_BINARY_COMPARISON(gtu, uint, >)
IMPL_BINARY_COMPARISON(leu, uint, <=)
IMPL_BINARY_COMPARISON(ltu, uint, <)

IMPL_BINARY_COMPARISON(eqi, int, ==)
IMPL_BINARY_COMPARISON(nei, int, !=)
IMPL_BINARY_COMPARISON(gei, int, >=)
IMPL_BINARY_COMPARISON(gti, int, >)
IMPL_BINARY_COMPARISON(lei, int, <=)
IMPL_BINARY_COMPARISON(lti, int, <)

IMPL_BINARY_COMPARISON(eqf, float, ==)
IMPL_BINARY_COMPARISON(nef, float, !=)
IMPL_BINARY_COMPARISON(gef, float, >=)
IMPL_BINARY_COMPARISON(gtf, float, >)
IMPL_BINARY_COMPARISON(lef, float, <=)
IMPL_BINARY_COMPARISON(ltf, float, <)

#define IMPL_BINARY_OPERATION(mnemo, type, op)                  \
    static ControlFlow op_##mnemo(Vm* vm) {                     \
        Word rhs = pop(vm);                                     \
        Word lhs = pop(vm);                                     \
        push(vm, (Word){                                        \
            .as_##type = lhs.as_##type op rhs.as_##type         \
        });                                                     \
        return FLOW_CONTINUE;                                   \
    }

IMPL_BINARY_OPERATION(lor, uint, |)
IMPL_BINARY_OPERATION(and, uint, &)
IMPL_BINARY_OPERATION(xor, uint, ^)

// FIXME: check for overflow & division by zero

IMPL_BINARY_OPERATION(adu, uint, +)
IMPL_BINARY_OPERATION(sbu, uint, -)
IMPL_BINARY_OPERATION(mlu, uint, *)
IMPL_BINARY_OPERATION(dvu, uint, /)

static ControlFlow op_ngi(Vm* vm) {
    push(vm, (Word){ .as_int = -pop(vm).as_int });
    return FLOW_CONTINUE;
}

IMPL_BINARY_OPERATION(adi, int, +)
IMPL_BINARY_OPERATION(sbi, int, -)
IMPL_BINARY_OPERATION(mli, int, *)
IMPL_BINARY_OPERATION(dvi, int, /)

static ControlFlow op_ngf(Vm* vm) {
    push(vm, (Word){ .as_float = -pop(vm).as_float });
    return FLOW_CONTINUE;
}

IMPL_BINARY_OPERATION(adf, float, +)
IMPL_BINARY_OPERATION(sbf, float, -)
IMPL_BINARY_OPERATION(mlf, float, *)
IMPL_BINARY_OPERATION(dvf, float, /)

static ControlFlow sys_nop(Vm* vm) {
    (vm->system)->vtable->nop(vm->system);
    return FLOW_CONTINUE;
}

static ControlFlow sys_exit(Vm* vm) {
    (vm->system)->vtable->exit(vm->system, pop(vm).as_int);
    return FLOW_EXIT;
}

static ControlFlow sys_hi(Vm* vm) {
    (vm->system)->vtable->hi(vm->system);
    return FLOW_CONTINUE;
}

static ControlFlow sys_bye(Vm* vm) {
    (vm->system)->vtable->bye(vm->system);
    return FLOW_CONTINUE;
}

static ControlFlow sys_dbg(Vm* vm, usize var_index) {
    Word val = vm->frames.local_variables[var_index];
    (vm->system)->vtable->dbg(vm->system, var_index, val);
    return FLOW_CONTINUE;
}
