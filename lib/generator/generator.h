#pragma once

#include "ast/ast.h"
#include "emitter/emitter.h"
#include "diagnostics/report.h"

void generate_module(Module module, AstData data, Emitter* emitter);
