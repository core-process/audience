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
#include <boost/bimap.hpp>

#include "../../common/trace.h"
#include "../../common/safefn.h"
#include "webserver/process.h"
#include "lib.h"
#include "nucleus.h"
#include "util.h"

static std::mutex nucleus_thread_lock_mutex;
static std::thread::id nucleus_thread_lock_id;

#define SHELL_CHECK_THREAD_LOCK(on_fail)                                                                           \
  {                                                                                                                \
    bool thread_check_failed = false;                                                                              \
    {                                                                                                              \
      std::lock_guard<std::mutex> lock(nucleus_thread_lock_mutex);                                                 \
      if (nucleus_thread_lock_id == std::thread::id())                                                             \
      {                                                                                                            \
        throw std::runtime_error("audience needs to be locked to a specific thread (call audience_init() first)"); \
      }                                                                                                            \
      if (nucleus_thread_lock_id != std::this_thread::get_id())                                                    \
      {                                                                                                            \
        thread_check_failed = true;                                                                                \
      }                                                                                                            \
    }                                                                                                              \
    if (thread_check_failed)                                                                                       \
    {                                                                                                              \
      on_fail;                                                                                                     \
    }                                                                                                              \
  }

#define SHELL_CHECK_THREAD_LOCK_THROW SHELL_CHECK_THREAD_LOCK(throw std::runtime_error("audience cannot be called from multiple threads (stick to you applications main thread)"))

