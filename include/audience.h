#pragma once

#include <wchar.h>

#ifdef __cplusplus
extern "C"
{
#endif

  bool audience_init();
  bool audience_is_initialized();
  void *audience_window_create(const wchar_t *const title, const wchar_t *const url);
  void audience_window_destroy(void *handle);
  void audience_loop();

#ifdef __cplusplus
}
#endif
