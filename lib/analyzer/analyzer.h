#pragma once

#include "ast/ast.h"
#include "diagnostics/report.h"

typedef enum AnalyzerError {
    CE_ANALYZER_ERROR = 3000,
    CE_DUPLICATE_BINDING_NAME,
    CE_EXPECTED_CONSTANT,
    CE_IMPLICIT_TYPE,
    CE_MISMATCHED_TYPES,
    CE_BINDING_NOT_FOUND,
    CE_INVALID_BINDING_KIND,
} AnalyzerError;

bool analyze(Ast* ast, Reporter* reporter);
