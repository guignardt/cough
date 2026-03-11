#pragma once

/// @brief The result type.
typedef enum Result {
    /// @brief Indicates the callee has returned properly and that the caller
    /// may continue normally.
    ///
    /// This does not mean that no error has been emitted -- rather, that all
    /// errors have been contained and that the current stage can continue like
    /// no errors have occured. However, if an error has occured (which is
    /// recorded by the reporter), the next stage may not be executed.
    OK = 0,

    /// @brief Indicates that the callee has failed, and that the caller may not
    /// continue executing right away.
    ///
    /// For example, a parsing function that has returned `ERROR` might have
    /// left the parser in the middle of e. g. an expression, and that the remaining
    /// token stream does not start with valid Cough code.
    ///
    /// The caller is then responsible for putting the stage back into a state
    /// where it can continue, or to propagate the `ERROR` upstream.
    ///
    /// Regardless of success or failure, the *callee* is responsible for managing
    /// its resources like allocated memory.
    ERROR = -1,
} Result;

typedef int Errno;

#define DUMMY_ERRNO ((Errno)0xAAAAAAAA)
