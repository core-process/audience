#pragma once

#include <string>
#include <audience_details.h>

extern void channel_create(const std::string& path);
extern void channel_activate();
extern void channel_shutdown();

extern void channel_emit_window_message(AudienceWindowHandle handle, const wchar_t *message);
extern void channel_emit_window_close_intent(AudienceWindowHandle handle);
extern void channel_emit_window_close(AudienceWindowHandle handle, bool is_last_window);
extern void channel_emit_app_quit();