#define SHELL_DISPATCH_SYNC(fn, return_type, ...)                                          \
  {                                                                                        \
    TRACEA(info, "dispatching " << #fn << " to main thread");                              \
    return_type return_value{};                                                            \
    auto task_lambda = [&]() { return_value = fn(__VA_ARGS__); };                          \
    auto task = [](void *context) { (*static_cast<decltype(task_lambda) *>(context))(); }; \
    nucleus_dispatch_sync(task, &task_lambda);                                             \
    return return_value;                                                                   \
  }

#define SHELL_DISPATCH_SYNC_VOID(fn, ...)                                                  \
  {                                                                                        \
    TRACEA(info, "dispatching " << #fn << " to main thread");                              \
    auto task_lambda = [&]() { fn(__VA_ARGS__); };                                         \
    auto task = [](void *context) { (*static_cast<decltype(task_lambda) *>(context))(); }; \
    nucleus_dispatch_sync(task, &task_lambda);                                             \
  }

static nucleus_init_t nucleus_init = nullptr;
static nucleus_window_create_t nucleus_window_create = nullptr;
static nucleus_window_post_message_t nucleus_window_post_message = nullptr;
static nucleus_window_destroy_t nucleus_window_destroy = nullptr;
static nucleus_main_t nucleus_main = nullptr;
static nucleus_dispatch_sync_t nucleus_dispatch_sync = nullptr;
static nucleus_dispatch_async_t nucleus_dispatch_async = nullptr;

static AudienceNucleusProtocolNegotiation nucleus_protocol_negotiation{};
static boost::bimap<AudienceWindowHandle, WebserverContext> nucleus_webserver_registry{};

static AudienceEventHandler user_process_event_handler{};
static std::map<AudienceWindowHandle, AudienceWindowEventHandler> user_window_event_handler{};

static void _audience_on_window_message(AudienceWindowHandle handle, const char *message);
static void _audience_on_window_will_close(AudienceWindowHandle handle, bool *prevent_close);
static void _audience_on_window_close(AudienceWindowHandle handle, bool *prevent_quit);
static void _audience_on_process_will_quit(bool *prevent_quit);
static void _audience_on_process_quit();

static bool audience_is_initialized()
{
  return nucleus_init != nullptr && nucleus_window_create != nullptr && nucleus_window_post_message != nullptr && nucleus_window_destroy != nullptr && nucleus_main != nullptr && nucleus_dispatch_sync != nullptr && nucleus_dispatch_async != nullptr;
}

static bool _audience_init(const AudienceDetails *details, const AudienceEventHandler *event_handler)
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
  SHELL_CHECK_THREAD_LOCK_THROW;

  // prevent double initialization
  if (audience_is_initialized())
  {
    return true;
  }

  // nucleus library load order
  std::vector<std::string> dylibs{};
  for (size_t i = 0; i < AUDIENCE_DETAILS_LOAD_ORDER_ENTRIES; ++i)
  {
#ifdef WIN32
    auto tech = details->load_order.windows[i];
#elif __APPLE__
    auto tech = details->load_order.macos[i];
#else
    auto tech = details->load_order.unix[i];
#endif

    switch (tech)
    {
#ifdef WIN32
    case AUDIENCE_NUCLEUS_WINDOWS_NONE:
      break;
    case AUDIENCE_NUCLEUS_WINDOWS_EDGE:
      dylibs.push_back("audience_windows_edge.dll");
      break;
    case AUDIENCE_NUCLEUS_WINDOWS_IE11:
      dylibs.push_back("audience_windows_ie11.dll");
      break;
#elif __APPLE__
    case AUDIENCE_NUCLEUS_MACOS_NONE:
      break;
    case AUDIENCE_NUCLEUS_MACOS_WEBKIT:
      dylibs.push_back("libaudience_macos_webkit.dylib");
      break;
#else
    case AUDIENCE_NUCLEUS_UNIX_NONE:
      break;
    case AUDIENCE_NUCLEUS_UNIX_WEBKIT:
      dylibs.push_back("libaudience_unix_webkit.so");
      break;
#endif
    default:
      TRACEA(error, "unknown nucleus technology selector: " << tech);
      break;
    }
  }

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
      nucleus_window_post_message = (nucleus_window_post_message_t)LookupFunction(dlh, "audience_window_post_message");
      nucleus_window_destroy = (nucleus_window_destroy_t)LookupFunction(dlh, "audience_window_destroy");
      nucleus_main = (nucleus_main_t)LookupFunction(dlh, "audience_main");
      nucleus_dispatch_sync = (nucleus_dispatch_sync_t)LookupFunction(dlh, "audience_dispatch_sync");
      nucleus_dispatch_async = (nucleus_dispatch_async_t)LookupFunction(dlh, "audience_dispatch_async");

      if (!audience_is_initialized())
      {
        TRACEA(info, "could not find function pointer in library " << dylib);
      }

      // try to initializes and negotiate protocol
      nucleus_protocol_negotiation = {};
      nucleus_protocol_negotiation.shell_event_handler.window_level.on_message = _audience_on_window_message;
      nucleus_protocol_negotiation.shell_event_handler.window_level.on_will_close = _audience_on_window_will_close;
      nucleus_protocol_negotiation.shell_event_handler.window_level.on_close = _audience_on_window_close;
      nucleus_protocol_negotiation.shell_event_handler.process_level.on_will_quit = _audience_on_process_will_quit;
      nucleus_protocol_negotiation.shell_event_handler.process_level.on_quit = _audience_on_process_quit;

      if (audience_is_initialized() && nucleus_init(&nucleus_protocol_negotiation))
      {
        TRACEA(info, "library " << dylib << " loaded successfully");
        break;
      }
      else
      {
        TRACEA(info, "could not initialize library " << dylib);
      }

      // reset function pointer and negotiation in case we failed
      nucleus_init = nullptr;
      nucleus_window_create = nullptr;
      nucleus_window_post_message = nullptr;
      nucleus_window_destroy = nullptr;
      nucleus_main = nullptr;
      nucleus_dispatch_sync = nullptr;
      nucleus_dispatch_async = nullptr;
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

  // bail out in case we failed
  if (!audience_is_initialized())
  {
    return false;
  }

  // copy event handler info, then we are done
  user_process_event_handler = *event_handler;
  return true;
}

bool audience_init(const AudienceDetails *details, const AudienceEventHandler *event_handler)
{
  return SAFE_FN(_audience_init, false)(details, event_handler);
}

AudienceWindowHandle _audience_window_create(const AudienceWindowDetails *details, const AudienceWindowEventHandler *event_handler)
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK(SHELL_DISPATCH_SYNC(audience_window_create, AudienceWindowHandle, details, event_handler));

  // ensure initialization
  if (!audience_is_initialized())
  {
    return {};
  }

  AudienceWindowHandle window_handle{};

  // cases which do not require an webserver
  if (details->webapp_type == AUDIENCE_WEBAPP_TYPE_URL && nucleus_protocol_negotiation.nucleus_handles_webapp_type_url)
  {
    window_handle = nucleus_window_create(details);
  }
  else if (details->webapp_type == AUDIENCE_WEBAPP_TYPE_DIRECTORY && nucleus_protocol_negotiation.nucleus_handles_webapp_type_directory)
  {
    window_handle = nucleus_window_create(details);
  }
  // create a webserver and translate directory based webapp to url webapp
  else if (details->webapp_type == AUDIENCE_WEBAPP_TYPE_DIRECTORY && nucleus_protocol_negotiation.nucleus_handles_webapp_type_url)
  {
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;

    // start webserver on available port
    std::string address = "127.0.0.1";
    unsigned short ws_port = 0;

    auto ws_ctx = webserver_start(address, ws_port, converter.to_bytes(details->webapp_location), 3, [](WebserverContext context, std::string message) {
      auto task_lambda = [&]() {
        auto ic = nucleus_webserver_registry.right.find(context);
        if (ic != nucleus_webserver_registry.right.end())
        {
          auto wh = ic->second;
          _audience_on_window_message(wh, message.c_str());
        }
      };
      auto task = [](void *context) { (*static_cast<decltype(task_lambda) *>(context))(); };
      nucleus_dispatch_sync(task, &task_lambda);
    });

    // construct url of webapp
    auto webapp_url = std::wstring(L"http://") + converter.from_bytes(address) + L":" + std::to_wstring(ws_port) + L"/";

    TRACEW(info, L"serving app from folder " << details->webapp_location);
    TRACEW(info, L"serving app via url " << webapp_url);

    // adapt window details
    AudienceWindowDetails new_details = *details;
    new_details.webapp_type = AUDIENCE_WEBAPP_TYPE_URL;
    new_details.webapp_location = webapp_url.c_str();

    // create window
    window_handle = nucleus_window_create(&new_details);

    if (window_handle == AudienceWindowHandle{})
    {
      // stop the web server in case we could not create window
      webserver_stop(ws_ctx);
    }
    else
    {
      // attach webserver to registry
      nucleus_webserver_registry.insert(boost::bimap<AudienceWindowHandle, WebserverContext>::value_type(window_handle, ws_ctx));
    }
  }
  else
  {
    throw std::invalid_argument("cannot serve web app, either unknown type or protocol insufficient");
  }

  // copy event handler info
  if (window_handle != AudienceWindowHandle{})
  {
    user_window_event_handler[window_handle] = *event_handler;
  }

  return window_handle;
}

