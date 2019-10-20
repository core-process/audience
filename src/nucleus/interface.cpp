#include <wchar.h>
#include <exception>

#include "../common/trace.h"
#include "interface.h"

#ifdef WIN32
#define AUDIENCE_INTERFACE_EXPORT __declspec(dllexport)
#else
#define AUDIENCE_INTERFACE_EXPORT
#endif

extern "C"
{
  AUDIENCE_INTERFACE_EXPORT bool audience_inner_init();
  AUDIENCE_INTERFACE_EXPORT void *audience_inner_window_create(const wchar_t *const title, const wchar_t *const url);
  AUDIENCE_INTERFACE_EXPORT void audience_inner_window_destroy(void *window);
  AUDIENCE_INTERFACE_EXPORT void audience_inner_loop();
}

#define AUDIENCE_INTERFACE_CATCH(return_value)            \
  catch (const std::exception &ex)                        \
  {                                                       \
    TRACEA(error, "an exception occured: " << ex.what()); \
    return return_value;                                  \
  }                                                       \
  catch (...)                                             \
  {                                                       \
    TRACEA(error, "an unknown exception occured");        \
    return return_value;                                  \
  }

#ifdef __OBJC__
#define AUDIENCE_INTERFACE_POOL_BEGIN \
  @autoreleasepool                    \
  {
#define AUDIENCE_INTERFACE_POOL_END }
#else
#define AUDIENCE_INTERFACE_POOL_BEGIN
#define AUDIENCE_INTERFACE_POOL_END
#endif

bool audience_inner_init()
{
  AUDIENCE_INTERFACE_POOL_BEGIN
  try
  {
    return _audience_inner_init();
  }
  AUDIENCE_INTERFACE_CATCH(false)
  AUDIENCE_INTERFACE_POOL_END
}

void *audience_inner_window_create(const wchar_t *const title, const wchar_t *const url)
{
  AUDIENCE_INTERFACE_POOL_BEGIN
  try
  {
    AudienceHandle *handle = _audience_inner_window_create(std::wstring(title), std::wstring(url));
#ifdef __OBJC__
    return (__bridge_retained void *)handle;
#else
    return reinterpret_cast<void *>(handle);
#endif
  }
  AUDIENCE_INTERFACE_CATCH(nullptr)
  AUDIENCE_INTERFACE_POOL_END
}

void audience_inner_window_destroy(void *vhandle)
{
  AUDIENCE_INTERFACE_POOL_BEGIN
  try
  {
#ifdef __OBJC__
    AudienceHandle *handle = (__bridge_transfer AudienceHandle *)vhandle;
#else
    AudienceHandle *handle = reinterpret_cast<AudienceHandle *>(vhandle);
#endif
    _audience_inner_window_destroy(handle);
  }
  AUDIENCE_INTERFACE_CATCH()
  AUDIENCE_INTERFACE_POOL_END
}

void audience_inner_loop()
{
  AUDIENCE_INTERFACE_POOL_BEGIN
  try
  {
    _audience_inner_loop();
  }
  AUDIENCE_INTERFACE_CATCH()
  AUDIENCE_INTERFACE_POOL_END
}
