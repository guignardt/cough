#pragma once

#include <errno.h>

typedef int Errno;

#define DUMMY_ERRNO ((Errno)0xAAAAAAAA)

void log_errno(Errno err);
void log_errno_or(Errno err, const char* unknown_msg);
