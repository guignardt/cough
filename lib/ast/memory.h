#pragma once

#include "util/collections/array.h"

typedef void* Allocation;
DECL_ARRAY_BUF(Allocation)

typedef struct AstStorage {
    ArrayBuf(Allocation) _allocations;
} AstStorage;

AstStorage ast_storage_new(void);
void ast_storage_free(AstStorage* storage);

void ast_store(AstStorage* storage, void* allocation);
