#pragma once

#include <audience_details.h>

#pragma pack(push)
#pragma pack(1)

struct AudienceNucleusProtocolNegotiation
{
  bool nucleus_handles_webapp_type_directory;
  bool nucleus_handles_webapp_type_url;
  bool nucleus_handles_messaging;
  struct
  {
    struct
    {
      void (*on_message)(AudienceWindowHandle handle, const wchar_t *message);
      void (*on_close_intent)(AudienceWindowHandle handle);
      void (*on_close)(AudienceWindowHandle handle, bool is_last_window);
    } window_level;
    struct
    {
      void (*on_quit)();
    } app_level;
  } shell_event_handler;
};

typedef struct
{
  const wchar_t *icon_set[AUDIENCE_APP_DETAILS_ICON_SET_ENTRIES];
} AudienceNucleusAppDetails;

#pragma pack(pop)
