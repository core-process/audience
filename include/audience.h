#pragma once

#include <stdbool.h>
#include <audience_details.h>

#ifdef __cplusplus
extern "C"
{
#endif

  bool audience_init(const AudienceEventHandler *event_handler);
  bool audience_is_initialized();
  AudienceWindowHandle audience_window_create(const AudienceWindowDetails *details, const AudienceWindowEventHandler *event_handler);
  void audience_window_destroy(AudienceWindowHandle handle);
  void audience_main();

#ifdef __cplusplus
}
#endif
