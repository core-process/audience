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
      void (*on_message)(AudienceWindowHandle handle, const char *message);
      void (*on_will_close)(AudienceWindowHandle handle, bool *prevent_close);
      void (*on_close)(AudienceWindowHandle handle, bool *prevent_quit);
    } window_level;
    struct
    {
      void (*on_will_quit)(bool *prevent_quit);
      void (*on_quit)();
    } process_level;
  } shell_event_handler;
};

#pragma pack(pop)