AudienceWindowHandle audience_window_create(const AudienceWindowDetails *details, const AudienceWindowEventHandler *event_handler)
{
  return SAFE_FN(_audience_window_create, AudienceWindowHandle{})(details, event_handler);
}

void _audience_window_post_message(AudienceWindowHandle handle, const char *message)
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK(SHELL_DISPATCH_SYNC_VOID(audience_window_post_message, handle, message));

  // ensure initialization
  if (!audience_is_initialized())
  {
    return;
  }

  // delegate post message to nucleus, in case protocol demands
  if (nucleus_protocol_negotiation.nucleus_handles_messaging)
  {
    TRACEA(debug, "delegate post message to nucleus");
    nucleus_window_post_message(handle, message);
    return;
  }

  // post message
  auto iws = nucleus_webserver_registry.left.find(handle);
  if (iws != nucleus_webserver_registry.left.end())
  {
    TRACEA(debug, "posting message to frontend");
    webserver_post_message(iws->second, std::string(message));
  }
  else
  {
    TRACEA(error, "could not find webserver for window handle");
  }
}

void audience_window_post_message(AudienceWindowHandle handle, const char *message)
{
  SAFE_FN(_audience_window_post_message)
  (handle, message);
}

void _audience_window_destroy(AudienceWindowHandle handle)
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK(SHELL_DISPATCH_SYNC_VOID(audience_window_destroy, handle));

  // ensure initialization
  if (!audience_is_initialized())
  {
    return;
  }

  // destroy window
  nucleus_window_destroy(handle);
}

