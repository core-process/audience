#pragma once

#include "../../common/safefn.h"
#include "nucleus.h"

#if defined(NUCLEUS_TRANSLATE_EXCEPTIONS)
#define NUCLEUS_SAFE_FN(...) SAFE_FN(__VA_ARGS__, NUCLEUS_TRANSLATE_EXCEPTIONS)
#else
#define NUCLEUS_SAFE_FN(...) SAFE_FN(__VA_ARGS__)
#endif
