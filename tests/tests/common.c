#include "tests/common.h"

#include "source/source.h"

IMPL_ARRAY_BUF(SyscallRecord)

static void test_reporter_start(Reporter* raw, Severity severity, i32 code) {
    TestReporter* self = (TestReporter*)raw;
    array_buf_push(i32)(&self->error_codes, code);
}

static void test_reporter_end(Reporter* self) {}

static void test_reporter_message(Reporter* self, StringBuf message) {}

static void test_reporter_source_code(Reporter* self, Range source_code) {}

static usize test_reporter_error_count(const Reporter* raw) {
    const TestReporter* self = (const TestReporter*)raw;
    return self->error_codes.len;
}

static const ReporterVTable test_reporter_vtable = {
    .start = test_reporter_start,
    .end = test_reporter_end,
    .message = test_reporter_message,
    .source_code = test_reporter_source_code,
    .error_count = test_reporter_error_count,
};

TestReporter test_reporter_new(void) {
    return (TestReporter){
        .base.vtable = &test_reporter_vtable,
        .error_codes = array_buf_new(i32)()
    };
}

void test_reporter_free(TestReporter* reporter) {
    array_buf_free(i32)(&reporter->error_codes);
}

static void crashing_report_start(Reporter* raw, Severity severity, i32 code) {
    CrashingReporter* self = (CrashingReporter*)raw;
    self->severity = severity;
}

static void crashing_report_end(Reporter* raw) {
    exit(-1);
}

static void crashing_report_message(Reporter* raw, StringBuf message) {
    CrashingReporter* self = (CrashingReporter*)raw;
    log_diagnostic(self->severity, "%.*s", (int)message.len, message.data);
}

static void crashing_report_souce_code(Reporter* raw, Range source) {
    CrashingReporter* self = (CrashingReporter*)raw;
    LineColumn start = source_text_position(self->source, source.start);
    LineColumn end = source_text_position(self->source, source.end);
    char const* path = self->source.path ? self->source.path : "<stdin>";
    eprintf(
        "at %s:%zu:%zu-%zu:%zu: %.*s\n",
        path,
        start.line + 1, start.column + 1,
        end.line + 1, end.column + 1,
        (int)(source.end - source.start),
        self->source.text + source.start
    );
}

static usize crashing_report_error_count(Reporter const* raw) {
    return 0;
}

static ReporterVTable const crashing_reporter_vtable = {
    .start = crashing_report_start,
    .end = crashing_report_end,
    .message = crashing_report_message,
    .source_code = crashing_report_souce_code,
    .error_count = crashing_report_error_count,
};

CrashingReporter crashing_reporter_new(SourceText source) {
    return (CrashingReporter){
        .base.vtable = &crashing_reporter_vtable,
        .source = source,
        .severity = 0,
    };
}

static void test_vm_system_nop(VmSystem* raw) {
    TestVmSystem* self = (TestVmSystem*)raw;
    SyscallRecord record = { .kind = SYS_NOP };
    array_buf_push(SyscallRecord)(&self->syscalls, record);
}

static void test_vm_system_exit(VmSystem* raw, i64 exit_code) {
    TestVmSystem* self = (TestVmSystem*)raw;
    SyscallRecord record = {
        .kind = SYS_EXIT,
        .as.exit = { .exit_code = exit_code },
    };
    array_buf_push(SyscallRecord)(&self->syscalls, record);
}

static void test_vm_system_hi(VmSystem* raw) {
    TestVmSystem* self = (TestVmSystem*)raw;
    SyscallRecord record = { .kind = SYS_HI };
    array_buf_push(SyscallRecord)(&self->syscalls, record);
}

static void test_vm_system_bye(VmSystem* raw) {
    TestVmSystem* self = (TestVmSystem*)raw;
    SyscallRecord record = { .kind = SYS_BYE };
    array_buf_push(SyscallRecord)(&self->syscalls, record);
}

static void test_vm_system_dbg(VmSystem* raw, usize var_idx, Word var_val) {
    TestVmSystem* self = (TestVmSystem*)raw;
    SyscallRecord record = {
        .kind = SYS_DBG,
        .as.dbg = {
            .var_idx = var_idx,
            .var_val = var_val,
        },
    };
    array_buf_push(SyscallRecord)(&self->syscalls, record);
}

static const VmSystemVTable test_vm_system_vtable = {
    .nop =  test_vm_system_nop,
    .exit = test_vm_system_exit,
    .hi =   test_vm_system_hi,
    .bye =  test_vm_system_bye,
    .dbg =  test_vm_system_dbg,
};

TestVmSystem test_vm_system_new(void) {
    return (TestVmSystem){
        .base.vtable = &test_vm_system_vtable,
        .syscalls = array_buf_new(SyscallRecord)(),
    };
}

void test_vm_system_free(TestVmSystem system) {
    array_buf_free(SyscallRecord)(&system.syscalls);
}

void source_to_module(String text, Module* dst, AstData* dst_data) {
    SourceText source = source_text_new(NULL, text.data);
    TokenStream tokens;
    CrashingReporter reporter = crashing_reporter_new(source);
    tokenize(text, &reporter.base, &tokens);
    parse_module(tokens, &reporter.base, dst, dst_data);
    analyze_module(dst, dst_data, &reporter.base);
}

void source_to_expression(String text, ExpressionId* dst, AstData* dst_data) {
    SourceText source = source_text_new(NULL, text.data);
    TokenStream tokens;
    CrashingReporter reporter = crashing_reporter_new(source);
    tokenize(text, &reporter.base, &tokens);
    parse_expression(tokens, &reporter.base, dst, dst_data);
    analyze_expression(&dst_data->expressions.data[*dst], dst_data, &reporter.base);
}

Bytecode source_to_module_bytecode(String text) {
    SourceText source = source_text_new(NULL, text.data);
    TokenStream tokens;
    CrashingReporter reporter = crashing_reporter_new(source);
    tokenize(text, &reporter.base, &tokens);
    Module module;
    AstData data;
    parse_module(tokens, &reporter.base, &module, &data);
    analyze_module(&module, &data, &reporter.base);
    Emitter emitter = emitter_new();
    generate_module(module, data, &emitter);
    Bytecode bytecode;
    assert(emitter_finish(&emitter, &bytecode));
    return bytecode;
}

Bytecode assembly_to_bytecode(char const** parts, usize count) {
    StringBuf text = string_buf_new();
    for (usize i = 0; i < count; i++) {
        string_buf_extend(&text, parts[i]);
        string_buf_push(&text, '\n');
    }
    SourceText source = source_text_new(NULL, text.data);
    CrashingReporter reporter = crashing_reporter_new(source);
    Bytecode bytecode;
    // we can ignore the result as we crash on error.
    assemble(
        (String){ .data = text.data, .len = text.len },
        &reporter.base,
        &bytecode
    );
    return bytecode;
}
