#pragma once

#include "util/collections/string.h"
#include "bytecode/bytecode.h"
#include "diagnostics/result.h"
#include "diagnostics/report.h"

Result assemble(String assembly, Reporter* reporter, Bytecode* dst);
