#pragma once

#include <wchar.h>
#include <string>
#include <boost/bimap.hpp>

#include <audience_details.h>

#include "../../shared/nucleus_api_details.h"
#include "safefn.h"
#include "nucleus.h"

///////////////////////////////////////////////////////////////////////
// External Declaration
///////////////////////////////////////////////////////////////////////

#ifdef WIN32
#define NUCLEUS_EXPORT __declspec(dllexport)
#else
#define NUCLEUS_EXPORT __attribute__((visibility("default")))
#endif

extern "C"
{
  NUCLEUS_EXPORT bool nucleus_init(AudienceNucleusProtocolNegotiation *negotiation, const AudienceNucleusAppDetails *details);
  NUCLEUS_EXPORT AudienceScreenList nucleus_screen_list();
  NUCLEUS_EXPORT AudienceWindowList nucleus_window_list();
  NUCLEUS_EXPORT AudienceWindowHandle nucleus_window_create(const AudienceWindowDetails *details);
  NUCLEUS_EXPORT void nucleus_window_update_position(AudienceWindowHandle handle, AudienceRect position);
  NUCLEUS_EXPORT void nucleus_window_post_message(AudienceWindowHandle handle, const wchar_t *message);
  NUCLEUS_EXPORT void nucleus_window_destroy(AudienceWindowHandle handle);
  NUCLEUS_EXPORT void nucleus_main();
  NUCLEUS_EXPORT void nucleus_dispatch_sync(void (*task)(void *context), void *context);
  NUCLEUS_EXPORT void nucleus_dispatch_async(void (*task)(void *context), void *context);
}

///////////////////////////////////////////////////////////////////////
// Internal Declaration
///////////////////////////////////////////////////////////////////////

#pragma pack(push)
#pragma pack(1)

struct NucleusImplAppDetails
{
  std::vector<std::wstring> icon_set;
};

struct NucleusImplWindowDetails
{
  AudienceWebAppType webapp_type;
  std::wstring webapp_location;
  std::wstring loading_title;
  AudienceRect position;
  AudienceWindowStyles styles;
  // TO BE IMPLEMENTED:
  // AudienceWindowContext modal_context;
  bool dev_mode;
};

struct NucleusImplWindowStatus
{
  bool has_focus;
  AudienceRect frame;
  AudienceSize workspace;
};

#pragma pack(pop)

bool nucleus_impl_init(AudienceNucleusProtocolNegotiation &negotiation, const NucleusImplAppDetails &details);
AudienceScreenList nucleus_impl_screen_list();
AudienceWindowContext nucleus_impl_window_create(const NucleusImplWindowDetails &details);
NucleusImplWindowStatus nucleus_impl_window_status(AudienceWindowContext context);
void nucleus_impl_window_update_position(AudienceWindowContext context, AudienceRect position);
void nucleus_impl_window_post_message(AudienceWindowContext context, const std::wstring &message);
void nucleus_impl_window_destroy(AudienceWindowContext context);
void nucleus_impl_main();
void nucleus_impl_dispatch_sync(void (*task)(void *context), void *context);
void nucleus_impl_dispatch_async(void (*task)(void *context), void *context);

///////////////////////////////////////////////////////////////////////
// Bridge Implementation
///////////////////////////////////////////////////////////////////////

typedef boost::bimap<AudienceWindowHandle, AudienceWindowContext> NucleusWindowContextMap;
extern NucleusWindowContextMap nucleus_window_context_map;
extern AudienceWindowHandle nucleus_window_context_next_handle;

extern AudienceNucleusProtocolNegotiation *nucleus_protocol_negotiation;

static inline bool bridge_init(AudienceNucleusProtocolNegotiation *negotiation, const AudienceNucleusAppDetails *details)
{
  // translate details
  NucleusImplAppDetails impl_details{};

  impl_details.icon_set.reserve(AUDIENCE_APP_DETAILS_ICON_SET_ENTRIES);
  for (size_t i = 0; i < AUDIENCE_APP_DETAILS_ICON_SET_ENTRIES; ++i)
  {
    if (details->icon_set[i] != nullptr)
    {
      impl_details.icon_set.push_back(details->icon_set[i]);
    }
  }

  // init
  auto status = nucleus_impl_init(*negotiation, impl_details);
  if (status)
  {
    nucleus_protocol_negotiation = negotiation;
  }
  return status;
}

