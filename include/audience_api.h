#pragma once

#include <stdbool.h>
#include <audience_details.h>

#ifdef __cplusplus
extern "C"
{
#endif

  AUDIENCE_API bool audience_init(const AudienceAppDetails *details, const AudienceAppEventHandler *event_handler);
  AUDIENCE_API AudienceScreenList audience_screen_list();
  AUDIENCE_API AudienceWindowList audience_window_list();
  AUDIENCE_API AudienceWindowHandle audience_window_create(const AudienceWindowDetails *details, const AudienceWindowEventHandler *event_handler);
  AUDIENCE_API void audience_window_update_position(AudienceWindowHandle handle, AudienceRect position);
  AUDIENCE_API void audience_window_post_message(AudienceWindowHandle handle, const wchar_t *message);
  AUDIENCE_API void audience_window_destroy(AudienceWindowHandle handle);
  AUDIENCE_API void audience_quit();
  AUDIENCE_API void audience_main();

#ifdef __cplusplus
}
#endif