void audience_window_destroy(AudienceWindowHandle handle)
{
  SAFE_FN(_audience_window_destroy)
  (handle);
}

void _audience_main()
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK_THROW;

  // ensure initialization
  if (!audience_is_initialized())
  {
    return;
  }

  // run loop
  nucleus_main();
}

void audience_main()
{
  SAFE_FN(_audience_main)
  ();
}

static inline void _audience_on_window_message(AudienceWindowHandle handle, const char *message)
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK(SHELL_DISPATCH_SYNC_VOID(_audience_on_window_message, handle, message));

  // call user event handler
  auto ehi = user_window_event_handler.find(handle);
  if (ehi != user_window_event_handler.end())
  {
    if (ehi->second.on_message.handler != nullptr)
    {
      ehi->second.on_message.handler(
          handle,
          ehi->second.on_message.context,
          message);
    }
  }
}

static inline void _audience_on_window_will_close(AudienceWindowHandle handle, bool *prevent_close)
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK(SHELL_DISPATCH_SYNC_VOID(_audience_on_window_will_close, handle, prevent_close));

  // call user event handler
  auto ehi = user_window_event_handler.find(handle);
  if (ehi != user_window_event_handler.end())
  {
    if (ehi->second.on_will_close.handler != nullptr)
    {
      ehi->second.on_will_close.handler(
          handle,
          ehi->second.on_will_close.context,
          prevent_close);
    }
  }
}

static inline void _audience_on_window_close(AudienceWindowHandle handle, bool *prevent_quit)
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK(SHELL_DISPATCH_SYNC_VOID(_audience_on_window_close, handle, prevent_quit));

  // call user event handler
  auto ehi = user_window_event_handler.find(handle);
  if (ehi != user_window_event_handler.end())
  {
    if (ehi->second.on_close.handler != nullptr)
    {
      ehi->second.on_close.handler(
          handle,
          ehi->second.on_close.context,
          prevent_quit);
    }
  }

  // check if we have to stop a running webservice
  auto wsi = nucleus_webserver_registry.left.find(handle);
  if (wsi != nucleus_webserver_registry.left.end())
  {
    auto ws = wsi->second;
    nucleus_webserver_registry.left.erase(wsi);
    webserver_stop(ws);
  }
}

static inline void _audience_on_process_will_quit(bool *prevent_quit)
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK(SHELL_DISPATCH_SYNC_VOID(_audience_on_process_will_quit, prevent_quit));

  // call user event handler
  if (user_process_event_handler.on_will_quit.handler != nullptr)
  {
    user_process_event_handler.on_will_quit.handler(
        user_process_event_handler.on_will_quit.context,
        prevent_quit);
  }
}

static inline void _audience_on_process_quit()
{
  // validate thread lock
  SHELL_CHECK_THREAD_LOCK(SHELL_DISPATCH_SYNC_VOID(_audience_on_process_quit));

  // call user event handler
  if (user_process_event_handler.on_quit.handler != nullptr)
  {
    user_process_event_handler.on_quit.handler(
        user_process_event_handler.on_quit.context);
  }
}