static inline AudienceWindowList bridge_window_list()
{
  AudienceWindowList result{};

  result.focused = -1;

  for (auto &entry : nucleus_window_context_map.left)
  {
    if (result.count >= AUDIENCE_WINDOW_LIST_ENTRIES)
    {
      break;
    }

    auto status = nucleus_impl_window_status(entry.second);

    if (status.has_focus)
    {
      result.focused = result.count;
    }

    result.windows[result.count].handle = entry.first;
    result.windows[result.count].frame = status.frame;
    result.windows[result.count].workspace = status.workspace;

    result.count += 1;
  }

  return result;
}

static inline AudienceWindowHandle bridge_window_create(const AudienceWindowDetails *details)
{
  // find modal parent context
  // TO BE IMPLEMENTED:
  // auto i_modal_parent_context =
  //     details->modal_parent != AudienceWindowHandle{}
  //         ? nucleus_window_context_map.left.find(details->modal_parent)
  //         : nucleus_window_context_map.left.end();

  // translate details
  NucleusImplWindowDetails impl_details{
      details->webapp_type,
      std::wstring(details->webapp_location),
      std::wstring(details->loading_title != nullptr ? details->loading_title : L"Loading..."),
      details->position,
      details->styles,
      // TO BE IMPLEMENTED:
      // i_modal_parent_context != nucleus_window_context_map.left.end()
      //     ? i_modal_parent_context->second
      //     : AudienceWindowContext{},
      details->dev_mode};

  // create window
  auto context = nucleus_impl_window_create(impl_details);

  if (!context)
  {
    return AudienceWindowHandle{};
  }

  // allocate handle (we use defined overflow behaviour from unsigned data type here)
  auto handle = nucleus_window_context_next_handle++;
  while (nucleus_window_context_map.left.find(handle) != nucleus_window_context_map.left.end() || handle == AudienceWindowHandle{})
  {
    handle = nucleus_window_context_next_handle++;
  }

  // add context to map
  nucleus_window_context_map.insert(NucleusWindowContextMap::value_type(handle, context));
  TRACEA(info, "window context and associated handle added to map");

  return handle;
}

static inline void bridge_window_update_position(AudienceWindowHandle handle, AudienceRect position)
{
  // find context
  auto icontext = nucleus_window_context_map.left.find(handle);

  // update window position
  if (icontext != nucleus_window_context_map.left.end())
  {
    return nucleus_impl_window_update_position(icontext->second, position);
  }
  else
  {
    TRACEA(warning, "window handle/context not found, window and its context already destroyed");
    return;
  }
}

static inline void bridge_window_post_message(AudienceWindowHandle handle, const wchar_t *message)
{
  // find context
  auto icontext = nucleus_window_context_map.left.find(handle);

  // post message
  if (icontext != nucleus_window_context_map.left.end())
  {
    return nucleus_impl_window_post_message(icontext->second, std::wstring(message));
  }
  else
  {
    TRACEA(warning, "window handle/context not found, window and its context already destroyed");
    return;
  }
}

static inline void bridge_window_destroy(AudienceWindowHandle handle)
{
  // find context
  auto icontext = nucleus_window_context_map.left.find(handle);

  // destroy window
  if (icontext != nucleus_window_context_map.left.end())
  {
    return nucleus_impl_window_destroy(icontext->second);
  }
  else
  {
    TRACEA(warning, "window handle/context not found, window and its context already destroyed");
    return;
  }
}

///////////////////////////////////////////////////////////////////////
// Event Handling
///////////////////////////////////////////////////////////////////////

static inline void emit_unsafe_window_message(AudienceWindowContext context, const std::wstring &message)
{
  // lookup handle
  auto ihandle = nucleus_window_context_map.right.find(context);
  if (ihandle == nucleus_window_context_map.right.end())
  {
    TRACEA(warning, "window handle/context not found, window and its context already destroyed");
    return;
  }

  auto handle = ihandle->second;

  // call shell handler
  nucleus_protocol_negotiation->shell_event_handler.window_level.on_message(handle, message.c_str());
}

