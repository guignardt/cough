#include <string.h>

#include "diagnostics/log.h"
#include "diagnostics/errno.h"

void log_errno(Errno err) {
    log_errno_or(err, "unknown error");
}

void log_errno_or(Errno err, const char* unknown_msg) {
    if (!err) return;
    if (err == DUMMY_ERRNO) log_system_error("%s", unknown_msg);
    else log_system_error("%s (errno %d)", strerror(err), err);
}
