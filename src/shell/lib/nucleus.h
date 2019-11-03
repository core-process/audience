#pragma once

#include <audience_details.h>
#include "../../shared/nucleus_api_details.h"

typedef bool (*nucleus_init_t)(AudienceNucleusProtocolNegotiation *negotiation, const AudienceNucleusAppDetails *details);
typedef AudienceScreenList (*nucleus_screen_list_t)();
typedef AudienceWindowList (*nucleus_window_list_t)();
typedef AudienceWindowHandle (*nucleus_window_create_t)(const AudienceWindowDetails *details);
typedef void (*nucleus_window_update_position_t)(AudienceWindowHandle handle, AudienceRect position);
typedef void (*nucleus_window_post_message_t)(AudienceWindowHandle handle, const wchar_t *message);
typedef void (*nucleus_window_destroy_t)(AudienceWindowHandle handle);
typedef void (*nucleus_quit_t)();
typedef void (*nucleus_main_t)();
typedef void (*nucleus_dispatch_sync_t)(void (*task)(void *context), void *context);
typedef void (*nucleus_dispatch_async_t)(void (*task)(void *context), void *context);
