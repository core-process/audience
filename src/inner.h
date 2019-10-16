#pragma once

#ifdef AUDIENCE_COMPILING_INNER

extern "C" __declspec(dllexport) bool audience_inner_init();
extern "C" __declspec(dllexport) void *audience_inner_window_create(const wchar_t *const title, const wchar_t *const url);
extern "C" __declspec(dllexport) void audience_inner_window_destroy(void *window);
extern "C" __declspec(dllexport) void audience_inner_loop();

#else

typedef bool (*audience_inner_init_t)();
typedef void *(*audience_inner_window_create_t)(const wchar_t *const title, const wchar_t *const url);
typedef void (*audience_inner_window_destroy_t)(void *window);
typedef void (*audience_inner_loop_t)();

#endif
