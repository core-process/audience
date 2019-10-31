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
  NUCLEUS_EXPORT AudienceWindowHandle nucleus_window_create(const AudienceWindowDetails *details);
  NUCLEUS_EXPORT void nucleus_window_post_message(AudienceWindowHandle handle, const wchar_t *message);
  NUCLEUS_EXPORT void nucleus_window_destroy(AudienceWindowHandle handle);
  NUCLEUS_EXPORT void nucleus_main();
  NUCLEUS_EXPORT void nucleus_dispatch_sync(void (*task)(void *context), void *context);
  NUCLEUS_EXPORT void nucleus_dispatch_async(void (*task)(void *context), void *context);
}

///////////////////////////////////////////////////////////////////////
// Internal Declaration
///////////////////////////////////////////////////////////////////////

struct NucleusImplAppDetails
{
  std::vector<std::wstring> icon_set;
};

struct NucleusImplWindowDetails
{
  AudienceWebAppType webapp_type;
  std::wstring webapp_location;
  std::wstring loading_title;
  bool dev_mode;
};

bool nucleus_impl_init(AudienceNucleusProtocolNegotiation &negotiation, const NucleusImplAppDetails &details);
AudienceWindowContext nucleus_impl_window_create(const NucleusImplWindowDetails &details);
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

static inline AudienceWindowHandle bridge_window_create(const AudienceWindowDetails *details)
{
  // translate details
  NucleusImplWindowDetails impl_details{
      details->webapp_type,
      std::wstring(details->webapp_location),
      std::wstring(details->loading_title != nullptr ? details->loading_title : L"Loading..."),
      details->dev_mode};

  // create window
  auto context = nucleus_impl_window_create(impl_details);

  if (!context)
  {
    return AudienceWindowHandle{};
  }

  // allocate handle (we use defined overflow behaviour from unsigned data type here)
  auto handle = nucleus_window_context_next_handle++;
  while (nucleus_window_context_map.left.find(handle) != nucleus_window_context_map.left.end())
  {
    handle = nucleus_window_context_next_handle++;
  }

  // add context to map
  nucleus_window_context_map.insert(NucleusWindowContextMap::value_type(handle, context));
  TRACEA(info, "window context and associated handle added to map");

  return handle;
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
      return NUCLEUS_SAFE_FN(bridge_init, false)(negotiation, details);                                        \
    }                                                                                                          \
  }

#define NUCLEUS_PUBIMPL_WINDOW_CREATE                                                \
  AudienceWindowHandle nucleus_window_create(const AudienceWindowDetails *details)   \
  {                                                                                  \
    NUCLEUS_RELEASEPOOL                                                              \
    {                                                                                \
      return NUCLEUS_SAFE_FN(bridge_window_create, AudienceWindowHandle{})(details); \
    }                                                                                \
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
  AudienceWindowHandle nucleus_window_context_next_handle = AudienceWindowHandle{} + 1;   \
  AudienceNucleusProtocolNegotiation *nucleus_protocol_negotiation = nullptr;             \
  NUCLEUS_PUBIMPL_INIT;                                                                   \
  NUCLEUS_PUBIMPL_WINDOW_CREATE;                                                          \
  NUCLEUS_PUBIMPL_WINDOW_POST_MESSAGE;                                                    \
  NUCLEUS_PUBIMPL_WINDOW_DESTROY;                                                         \
  NUCLEUS_PUBIMPL_MAIN;                                                                   \
  NUCLEUS_PUBIMPL_DISPATCH_SYNC;                                                          \
  NUCLEUS_PUBIMPL_DISPATCH_ASYNC;
