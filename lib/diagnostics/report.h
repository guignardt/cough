#pragma once

#include <stdio.h>

#include "util/collections/array.h"
#include "util/collections/string.h"
#include "diagnostics/log.h"

typedef struct Reporter {
    struct ReporterVTable const* vtable;
} Reporter;

typedef struct ReporterVTable {
    // FIXME: exact integer type for code
    void(*start)(Reporter* self, Severity severity, i32 code);
    void(*end)(Reporter* self);
    void(*message)(Reporter* self, StringBuf message);
    void(*source_code)(Reporter* self, Range source_code);
    usize(*error_count)(Reporter const* self);
} ReporterVTable;

void report_start(Reporter* reporter, Severity severity, i32 code);
void report_end(Reporter* reporter);
void report_message(Reporter* reporter, StringBuf message);
void report_source_code(Reporter* reporter, Range source_code);
usize reporter_error_count(const Reporter* reporter);
