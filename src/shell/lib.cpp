#ifdef WIN32
#include <windows.h>
#elif __APPLE__
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <sys/syslimits.h>
#include <stdlib.h>
#else
#include <dlfcn.h>
#include <linux/limits.h>
#include <cstring>
#endif

#include <vector>
#include <string>
#include <codecvt>
#include <locale>
#include <map>
#include <thread>
#include <mutex>

#include <audience.h>

#include "../common/trace.h"
#include "../common/safefn.h"
#include "webserver/process.h"
#include "nucleus.h"
#include "util.h"

std::mutex nucleus_thread_lock_mutex;
std::thread::id nucleus_thread_lock_id;

#define SHELL_CHECK_THREAD_LOCK                                                                                            \
  {                                                                                                                        \
    std::lock_guard<std::mutex> lock(nucleus_thread_lock_mutex);                                                           \
    if (nucleus_thread_lock_id == std::thread::id())                                                                       \
    {                                                                                                                      \
      throw std::runtime_error("audience needs to be locked to a specific thread (call audience_init() first)");           \
    }                                                                                                                      \
    if (nucleus_thread_lock_id != std::this_thread::get_id())                                                              \
    {                                                                                                                      \
      throw std::runtime_error("audience cannot be called from multiple threads (stick to you applications main thread)"); \
    }                                                                                                                      \
  }

thread_local nucleus_init_t nucleus_init = nullptr;
thread_local nucleus_window_create_t nucleus_window_create = nullptr;
thread_local nucleus_window_destroy_t nucleus_window_destroy = nullptr;
thread_local nucleus_loop_t nucleus_loop = nullptr;

thread_local AudienceNucleusProtocolNegotiation nucleus_protocol_negotiation{};
thread_local std::map<AudienceWindowHandle, std::shared_ptr<WebserverHandle>> nucleus_webserver_registry{};

bool audience_is_initialized()
{
  return nucleus_init != nullptr && nucleus_window_create != nullptr && nucleus_window_destroy != nullptr && nucleus_loop != nullptr;
}

