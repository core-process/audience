#pragma once

#include <audience_details.h>

#pragma pack(push)
#pragma pack(1)

struct AudienceNucleusProtocolNegotiation
{
  bool nucleus_handles_webapp_type_directory;
  bool nucleus_handles_webapp_type_url;
  bool nucleus_handles_messaging;
};

#pragma pack(pop)
