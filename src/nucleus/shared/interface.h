#pragma once

#include <wchar.h>
#include <string>
#include <boost/bimap.hpp>

#include <audience_details.h>

#include "../../common/nucleus_interface_details.h"
#include "safefn.h"
#include "nucleus.h"

///////////////////////////////////////////////////////////////////////
// External Declaration
///////////////////////////////////////////////////////////////////////

#ifdef WIN32
#define AUDIENCE_EXT_EXPORT __declspec(dllexport)
#else
#define AUDIENCE_EXT_EXPORT
#endif

extern "C"
{
  AUDIENCE_EXT_EXPORT bool audience_init(AudienceNucleusProtocolNegotiation *negotiation);
  AUDIENCE_EXT_EXPORT AudienceWindowHandle audience_window_create(const AudienceWindowDetails *details);
  AUDIENCE_EXT_EXPORT void audience_window_destroy(AudienceWindowHandle handle);
  AUDIENCE_EXT_EXPORT void audience_main();
  AUDIENCE_EXT_EXPORT void audience_dispatch_sync(void (*task)(void *context), void *context);
}

///////////////////////////////////////////////////////////////////////
// Internal Declaration
///////////////////////////////////////////////////////////////////////

struct InternalWindowDetails
{
  AudienceWebAppType webapp_type;
  std::wstring webapp_location;
  std::wstring loading_title;
  bool dev_mode;
};

bool internal_init(AudienceNucleusProtocolNegotiation *negotiation);
AudienceWindowContext internal_window_create(const InternalWindowDetails &details);
void internal_window_destroy(AudienceWindowContext context);
void internal_main();
void internal_dispatch_sync(void (*task)(void *context), void *context);

///////////////////////////////////////////////////////////////////////
// Bridge Implementation
///////////////////////////////////////////////////////////////////////

typedef boost::bimap<AudienceWindowHandle, AudienceWindowContext> InternalWindowContextMap;
extern InternalWindowContextMap _internal_window_context_map;
extern AudienceWindowHandle _internal_window_context_next_handle;

extern AudienceNucleusProtocolNegotiation *_internal_protocol_negotiation;

static inline bool _internal_init(AudienceNucleusProtocolNegotiation *negotiation)
{
  auto status = internal_init(negotiation);
  if (status)
  {
    _internal_protocol_negotiation = negotiation;
  }
  return status;
}

static inline AudienceWindowHandle _internal_window_create(const AudienceWindowDetails *details)
{
  // create window
  InternalWindowDetails internal_details{
      details->webapp_type,
      std::wstring(details->webapp_location),
      std::wstring(details->loading_title != nullptr ? details->loading_title : L"Loading..."),
      details->dev_mode};

  auto context = internal_window_create(internal_details);

  if (!context)
  {
    return AudienceWindowHandle{};
  }

  // allocate handle (we use defined overflow behaviour from unsigned data type here)
  auto handle = _internal_window_context_next_handle++;
  while (_internal_window_context_map.left.find(handle) != _internal_window_context_map.left.end())
  {
    handle = _internal_window_context_next_handle++;
  }

  // add context to map
  _internal_window_context_map.insert(InternalWindowContextMap::value_type(handle, context));
  TRACEA(info, "window context and associated handle added to map");

  return handle;
}

static inline void _internal_window_destroy(AudienceWindowHandle handle)
{
  // find context
  auto icontext = _internal_window_context_map.left.find(handle);

  // destroy window
  if (icontext != _internal_window_context_map.left.end())
  {
    return internal_window_destroy(icontext->second);
  }
  else
  {
    TRACEA(warning, "window handle/context not found, window and its context already destroyed");
  }
}

///////////////////////////////////////////////////////////////////////
// Event Handling
///////////////////////////////////////////////////////////////////////

static inline void _internal_on_window_will_close(AudienceWindowContext context, bool &prevent_close)
{
  // lookup handle
  auto ihandle = _internal_window_context_map.right.find(context);
  if (ihandle == _internal_window_context_map.right.end())
  {
    TRACEA(warning, "window handle/context not found, window and its context already destroyed");
    return;
  }

  auto handle = ihandle->second;

  // call shell handler
  _internal_protocol_negotiation->shell_event_handler.window_level.on_will_close(handle, &prevent_close);
}

static inline void internal_on_window_will_close(AudienceWindowContext context, bool &prevent_close)
{
  NUCLEUS_SAFE_FN(_internal_on_window_will_close)
  (context, prevent_close);
}

