#pragma once

#ifdef AUDIENCE_COMPILING_INNER

#ifdef WIN32
#define AUDIENCE_EXPORT __declspec(dllexport)
#else
#define AUDIENCE_EXPORT
#endif

extern "C" AUDIENCE_EXPORT bool audience_inner_init();
extern "C" AUDIENCE_EXPORT void *audience_inner_window_create(const wchar_t *const title, const wchar_t *const url);
extern "C" AUDIENCE_EXPORT void audience_inner_window_destroy(void *window);
extern "C" AUDIENCE_EXPORT void audience_inner_loop();

#else

typedef bool (*audience_inner_init_t)();
typedef void *(*audience_inner_window_create_t)(const wchar_t *const title, const wchar_t *const url);
typedef void (*audience_inner_window_destroy_t)(void *window);
typedef void (*audience_inner_loop_t)();

#endif
