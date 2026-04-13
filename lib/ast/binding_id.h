#pragma once

#include "util/collections/array.h"

typedef usize ScopeId;
#define ROOT_SCOPE_ID ((usize)0)

// MSB is one if it's a value, zero otherwise
// other bits are the index
typedef usize BindingId;
DECL_ARRAY_BUF(BindingId);
#define BINDING_ID_INVALID ((BindingId)-1)
