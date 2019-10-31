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

#define AUDIENCE_SCREEN_LIST_ENTRIES 10
#define AUDIENCE_WINDOW_LIST_ENTRIES 20
#define AUDIENCE_APP_DETAILS_LOAD_ORDER_ENTRIES 10
#define AUDIENCE_APP_DETAILS_ICON_SET_ENTRIES 20

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

  typedef struct
  {
    float x;
    float y;
  } AudiencePoint;

  typedef struct
  {
    float width;
    float height;
  } AudienceSize;

  typedef struct
  {
    AudiencePoint origin;
    AudienceSize size;
  } AudienceRect;

  typedef uint16_t AudienceWindowHandle;

  typedef struct
  {
    struct
    {
      AudienceNucleusTechWindows windows[AUDIENCE_APP_DETAILS_LOAD_ORDER_ENTRIES];
      AudienceNucleusTechMacOS macos[AUDIENCE_APP_DETAILS_LOAD_ORDER_ENTRIES];
      AudienceNucleusTechUnix unix[AUDIENCE_APP_DETAILS_LOAD_ORDER_ENTRIES];
    } load_order;
    // icon set:
    // - use png files, as it is supported by all implementations
    // - provide sizes from 16x16 to 1024x1024 if required by your icon design, e.g. 16, 32, 64, 128, 256, 512, 1024
    // - it is not necessary to provide all sizes, every implementation is capable of scaling
    // - windows: picks smallest icon by width, larger or equal to SM_C?ICON/SM_C?SMICON metrics
    // - unix: sorts by width ascending and builds an icon list, which gets cut off at a certain position by the underlying system when hitting a limit
    // - macos: picks largest icon by width
    const wchar_t *icon_set[AUDIENCE_APP_DETAILS_ICON_SET_ENTRIES];
  } AudienceAppDetails;

  typedef struct
  {
    uint8_t focused; // list index
    uint8_t primary; // list index
    struct
    {
      AudienceRect frame;
      AudienceRect workspace;
    } screens[AUDIENCE_SCREEN_LIST_ENTRIES];
    uint8_t count;
  } AudienceScreenList;

  typedef struct
  {
    int8_t focused; // -1 = none/unknown/other, otherwise list index
    struct
    {
      AudienceWindowHandle handle;
      AudienceRect frame;
      AudienceSize workspace;
    } windows[AUDIENCE_WINDOW_LIST_ENTRIES];
    uint8_t count;
  } AudienceWindowList;

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
  } AudienceAppEventHandler;

  enum AudienceWebAppType
  {
    AUDIENCE_WEBAPP_TYPE_DIRECTORY = 0,
    AUDIENCE_WEBAPP_TYPE_URL = 1
  };

  typedef struct
  {
    AudienceWebAppType webapp_type;
    const wchar_t *webapp_location; // cannot be nullptr
    const wchar_t *loading_title;   // defaults to "Loading..."
    AudienceRect position;
    typedef struct
    {
      bool not_decorated;
      bool not_resizable;
    } styles;
    AudienceWindowHandle modal_parent; // becomes a modal of parent, if set
    bool dev_mode;
  } AudienceWindowDetails;

  typedef struct
  {
    struct
    {
      void (*handler)(AudienceWindowHandle handle, void *context, const wchar_t *message);
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
