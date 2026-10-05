#pragma once

// Minimal configuration header for LEMON when the generated config is absent.
// These defaults cover the portions of the library used by qasmtrans.

#define LEMON_VERSION_MAJOR 1
#define LEMON_VERSION_MINOR 3
#define LEMON_VERSION_PATCH 1
#define LEMON_VERSION_TWEAK 0
#define LEMON_VERSION "1.3.1"

#define LEMON_HAVE_LONG_LONG 1
#define LEMON_HAVE_LONG_DOUBLE 1

#define LEMON_USE_PTHREAD 0
#define LEMON_USE_WIN32_THREADS 0
#define LEMON_USE_OPENMP 0

#define LEMON_NO_DEPRECATED_WARNINGS 0
#define LEMON_NO_UNUSED_LOCAL_TYPEDEF_WARNINGS 1