static bool _audience_init()
{
  // perform thread lock if not locked already
  {
    std::lock_guard<std::mutex> lock(nucleus_thread_lock_mutex);
    if (nucleus_thread_lock_id == std::thread::id())
    {
      nucleus_thread_lock_id = std::this_thread::get_id();
    }
  }

  // validate thread lock
  SHELL_CHECK_THREAD_LOCK;

  // prevent double initialization
  if (audience_is_initialized())
  {
    return true;
  }

  // nucleus library load order
  std::vector<std::string> dylibs{
#ifdef WIN32
      "audience_windows_edge.dll",
      "audience_windows_ie11.dll",
#elif __APPLE__
      "libaudience_macos_webkit.dylib",
#else
      "libaudience_unix_webkit.so",
#endif
  };

  // iterate libraries and stop at first successful load
  for (auto dylib : dylibs)
  {
    // load library
    auto dylib_abs = dir_of_exe() + PATH_SEPARATOR + dylib;
    TRACEA(info, "trying to load library from path " << dylib_abs);
#ifdef WIN32
    auto dlh = LoadLibraryA(dylib_abs.c_str());
#else
    auto dlh = dlopen(dylib_abs.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif

    // try to lookup symbols if loaded successfully
    if (dlh != nullptr)
    {
#ifdef WIN32
#define LookupFunction GetProcAddress
#else
#define LookupFunction dlsym
#endif

      nucleus_init = (nucleus_init_t)LookupFunction(dlh, "audience_init");
      nucleus_window_create = (nucleus_window_create_t)LookupFunction(dlh, "audience_window_create");
      nucleus_window_destroy = (nucleus_window_destroy_t)LookupFunction(dlh, "audience_window_destroy");
      nucleus_loop = (nucleus_loop_t)LookupFunction(dlh, "audience_loop");

      if (!audience_is_initialized())
      {
        TRACEA(info, "could not find function pointer in library " << dylib);
      }

      // try to initializes and negotiate protocol
      nucleus_protocol_negotiation = {false, false, false};

      if (audience_is_initialized() && nucleus_init(&nucleus_protocol_negotiation))
      {
        TRACEA(info, "library " << dylib << " loaded successfully");
        return true;
      }
      else
      {
        TRACEA(info, "could not initialize library " << dylib);
      }

      // reset function pointer and negotiation in case we failed
      nucleus_init = nullptr;
      nucleus_window_create = nullptr;
      nucleus_window_destroy = nullptr;
      nucleus_loop = nullptr;
      nucleus_protocol_negotiation = {};

#ifdef WIN32
      FreeLibrary(dlh);
#else
      dlclose(dlh);
#endif
    }
    else
    {
      TRACEA(info, "could not load library " << dylib);
#ifdef WIN32
      TRACEW(info, GetLastErrorString().c_str());
#endif
    }
  }

  return false;
}

bool audience_init()
{
  return SAFE_FN(_audience_init, false)();
}

AudienceWindowHandle _audience_window_create(const AudienceWindowDetails *details)
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK;

  // ensure initialization
  if (!audience_is_initialized())
  {
    return {};
  }

  // cases which do not require an webserver
  if (details->webapp_type == AUDIENCE_WEBAPP_TYPE_URL && nucleus_protocol_negotiation.nucleus_handles_webapp_type_url)
  {
    return nucleus_window_create(details);
  }

  if (details->webapp_type == AUDIENCE_WEBAPP_TYPE_DIRECTORY && nucleus_protocol_negotiation.nucleus_handles_webapp_type_directory)
  {
    return nucleus_window_create(details);
  }

  // create a webserver and translate directory based webapp to url webapp
  if (details->webapp_type == AUDIENCE_WEBAPP_TYPE_DIRECTORY && nucleus_protocol_negotiation.nucleus_handles_webapp_type_url)
  {
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

    // start webserver on available port
    std::string address = "127.0.0.1";
    unsigned short ws_port = 0;

    auto ws_handle = webserver_start(address, ws_port, converter.to_bytes(details->webapp_location), 3);

    // construct url of webapp
    auto webapp_url = std::wstring(L"http://") + converter.from_bytes(address) + L":" + std::to_wstring(ws_port) + L"/";

    TRACEW(info, L"serving app from folder " << details->webapp_location);
    TRACEW(info, L"serving app via url " << webapp_url);

    // create window
    AudienceWindowDetails new_details{
        AUDIENCE_WEBAPP_TYPE_URL,
        webapp_url.c_str(),
        details->loading_title};

    auto window_handle = nucleus_window_create(&new_details);

    if (window_handle == AudienceWindowHandle{})
    {
      webserver_stop(ws_handle);
      return {};
    }

    // attach webserver to registry
    nucleus_webserver_registry[window_handle] = ws_handle;

    return window_handle;
  }

  throw std::invalid_argument("cannot serve web app, either unknown type or protocol insufficient");
}

AudienceWindowHandle audience_window_create(const AudienceWindowDetails *details)
{
  return SAFE_FN(_audience_window_create, AudienceWindowHandle{})(details);
}

void _audience_window_destroy(AudienceWindowHandle handle)
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK;

  // ensure initialization
  if (!audience_is_initialized())
  {
    return;
  }

  // destroy window
  nucleus_window_destroy(handle);

  // check if we have to stop a running webservice
  auto i = nucleus_webserver_registry.find(handle);
  if (i != nucleus_webserver_registry.end())
  {
    webserver_stop(i->second);
    nucleus_webserver_registry.erase(i);
  }
}

void audience_window_destroy(AudienceWindowHandle handle)
{
  SAFE_FN(_audience_window_destroy)
  (handle);
}

void _audience_loop()
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK;

  // ensure initialization
  if (!audience_is_initialized())
  {
    return;
  }

  // run loop
  nucleus_loop();
}

void audience_loop()
{
  SAFE_FN(_audience_loop)
  ();
}
