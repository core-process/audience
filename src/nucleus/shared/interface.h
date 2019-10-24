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
  AUDIENCE_EXT_EXPORT void audience_loop();
}

///////////////////////////////////////////////////////////////////////
// Internal Declaration
///////////////////////////////////////////////////////////////////////

struct InternalWindowDetails
{
  AudienceWebAppType webapp_type;
  std::wstring webapp_location;
  std::wstring loading_title;
};

bool internal_init(AudienceNucleusProtocolNegotiation *negotiation);
AudienceWindowContext internal_window_create(const InternalWindowDetails &details);
void internal_window_destroy(AudienceWindowContext context);
void internal_loop();

///////////////////////////////////////////////////////////////////////
// Bridge Implementation
///////////////////////////////////////////////////////////////////////

typedef boost::bimap<AudienceWindowHandle, AudienceWindowContext> InternalWindowContextMap;
extern thread_local InternalWindowContextMap _internal_window_context_map;
extern thread_local AudienceWindowHandle _internal_window_context_next_handle;

static inline AudienceWindowHandle _internal_window_create(const AudienceWindowDetails *details)
{
  // create window
  InternalWindowDetails internal_details{
      details->webapp_type,
      std::wstring(details->webapp_location),
      std::wstring(details->loading_title != nullptr ? details->loading_title : L"Loading...")};

  auto context = internal_window_create(internal_details);

  if (!context)
  {
    return AudienceWindowHandle{};
  }

  // allocate handle (we use defined overflow behaviour from unsinged data type here)
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
    TRACEA(warning, "window handle not found, window and its context already destroyed");
  }
}

///////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////

static inline void _internal_on_window_destroyed(AudienceWindowContext context)
{
  _internal_window_context_map.right.erase(context);
  TRACEA(info, "window context and associated handle removed from map");
}

static inline void internal_on_window_destroyed(AudienceWindowContext context)
{
  NUCLEUS_SAFE_FN(_internal_on_window_destroyed)
  (context);
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
      return NUCLEUS_SAFE_FN(internal_init, false)(negotiation);      \
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

#define AUDIENCE_EXTIMPL_LOOP        \
  void audience_loop()               \
  {                                  \
    AUDIENCE_EXTIMPL_RELEASEPOOL     \
    {                                \
      NUCLEUS_SAFE_FN(internal_loop) \
      ();                            \
    }                                \
  }

#define AUDIENCE_EXTIMPL                                                                               \
  thread_local boost::bimap<AudienceWindowHandle, AudienceWindowContext> _internal_window_context_map; \
  thread_local AudienceWindowHandle _internal_window_context_next_handle = AudienceWindowHandle{} + 1; \
  AUDIENCE_EXTIMPL_INIT;                                                                               \
  AUDIENCE_EXTIMPL_WINDOW_CREATE;                                                                      \
  AUDIENCE_EXTIMPL_WINDOW_DESTROY;                                                                     \
  AUDIENCE_EXTIMPL_LOOP;
