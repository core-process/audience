#pragma once

#include "../../common/safefn.h"
#include "nucleus.h"

#define NUCLEUS_SAFE_FN_EXPAND( x ) x

#if defined(NUCLEUS_TRANSLATE_EXCEPTIONS)
#define NUCLEUS_SAFE_FN(...) NUCLEUS_SAFE_FN_EXPAND(SAFE_FN(__VA_ARGS__, NUCLEUS_TRANSLATE_EXCEPTIONS))
#else
#define NUCLEUS_SAFE_FN(...) NUCLEUS_SAFE_FN_EXPAND(SAFE_FN(__VA_ARGS__))
#endif
