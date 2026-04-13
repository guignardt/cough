#pragma once

#include "util/primitives/primitives.h"
#include "bytecode/bytecode.h"

typedef struct VmSystem VmSystem;

typedef struct VmSystemVTable {
    void(*nop)(VmSystem* self);
    void(*exit)(VmSystem* self, i64 exit_code);
    void(*hi)(VmSystem* self);
    void(*bye)(VmSystem* self);
    void(*dbg)(VmSystem* self, usize var_idx, Word var_val);
} VmSystemVTable;

typedef struct VmSystem {
    const VmSystemVTable* vtable;
} VmSystem;

typedef struct DefaultVmSystem {
    VmSystem base;
} DefaultVmSystem;

DefaultVmSystem new_default_vm_system(void);
void free_default_vm_system(DefaultVmSystem system);