static inline void emit_window_message(AudienceWindowContext context, const std::wstring &message)
{
  NUCLEUS_SAFE_FN(emit_unsafe_window_message)
  (context, message);
}

static inline void emit_unsafe_window_will_close(AudienceWindowContext context, bool &prevent_close)
{
  // lookup handle
  auto ihandle = nucleus_window_context_map.right.find(context);
  if (ihandle == nucleus_window_context_map.right.end())
  {
    TRACEA(warning, "window handle/context not found, window and its context already destroyed");
    return;
  }

  auto handle = ihandle->second;

  // call shell handler
  nucleus_protocol_negotiation->shell_event_handler.window_level.on_will_close(handle, &prevent_close);
}

static inline void emit_window_will_close(AudienceWindowContext context, bool &prevent_close)
{
  NUCLEUS_SAFE_FN(emit_unsafe_window_will_close)
  (context, prevent_close);
}

static inline void emit_unsafe_window_close(AudienceWindowContext context, bool &prevent_quit)
{
  // lookup handle
  auto ihandle = nucleus_window_context_map.right.find(context);
  if (ihandle == nucleus_window_context_map.right.end())
  {
    TRACEA(warning, "window handle/context not found, window and its context already destroyed");
    return;
  }

  auto handle = ihandle->second;

  // call shell handler
  nucleus_protocol_negotiation->shell_event_handler.window_level.on_close(handle, &prevent_quit);

  // remove window from context map
  nucleus_window_context_map.right.erase(context);
  TRACEA(info, "window context and associated handle removed from map");
}

static inline void emit_window_close(AudienceWindowContext context, bool &prevent_quit)
{
  NUCLEUS_SAFE_FN(emit_unsafe_window_close)
  (context, prevent_quit);
}

static inline void emit_unsafe_app_will_quit(bool &prevent_quit)
{
  // call shell handler
  nucleus_protocol_negotiation->shell_event_handler.app_level.on_will_quit(&prevent_quit);
}

static inline void emit_app_will_quit(bool &prevent_quit)
{
  NUCLEUS_SAFE_FN(emit_unsafe_app_will_quit)
  (prevent_quit);
}

static inline void emit_unsafe_app_quit()
{
  // call shell handler
  nucleus_protocol_negotiation->shell_event_handler.app_level.on_quit();
}

static inline void emit_app_quit()
{
  NUCLEUS_SAFE_FN(emit_unsafe_app_quit)
  ();
}

///////////////////////////////////////////////////////////////////////
// External Implementation
///////////////////////////////////////////////////////////////////////

#ifdef __OBJC__
#define NUCLEUS_RELEASEPOOL @autoreleasepool
#else
#define NUCLEUS_RELEASEPOOL
#endif

#define NUCLEUS_PUBIMPL_INIT                                                                                   \
  bool nucleus_init(AudienceNucleusProtocolNegotiation *negotiation, const AudienceNucleusAppDetails *details) \
  {                                                                                                            \
    NUCLEUS_RELEASEPOOL                                                                                        \
    {                                                                                                          \
      return NUCLEUS_SAFE_FN(bridge_init, SAFE_FN_DEFAULT(bool))(negotiation, details);                        \
    }                                                                                                          \
  }

#define NUCLEUS_PUBIMPL_SCREEN_LIST                                                            \
  AudienceScreenList nucleus_screen_list()                                                     \
  {                                                                                            \
    NUCLEUS_RELEASEPOOL                                                                        \
    {                                                                                          \
      return NUCLEUS_SAFE_FN(nucleus_impl_screen_list, SAFE_FN_DEFAULT(AudienceScreenList))(); \
    }                                                                                          \
  }

#define NUCLEUS_PUBIMPL_WINDOW_LIST                                                      \
  AudienceWindowList nucleus_window_list()                                               \
  {                                                                                      \
    NUCLEUS_RELEASEPOOL                                                                  \
    {                                                                                    \
      return NUCLEUS_SAFE_FN(bridge_window_list, SAFE_FN_DEFAULT(AudienceWindowList))(); \
    }                                                                                    \
  }

