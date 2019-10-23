#pragma once

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

  struct AudienceWindowDetails
  {
    AudienceWebAppType webapp_type;
    const wchar_t *webapp_location; // cannot be nullptr
    const wchar_t *loading_title;   // defaults to "Loading..."
  };

#ifdef __cplusplus
}
#endif
