#pragma once

#include <assert.h>

#include "util/collections/array.h"

#include "diagnostics/report.h"

#include "source/source.h"
#include "tokens/token.h"
#include "ast/ast.h"
#include "bytecode/bytecode.h"

#include "tokenizer/tokenizer.h"
#include "parser/parser.h"
#include "analyzer/analyzer.h"
#include "generator/generator.h"
#include "assembler/assembler.h"

#include "vm/system.h"
#include "vm/vm.h"

typedef struct TestReporter {
    Reporter base;
    ArrayBuf(i32) error_codes;
} TestReporter;

TestReporter test_reporter_new(void);
void test_reporter_free(TestReporter* reporter);

typedef struct CrashingReporter {
    Reporter base;
    SourceText source;
    Severity severity;
} CrashingReporter;

CrashingReporter crashing_reporter_new(SourceText source);

typedef struct SyscallRecord {
    Syscall kind;
    union {
        struct {
            i64 exit_code;
        } exit;
        struct {
            usize var_idx;
            Word var_val;
        } dbg;
    } as;
} SyscallRecord;
DECL_ARRAY_BUF(SyscallRecord)

typedef struct TestVmSystem {
    VmSystem base;
    ArrayBuf(SyscallRecord) syscalls;
} TestVmSystem;

TestVmSystem test_vm_system_new(void);
void test_vm_system_free(TestVmSystem system);

void source_to_module(String source, Module* dst, AstData* dst_data);
void source_to_expression(String source, AstData* data, ExpressionId* dst);
Bytecode source_to_module_bytecode(String source);
Bytecode assembly_to_bytecode(char const** parts, usize count);
