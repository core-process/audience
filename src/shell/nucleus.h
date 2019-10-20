#pragma once

#include <wchar.h>

typedef bool (*audience_inner_init_t)();
typedef void *(*audience_inner_window_create_t)(const wchar_t *const title, const wchar_t *const url);
typedef void (*audience_inner_window_destroy_t)(void *window);
typedef void (*audience_inner_loop_t)();
