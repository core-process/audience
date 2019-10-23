#pragma once

#include <audience.h>
#include "../common/nucleus_interface_details.h"

typedef bool (*nucleus_init_t)(AudienceNucleusProtocolNegotiation *negotiation);
typedef void *(*nucleus_window_create_t)(const AudienceWindowDetails *details);
typedef void (*nucleus_window_destroy_t)(void *window);
typedef void (*nucleus_loop_t)();
