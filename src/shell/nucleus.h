#pragma once

#include <wchar.h>

typedef bool (*nucleus_init_t)();
typedef void *(*nucleus_window_create_t)(const wchar_t *const title, const wchar_t *const url);
typedef void (*nucleus_window_destroy_t)(void *window);
typedef void (*nucleus_loop_t)();
