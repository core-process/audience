#pragma once

#include <stdint.h>
#include <wchar.h>
#include <stdbool.h>

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

  typedef struct
  {
    struct
    {
      void (*handler)(AudienceWindowHandle handle, void *context, bool *prevent_close);
      void *context;
    } on_will_close;
    struct
    {
      void (*handler)(AudienceWindowHandle handle, void *context, bool *prevent_quit);
      void *context;
    } on_close;
  } AudienceWindowEventHandler;

  typedef struct
  {
    struct
    {
      void (*handler)(void *context, bool *prevent_quit);
      void *context;
    } on_will_quit;
    struct
    {
      void (*handler)(void *context);
      void *context;
    } on_quit;
  } AudienceEventHandler;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
