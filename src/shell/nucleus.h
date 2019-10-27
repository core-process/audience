#pragma once

#include <audience_details.h>
#include "../common/nucleus_interface_details.h"

typedef bool (*nucleus_init_t)(AudienceNucleusProtocolNegotiation *negotiation);
typedef AudienceWindowHandle (*nucleus_window_create_t)(const AudienceWindowDetails *details);
typedef void (*nucleus_window_post_message_t)(AudienceWindowHandle handle, const char *message);
typedef void (*nucleus_window_destroy_t)(AudienceWindowHandle handle);
typedef void (*nucleus_main_t)();
typedef void (*nucleus_dispatch_sync_t)(void (*task)(void *context), void *context);
typedef void (*nucleus_dispatch_async_t)(void (*task)(void *context), void *context);
