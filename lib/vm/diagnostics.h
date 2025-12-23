#pragma once

#include "diagnostics/report.h"

typedef enum RuntimeErrorKind {
    RE_INVALID_INSTRUCTION,
    RE_INTEGER_OVERFLOW,
} RuntimeErrorKind;

typedef struct RuntimeReporter {
    Reporter base;
    usize error_count;
    Severity severity;
} RuntimeReporter;

RuntimeReporter new_runtime_reporter(void);

void report_simple_runtime_error(
    Reporter* reporter,
    RuntimeErrorKind kind,
    StringBuf message
);
