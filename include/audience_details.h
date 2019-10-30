#pragma once

#include <stdint.h>
#include <wchar.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#pragma pack(push)
#pragma pack(1)

  enum AudienceNucleusTechWindows
  {
    AUDIENCE_NUCLEUS_WINDOWS_NONE = 0,
    AUDIENCE_NUCLEUS_WINDOWS_EDGE = 1,
    AUDIENCE_NUCLEUS_WINDOWS_IE11 = 2
  };

  enum AudienceNucleusTechMacOS
  {
    AUDIENCE_NUCLEUS_MACOS_NONE = 0,
    AUDIENCE_NUCLEUS_MACOS_WEBKIT = 1
  };

  enum AudienceNucleusTechUnix
  {
    AUDIENCE_NUCLEUS_UNIX_NONE = 0,
    AUDIENCE_NUCLEUS_UNIX_WEBKIT = 1
  };

#define AUDIENCE_DETAILS_LOAD_ORDER_ENTRIES 10
#define AUDIENCE_DETAILS_ICON_SET_ENTRIES 20

  typedef struct
  {
    struct
    {
      AudienceNucleusTechWindows windows[AUDIENCE_DETAILS_LOAD_ORDER_ENTRIES];
      AudienceNucleusTechMacOS macos[AUDIENCE_DETAILS_LOAD_ORDER_ENTRIES];
      AudienceNucleusTechUnix unix[AUDIENCE_DETAILS_LOAD_ORDER_ENTRIES];
    } load_order;
    // icon set:
    // - use png files, as it is supported by all implementations
    // - provide sizes from 16x16 to 1024x1024 if required by your icon design, e.g. 16, 32, 64, 128, 256, 512, 1024
    // - it is not necessary to provide all sizes, every implementation is capable of scaling
    // - windows: picks smallest icon by width, larger or equal to SM_C?ICON/SM_C?SMICON metrics
    // - unix: sorts by width ascending and builds an icon list, which gets cut off at a certain position by the underlying system when hitting a limit
    // - macos: picks largest icon by width
    const wchar_t *icon_set[AUDIENCE_DETAILS_ICON_SET_ENTRIES];
  } AudienceDetails;

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

  enum AudienceWebAppType
  {
    AUDIENCE_WEBAPP_TYPE_DIRECTORY = 0,
    AUDIENCE_WEBAPP_TYPE_URL = 1
  };

  typedef uint16_t AudienceWindowHandle;

  typedef struct
  {
    AudienceWebAppType webapp_type;
    const wchar_t *webapp_location; // cannot be nullptr
    const wchar_t *loading_title;   // defaults to "Loading..."
    bool dev_mode;
  } AudienceWindowDetails;

  typedef struct
  {
    struct
    {
      void (*handler)(AudienceWindowHandle handle, void *context, const char *message);
      void *context;
    } on_message;
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

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
