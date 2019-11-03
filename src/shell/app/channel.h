#pragma once

#include <string>
#include <audience_details.h>

extern void channel_prepare(const std::string& path);
extern void channel_activate();
extern void channel_shutdown();

extern void channel_emit_window_message(AudienceWindowHandle handle, const wchar_t *message);
extern void channel_emit_window_close_intent(AudienceWindowHandle handle);
