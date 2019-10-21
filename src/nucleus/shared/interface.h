#pragma once

#include <wchar.h>
#include <string>

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
  AUDIENCE_EXT_EXPORT bool audience_init();
  AUDIENCE_EXT_EXPORT void *audience_window_create(const wchar_t *const title, const wchar_t *const url);
  AUDIENCE_EXT_EXPORT void audience_window_destroy(void *handle);
  AUDIENCE_EXT_EXPORT void audience_loop();
}

///////////////////////////////////////////////////////////////////////
// Internal Declaration
///////////////////////////////////////////////////////////////////////

bool internal_init();
AudienceHandle *internal_window_create(const std::wstring &title, const std::wstring &url);
void internal_window_destroy(AudienceHandle *handle);
void internal_loop();

///////////////////////////////////////////////////////////////////////
// Bridge Implementation
///////////////////////////////////////////////////////////////////////

static inline void *_internal_window_create(const wchar_t *const title, const wchar_t *const url)
{
  AudienceHandle *handle = internal_window_create(std::wstring(title), std::wstring(url));
#ifdef __OBJC__
  return (__bridge_retained void *)handle;
#else
  return reinterpret_cast<void *>(handle);
#endif
}

static inline void _internal_window_destroy(void *handle)
{
#ifdef __OBJC__
  AudienceHandle *typed_handle = (__bridge_transfer AudienceHandle *)handle;
#else
  AudienceHandle *typed_handle = reinterpret_cast<AudienceHandle *>(handle);
#endif
  internal_window_destroy(typed_handle);
}

///////////////////////////////////////////////////////////////////////
// External Implementation
///////////////////////////////////////////////////////////////////////

#ifdef __OBJC__
#define AUDIENCE_EXTIMPL_RELEASEPOOL @autoreleasepool
#else
#define AUDIENCE_EXTIMPL_RELEASEPOOL
#endif

#define AUDIENCE_EXTIMPL_INIT                         \
  bool audience_init()                                \
  {                                                   \
    AUDIENCE_EXTIMPL_RELEASEPOOL                      \
    {                                                 \
      return NUCLEUS_SAFE_FN(internal_init, false)(); \
    }                                                 \
  }

#define AUDIENCE_EXTIMPL_WINDOW_CREATE                                               \
  void *audience_window_create(const wchar_t *const title, const wchar_t *const url) \
  {                                                                                  \
    AUDIENCE_EXTIMPL_RELEASEPOOL                                                     \
    {                                                                                \
      return NUCLEUS_SAFE_FN(_internal_window_create, nullptr)(title, url);          \
    }                                                                                \
  }

#define AUDIENCE_EXTIMPL_WINDOW_DESTROY         \
  void audience_window_destroy(void *handle)    \
  {                                             \
    AUDIENCE_EXTIMPL_RELEASEPOOL                \
    {                                           \
      NUCLEUS_SAFE_FN(_internal_window_destroy) \
      (handle);                                 \
    }                                           \
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

#define AUDIENCE_EXTIMPL          \
  AUDIENCE_EXTIMPL_INIT           \
  AUDIENCE_EXTIMPL_WINDOW_CREATE  \
  AUDIENCE_EXTIMPL_WINDOW_DESTROY \
  AUDIENCE_EXTIMPL_LOOP