#define NUCLEUS_PUBIMPL_WINDOW_CREATE                                                               \
  AudienceWindowHandle nucleus_window_create(const AudienceWindowDetails *details)                  \
  {                                                                                                 \
    NUCLEUS_RELEASEPOOL                                                                             \
    {                                                                                               \
      return NUCLEUS_SAFE_FN(bridge_window_create, SAFE_FN_DEFAULT(AudienceWindowHandle))(details); \
    }                                                                                               \
  }

#define NUCLEUS_PUBIMPL_WINDOW_UPDATE_POSITION                                            \
  void nucleus_window_update_position(AudienceWindowHandle handle, AudienceRect position) \
  {                                                                                       \
    NUCLEUS_RELEASEPOOL                                                                   \
    {                                                                                     \
      NUCLEUS_SAFE_FN(bridge_window_update_position)                                      \
      (handle, position);                                                                 \
    }                                                                                     \
  }

#define NUCLEUS_PUBIMPL_WINDOW_POST_MESSAGE                                             \
  void nucleus_window_post_message(AudienceWindowHandle handle, const wchar_t *message) \
  {                                                                                     \
    NUCLEUS_RELEASEPOOL                                                                 \
    {                                                                                   \
      NUCLEUS_SAFE_FN(bridge_window_post_message)                                       \
      (handle, message);                                                                \
    }                                                                                   \
  }

#define NUCLEUS_PUBIMPL_WINDOW_DESTROY                     \
  void nucleus_window_destroy(AudienceWindowHandle handle) \
  {                                                        \
    NUCLEUS_RELEASEPOOL                                    \
    {                                                      \
      NUCLEUS_SAFE_FN(bridge_window_destroy)               \
      (handle);                                            \
    }                                                      \
  }

#define NUCLEUS_PUBIMPL_MAIN             \
  void nucleus_main()                    \
  {                                      \
    NUCLEUS_RELEASEPOOL                  \
    {                                    \
      NUCLEUS_SAFE_FN(nucleus_impl_main) \
      ();                                \
    }                                    \
  }

#define NUCLEUS_PUBIMPL_DISPATCH_SYNC                                    \
  void nucleus_dispatch_sync(void (*task)(void *context), void *context) \
  {                                                                      \
    NUCLEUS_RELEASEPOOL                                                  \
    {                                                                    \
      NUCLEUS_SAFE_FN(nucleus_impl_dispatch_sync)                        \
      (task, context);                                                   \
    }                                                                    \
  }

#define NUCLEUS_PUBIMPL_DISPATCH_ASYNC                                    \
  void nucleus_dispatch_async(void (*task)(void *context), void *context) \
  {                                                                       \
    NUCLEUS_RELEASEPOOL                                                   \
    {                                                                     \
      NUCLEUS_SAFE_FN(nucleus_impl_dispatch_async)                        \
      (task, context);                                                    \
    }                                                                     \
  }

#define NUCLEUS_PUBIMPL                                                                   \
  boost::bimap<AudienceWindowHandle, AudienceWindowContext> nucleus_window_context_map{}; \
  AudienceWindowHandle nucleus_window_context_next_handle = AudienceWindowHandle{};       \
  AudienceNucleusProtocolNegotiation *nucleus_protocol_negotiation = nullptr;             \
  NUCLEUS_PUBIMPL_INIT;                                                                   \
  NUCLEUS_PUBIMPL_SCREEN_LIST;                                                            \
  NUCLEUS_PUBIMPL_WINDOW_LIST;                                                            \
  NUCLEUS_PUBIMPL_WINDOW_CREATE;                                                          \
  NUCLEUS_PUBIMPL_WINDOW_UPDATE_POSITION                                                  \
  NUCLEUS_PUBIMPL_WINDOW_POST_MESSAGE;                                                    \
  NUCLEUS_PUBIMPL_WINDOW_DESTROY;                                                         \
  NUCLEUS_PUBIMPL_MAIN;                                                                   \
  NUCLEUS_PUBIMPL_DISPATCH_SYNC;                                                          \
  NUCLEUS_PUBIMPL_DISPATCH_ASYNC;