static inline void _internal_on_window_close(AudienceWindowContext context, bool &prevent_quit)
{
  // lookup handle
  auto ihandle = _internal_window_context_map.right.find(context);
  if (ihandle == _internal_window_context_map.right.end())
  {
    TRACEA(warning, "window handle/context not found, window and its context already destroyed");
    return;
  }

  auto handle = ihandle->second;

  // call shell handler
  _internal_protocol_negotiation->shell_event_handler.window_level.on_close(handle, &prevent_quit);

  // remove window from context map
  _internal_window_context_map.right.erase(context);
  TRACEA(info, "window context and associated handle removed from map");
}

static inline void internal_on_window_close(AudienceWindowContext context, bool &prevent_quit)
{
  NUCLEUS_SAFE_FN(_internal_on_window_close)
  (context, prevent_quit);
}

static inline void _internal_on_process_will_quit(bool &prevent_quit)
{
  // call shell handler
  _internal_protocol_negotiation->shell_event_handler.process_level.on_will_quit(&prevent_quit);
}

static inline void internal_on_process_will_quit(bool &prevent_quit)
{
  NUCLEUS_SAFE_FN(_internal_on_process_will_quit)
  (prevent_quit);
}

static inline void _internal_on_process_quit()
{
  // call shell handler
  _internal_protocol_negotiation->shell_event_handler.process_level.on_quit();
}

static inline void internal_on_process_quit()
{
  NUCLEUS_SAFE_FN(_internal_on_process_quit)
  ();
}

///////////////////////////////////////////////////////////////////////
// External Implementation
///////////////////////////////////////////////////////////////////////

#ifdef __OBJC__
#define AUDIENCE_EXTIMPL_RELEASEPOOL @autoreleasepool
#else
#define AUDIENCE_EXTIMPL_RELEASEPOOL
#endif

#define AUDIENCE_EXTIMPL_INIT                                         \
  bool audience_init(AudienceNucleusProtocolNegotiation *negotiation) \
  {                                                                   \
    AUDIENCE_EXTIMPL_RELEASEPOOL                                      \
    {                                                                 \
      return NUCLEUS_SAFE_FN(_internal_init, false)(negotiation);     \
    }                                                                 \
  }

#define AUDIENCE_EXTIMPL_WINDOW_CREATE                                                  \
  AudienceWindowHandle audience_window_create(const AudienceWindowDetails *details)     \
  {                                                                                     \
    AUDIENCE_EXTIMPL_RELEASEPOOL                                                        \
    {                                                                                   \
      return NUCLEUS_SAFE_FN(_internal_window_create, AudienceWindowHandle{})(details); \
    }                                                                                   \
  }

#define AUDIENCE_EXTIMPL_WINDOW_DESTROY                     \
  void audience_window_destroy(AudienceWindowHandle handle) \
  {                                                         \
    AUDIENCE_EXTIMPL_RELEASEPOOL                            \
    {                                                       \
      NUCLEUS_SAFE_FN(_internal_window_destroy)             \
      (handle);                                             \
    }                                                       \
  }

#define AUDIENCE_EXTIMPL_MAIN        \
  void audience_main()               \
  {                                  \
    AUDIENCE_EXTIMPL_RELEASEPOOL     \
    {                                \
      NUCLEUS_SAFE_FN(internal_main) \
      ();                            \
    }                                \
  }

#define AUDIENCE_EXTIMPL_DISPATCH_SYNC                                    \
  void audience_dispatch_sync(void (*task)(void *context), void *context) \
  {                                                                       \
    AUDIENCE_EXTIMPL_RELEASEPOOL                                          \
    {                                                                     \
      NUCLEUS_SAFE_FN(internal_dispatch_sync)                             \
      (task, context);                                                    \
    }                                                                     \
  }

#define AUDIENCE_EXTIMPL                                                                    \
  boost::bimap<AudienceWindowHandle, AudienceWindowContext> _internal_window_context_map{}; \
  AudienceWindowHandle _internal_window_context_next_handle = AudienceWindowHandle{} + 1;   \
  AudienceNucleusProtocolNegotiation *_internal_protocol_negotiation = nullptr;             \
  AUDIENCE_EXTIMPL_INIT;                                                                    \
  AUDIENCE_EXTIMPL_WINDOW_CREATE;                                                           \
  AUDIENCE_EXTIMPL_WINDOW_DESTROY;                                                          \
  AUDIENCE_EXTIMPL_MAIN;                                                                    \
  AUDIENCE_EXTIMPL_DISPATCH_SYNC;
