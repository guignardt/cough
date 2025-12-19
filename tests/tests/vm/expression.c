#include "tests/common.h"

int main(int argc, const char* argv[]) {
    TestVmSystem vm_system = test_vm_system_new();
    TestReporter reporter = test_reporter_new();

    char const* assembly[] = {
        ":entry",
        "   sca 0",
        "   loc :main",
        "   cal",
        "   sca 0",
        "   sys exit",
        "",
        ":main",
        "   res 1",
        "   loc :combo",
        "   cal",
        "   set %0",
        "   sys dbg %0",
        "   sca 0",
        "   sys exit",
        "",
        ":combo",
        "   res 2",
        "   set %0",
        "   var %0",
        "   sca 0",
        "   not",
        "   lor",
        "   ret"
    };
    Bytecode bytecode = assembly_to_bytecode(assembly, sizeof(assembly) / sizeof(char*));

    Vm vm = vm_new((VmSystem*)&vm_system, bytecode, (Reporter*)&reporter);
    vm_run(&vm);

    assert(vm_system.syscalls.len == 2);

    SyscallRecord dbg = vm_system.syscalls.data[0];
    assert(dbg.kind == SYS_DBG);
    assert(dbg.as.dbg.var_val.as_uint == -1);

    return 0;
}
