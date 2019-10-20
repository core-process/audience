#pragma once

extern "C" bool audience_init();
extern "C" void *audience_window_create(const wchar_t *const title, const wchar_t *const url);
extern "C" void audience_window_destroy(void *window);
extern "C" void audience_loop();
