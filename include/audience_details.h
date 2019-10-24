#pragma once

#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C"
{
#endif

  enum AudienceWebAppType
  {
    AUDIENCE_WEBAPP_TYPE_DIRECTORY = 0,
    AUDIENCE_WEBAPP_TYPE_URL = 1
  };

  typedef uint16_t AudienceWindowHandle;

#pragma pack(push)
#pragma pack(1)

  typedef struct
  {
    AudienceWebAppType webapp_type;
    const wchar_t *webapp_location; // cannot be nullptr
    const wchar_t *loading_title;   // defaults to "Loading..."
  } AudienceWindowDetails;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
