#pragma once

#include <stdbool.h>
#include <audience_details.h>

#ifdef __cplusplus
extern "C"
{
#endif

  AUDIENCE_API bool audience_init(const AudienceEventHandler *event_handler);
  AUDIENCE_API AudienceWindowHandle audience_window_create(const AudienceWindowDetails *details, const AudienceWindowEventHandler *event_handler);
  AUDIENCE_API void audience_window_post_message(AudienceWindowHandle handle, const char *message);
  AUDIENCE_API void audience_window_destroy(AudienceWindowHandle handle);
  AUDIENCE_API void audience_main();

#ifdef __cplusplus
}
#endif
