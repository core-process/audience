#pragma once

#include <wchar.h>

#ifdef WIN32
#define AUDIENCE_NUCLEUS_EXPORT __declspec(dllexport)
#else
#define AUDIENCE_NUCLEUS_EXPORT
#endif

#if __cplusplus
extern "C" {
#endif

AUDIENCE_NUCLEUS_EXPORT bool audience_inner_init();
AUDIENCE_NUCLEUS_EXPORT void *audience_inner_window_create(const wchar_t *const title, const wchar_t *const url);
AUDIENCE_NUCLEUS_EXPORT void audience_inner_window_destroy(void *window);
AUDIENCE_NUCLEUS_EXPORT void audience_inner_loop();

#if __cplusplus
}
#endif
