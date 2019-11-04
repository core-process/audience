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
#include <map>
#include <thread>
#include <mutex>
#include <algorithm>
#include <iterator>
#include <boost/bimap.hpp>

#include "../../common/safefn.h"
#include "../../common/utf.h"
#include "../../common/fs.h"
#include "../../common/logger.h"
#include "../../common/sys_error.h"
#include "webserver/process.h"
#include "lib.h"
#include "nucleus.h"
#include "util.h"

static std::mutex shell_thread_binding_mutex;
static std::thread::id shell_thread_binding_id;

#define SHELL_CHECK_THREAD_BINDING(on_fail)                                                                        \
  {                                                                                                                \
    bool thread_check_failed = false;                                                                              \
    {                                                                                                              \
      std::lock_guard<std::mutex> lock(shell_thread_binding_mutex);                                                \
      if (shell_thread_binding_id == std::thread::id())                                                            \
      {                                                                                                            \
        throw std::runtime_error("audience needs to be locked to a specific thread (call audience_init() first)"); \
      }                                                                                                            \
      if (shell_thread_binding_id != std::this_thread::get_id())                                                   \
      {                                                                                                            \
        thread_check_failed = true;                                                                                \
      }                                                                                                            \
    }                                                                                                              \
    if (thread_check_failed)                                                                                       \
    {                                                                                                              \
      on_fail;                                                                                                     \
    }                                                                                                              \
  }

#define SHELL_CHECK_THREAD_BINDING_THROW SHELL_CHECK_THREAD_BINDING(throw std::runtime_error("audience cannot be called from multiple threads (stick to your applications main thread)"))

#define SHELL_DISPATCH_SYNC(fn, return_type, ...)                                          \
  {                                                                                        \
    return_type return_value{};                                                            \
    auto task_lambda = [&]() { return_value = fn(__VA_ARGS__); };                          \
    auto task = [](void *context) { (*static_cast<decltype(task_lambda) *>(context))(); }; \
    auto ds = nucleus_dispatch_sync.load();                                                \
    if (ds != nullptr)                                                                     \
    {                                                                                      \
      SPDLOG_INFO("dispatching {} to main thread", #fn);                                   \
      ds(task, &task_lambda);                                                              \
    }                                                                                      \
    else                                                                                   \
    {                                                                                      \
      SPDLOG_WARN("could not dispatch {} to main thread", #fn);                            \
    }                                                                                      \
    return return_value;                                                                   \
  }

#define SHELL_DISPATCH_SYNC_VOID(fn, ...)                                                  \
  {                                                                                        \
    auto task_lambda = [&]() { fn(__VA_ARGS__); };                                         \
    auto task = [](void *context) { (*static_cast<decltype(task_lambda) *>(context))(); }; \
    auto ds = nucleus_dispatch_sync.load();                                                \
    if (ds != nullptr)                                                                     \
    {                                                                                      \
      SPDLOG_INFO("dispatching {} to main thread", #fn);                                   \
      ds(task, &task_lambda);                                                              \
    }                                                                                      \
    else                                                                                   \
    {                                                                                      \
      SPDLOG_WARN("could not dispatch {} to main thread", #fn);                            \
    }                                                                                      \
    return;                                                                                \
  }

static nucleus_init_t nucleus_init = nullptr;
static nucleus_screen_list_t nucleus_screen_list = nullptr;
static nucleus_window_list_t nucleus_window_list = nullptr;
static nucleus_window_create_t nucleus_window_create = nullptr;
static nucleus_window_update_position_t nucleus_window_update_position = nullptr;
static nucleus_window_post_message_t nucleus_window_post_message = nullptr;
static nucleus_window_destroy_t nucleus_window_destroy = nullptr;
static nucleus_quit_t nucleus_quit = nullptr;
static nucleus_main_t nucleus_main = nullptr;

static std::atomic<nucleus_dispatch_sync_t> nucleus_dispatch_sync = nullptr;
static std::atomic<nucleus_dispatch_async_t> nucleus_dispatch_async = nullptr;

static AudienceNucleusProtocolNegotiation shell_protocol_negotiation{};
static boost::bimap<AudienceWindowHandle, WebserverContext> shell_webserver_registry{};

static AudienceAppEventHandler audience_app_event_handler{};
static std::map<AudienceWindowHandle, AudienceWindowEventHandler> audience_window_event_handler{};

static std::atomic<bool> audience_is_initialized = false;
static std::atomic<bool> audience_is_shutdown = false;

static inline void shell_unsafe_on_window_message(AudienceWindowHandle handle, const wchar_t *message);
static inline void shell_unsafe_on_window_close_intent(AudienceWindowHandle handle);
static inline void shell_unsafe_on_window_close(AudienceWindowHandle handle, bool is_last_window);
static inline void shell_unsafe_on_app_quit();

static inline bool shell_unsafe_init(const AudienceAppDetails *details, const AudienceAppEventHandler *event_handler)
{
  // setup logger
  setup_logger("audience.shell.lib");

  // perform thread binding if not bound already
  {
    std::lock_guard<std::mutex> lock(shell_thread_binding_mutex);
    if (shell_thread_binding_id == std::thread::id())
    {
      shell_thread_binding_id = std::this_thread::get_id();
    }
  }

  // validate thread binding
  SHELL_CHECK_THREAD_BINDING_THROW;

  // prevent double initialization
  if (audience_is_initialized.load())
  {
    SPDLOG_DEBUG("prevent double initialization");
    return true;
  }

  // nucleus library load order
  std::vector<std::wstring> dylibs{};
  for (size_t i = 0; i < AUDIENCE_APP_DETAILS_LOAD_ORDER_ENTRIES; ++i)
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
      dylibs.push_back(L"audience_windows_edge.dll");
      break;
    case AUDIENCE_NUCLEUS_WINDOWS_IE11:
      dylibs.push_back(L"audience_windows_ie11.dll");
      break;
#elif __APPLE__
    case AUDIENCE_NUCLEUS_MACOS_NONE:
      break;
    case AUDIENCE_NUCLEUS_MACOS_WEBKIT:
      dylibs.push_back(L"libaudience_macos_webkit.dylib");
      break;
#else
    case AUDIENCE_NUCLEUS_UNIX_NONE:
      break;
    case AUDIENCE_NUCLEUS_UNIX_WEBKIT:
      dylibs.push_back(L"libaudience_unix_webkit.so");
      break;
#endif
    default:
      SPDLOG_ERROR("unknown nucleus technology selector: {}", tech);
      break;
    }
  }

  // prepare internal details
  AudienceNucleusAppDetails nucleus_details{};

  std::vector<std::wstring> icon_set_absolute; // ... keeps memory alive
  for (size_t i = 0; i < AUDIENCE_APP_DETAILS_ICON_SET_ENTRIES; ++i)
  {
    if (details->icon_set[i] != nullptr)
    {
      icon_set_absolute.push_back(normalize_path(details->icon_set[i]));
      SPDLOG_DEBUG("normalized icon path: {}", utf16_to_utf8(icon_set_absolute.back()));
      nucleus_details.icon_set[i] = icon_set_absolute.back().c_str();
    }
  }

  // iterate libraries and stop at first successful load
  for (auto dylib : dylibs)
  {
    // load library
    auto dylib_abs = normalize_path(dir_of_exe() + L"/" + dylib);
    SPDLOG_INFO("trying to load library from path {}", utf16_to_utf8(dylib_abs));
#ifdef WIN32
    auto dlh = LoadLibraryW(dylib_abs.c_str());
#else
    auto dlh = dlopen(utf16_to_utf8(dylib_abs).c_str(), RTLD_NOW | RTLD_LOCAL);
#endif

    // try to lookup symbols if loaded successfully
    if (dlh != nullptr)
    {
#ifdef WIN32
#define LookupFunction GetProcAddress
#else
#define LookupFunction dlsym
#endif

      nucleus_init = (nucleus_init_t)LookupFunction(dlh, "nucleus_init");
      nucleus_screen_list = (nucleus_screen_list_t)LookupFunction(dlh, "nucleus_screen_list");
      nucleus_window_list = (nucleus_window_list_t)LookupFunction(dlh, "nucleus_window_list");
      nucleus_window_create = (nucleus_window_create_t)LookupFunction(dlh, "nucleus_window_create");
      nucleus_window_update_position = (nucleus_window_update_position_t)LookupFunction(dlh, "nucleus_window_update_position");
      nucleus_window_post_message = (nucleus_window_post_message_t)LookupFunction(dlh, "nucleus_window_post_message");
      nucleus_window_destroy = (nucleus_window_destroy_t)LookupFunction(dlh, "nucleus_window_destroy");
      nucleus_quit = (nucleus_quit_t)LookupFunction(dlh, "nucleus_quit");
      nucleus_main = (nucleus_main_t)LookupFunction(dlh, "nucleus_main");
      nucleus_dispatch_sync = (nucleus_dispatch_sync_t)LookupFunction(dlh, "nucleus_dispatch_sync");
      nucleus_dispatch_async = (nucleus_dispatch_async_t)LookupFunction(dlh, "nucleus_dispatch_async");

      bool all_funcs_available = nucleus_init != nullptr && nucleus_screen_list != nullptr && nucleus_window_list != nullptr && nucleus_window_create != nullptr && nucleus_window_update_position != nullptr && nucleus_window_post_message != nullptr && nucleus_window_destroy != nullptr && nucleus_quit != nullptr && nucleus_main != nullptr && nucleus_dispatch_sync.load() != nullptr && nucleus_dispatch_async.load() != nullptr;

      if (!all_funcs_available)
      {
        SPDLOG_INFO("could not find function pointer in library {}", utf16_to_utf8(dylib));
      }

      // try to initializes and negotiate protocol
      shell_protocol_negotiation = {};
      shell_protocol_negotiation.shell_event_handler.window_level.on_message = SAFE_FN(shell_unsafe_on_window_message);
      shell_protocol_negotiation.shell_event_handler.window_level.on_close_intent = SAFE_FN(shell_unsafe_on_window_close_intent);
      shell_protocol_negotiation.shell_event_handler.window_level.on_close = SAFE_FN(shell_unsafe_on_window_close);
      shell_protocol_negotiation.shell_event_handler.app_level.on_quit = SAFE_FN(shell_unsafe_on_app_quit);

      if (all_funcs_available && nucleus_init(&shell_protocol_negotiation, &nucleus_details))
      {
        // copy event handler info and set init state
        audience_app_event_handler = *event_handler;
        audience_is_initialized = true;
        SPDLOG_INFO("library {} loaded successfully", utf16_to_utf8(dylib));
        return true;
      }
      else
      {
        SPDLOG_INFO("could not initialize library {}", utf16_to_utf8(dylib));
      }

      // reset function pointer and negotiation in case we failed
      nucleus_init = nullptr;
      nucleus_window_create = nullptr;
      nucleus_window_post_message = nullptr;
      nucleus_window_destroy = nullptr;
      nucleus_quit = nullptr;
      nucleus_main = nullptr;
      nucleus_dispatch_sync = nullptr;
      nucleus_dispatch_async = nullptr;
      shell_protocol_negotiation = {};

#ifdef WIN32
      FreeLibrary(dlh);
#else
      dlclose(dlh);
#endif
    }
    else
    {
      SPDLOG_INFO("could not load library {}", utf16_to_utf8(dylib));
#ifdef WIN32
      SPDLOG_INFO("{}", utf16_to_utf8(sys_get_last_error()));
#endif
    }
  }

  SPDLOG_ERROR("initialization failed");
  return false;
}

bool audience_init(const AudienceAppDetails *details, const AudienceAppEventHandler *event_handler)
{
  return SAFE_FN(shell_unsafe_init, SAFE_FN_DEFAULT(bool))(details, event_handler);
}

static inline AudienceScreenList shell_unsafe_screen_list()
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING(SHELL_DISPATCH_SYNC(audience_screen_list, AudienceScreenList));

  // ensure initialization
  if (!audience_is_initialized.load() || audience_is_shutdown.load())
  {
    SPDLOG_DEBUG("cannot call api in unitialized state");
    return AudienceScreenList{};
  }

  // get screen list
  return nucleus_screen_list();
}

AudienceScreenList audience_screen_list()
{
  return SAFE_FN(shell_unsafe_screen_list, SAFE_FN_DEFAULT(AudienceScreenList))();
}

static inline AudienceWindowList shell_unsafe_window_list()
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING(SHELL_DISPATCH_SYNC(audience_window_list, AudienceWindowList));

  // ensure initialization
  if (!audience_is_initialized.load() || audience_is_shutdown.load())
  {
    SPDLOG_DEBUG("cannot call api in unitialized state");
    return AudienceWindowList{};
  }

  // get window list
  return nucleus_window_list();
}

AudienceWindowList audience_window_list()
{
  return SAFE_FN(shell_unsafe_window_list, SAFE_FN_DEFAULT(AudienceWindowList))();
}

static inline AudienceWindowHandle shell_unsafe_window_create(const AudienceWindowDetails *details, const AudienceWindowEventHandler *event_handler)
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING(SHELL_DISPATCH_SYNC(audience_window_create, AudienceWindowHandle, details, event_handler));

  // ensure initialization
  if (!audience_is_initialized.load() || audience_is_shutdown.load())
  {
    SPDLOG_DEBUG("cannot call api in unitialized state");
    return {};
  }

  // translate web app location to absolute path
  AudienceWindowDetails new_details = *details;
  std::wstring webapp_dir_absolute; // ... variable from outer scope keeps memory alive

  if (new_details.webapp_type == AUDIENCE_WEBAPP_TYPE_DIRECTORY)
  {
    webapp_dir_absolute = normalize_path(std::wstring(new_details.webapp_location));
    SPDLOG_INFO("normalized web app path: {}", utf16_to_utf8(webapp_dir_absolute));
    new_details.webapp_location = webapp_dir_absolute.c_str();
  }

  // collect window handle
  AudienceWindowHandle window_handle{};

  // cases which do not require an webserver
  if (new_details.webapp_type == AUDIENCE_WEBAPP_TYPE_URL && shell_protocol_negotiation.nucleus_handles_webapp_type_url)
  {
    window_handle = nucleus_window_create(&new_details);
  }
  else if (new_details.webapp_type == AUDIENCE_WEBAPP_TYPE_DIRECTORY && shell_protocol_negotiation.nucleus_handles_webapp_type_directory)
  {
    window_handle = nucleus_window_create(&new_details);
  }
  // create a webserver and translate directory based webapp to url webapp
  else if (new_details.webapp_type == AUDIENCE_WEBAPP_TYPE_DIRECTORY && shell_protocol_negotiation.nucleus_handles_webapp_type_url)
  {
    // start webserver on available port
    std::string address = "127.0.0.1";
    unsigned short ws_port = 0;

    auto ws_ctx = webserver_start(address, ws_port, utf16_to_utf8(new_details.webapp_location), 3, [](WebserverContext context, std::wstring message) {
      auto task_lambda = [&]() {
        auto ic = shell_webserver_registry.right.find(context);
        if (ic != shell_webserver_registry.right.end())
        {
          auto wh = ic->second;
          shell_unsafe_on_window_message(wh, message.c_str());
        }
      };
      auto task = [](void *context) { (*static_cast<decltype(task_lambda) *>(context))(); };
      auto ds = nucleus_dispatch_sync.load();
      if (ds != nullptr)
      {
        ds(task, &task_lambda);
      }
    });

    // construct url of webapp
    auto webapp_url = std::wstring(L"http://") + utf8_to_utf16(address) + L":" + std::to_wstring(ws_port) + L"/";

    SPDLOG_INFO("serving app from folder {}", utf16_to_utf8(new_details.webapp_location));
    SPDLOG_INFO("serving app via url {}", utf16_to_utf8(webapp_url));

    // adapt window details
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
      shell_webserver_registry.insert(boost::bimap<AudienceWindowHandle, WebserverContext>::value_type(window_handle, ws_ctx));
    }
  }
  else
  {
    throw std::invalid_argument("cannot serve web app, either unknown type or protocol insufficient");
  }

  // copy event handler info
  if (window_handle != AudienceWindowHandle{})
  {
    audience_window_event_handler[window_handle] = *event_handler;
  }

  return window_handle;
}

AudienceWindowHandle audience_window_create(const AudienceWindowDetails *details, const AudienceWindowEventHandler *event_handler)
{
  return SAFE_FN(shell_unsafe_window_create, SAFE_FN_DEFAULT(AudienceWindowHandle))(details, event_handler);
}

static inline void shell_unsafe_window_update_position(AudienceWindowHandle handle, AudienceRect position)
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING(SHELL_DISPATCH_SYNC_VOID(audience_window_update_position, handle, position));

  // ensure initialization
  if (!audience_is_initialized.load() || audience_is_shutdown.load())
  {
    SPDLOG_DEBUG("cannot call api in unitialized state");
    return;
  }

  // update window position
  return nucleus_window_update_position(handle, position);
}

void audience_window_update_position(AudienceWindowHandle handle, AudienceRect position)
{
  return SAFE_FN(shell_unsafe_window_update_position)(handle, position);
}

static inline void shell_unsafe_window_post_message(AudienceWindowHandle handle, const wchar_t *message)
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING(SHELL_DISPATCH_SYNC_VOID(audience_window_post_message, handle, message));

  // ensure initialization
  if (!audience_is_initialized.load() || audience_is_shutdown.load())
  {
    SPDLOG_DEBUG("cannot call api in unitialized state");
    return;
  }

  // delegate post message to nucleus, in case protocol demands
  if (shell_protocol_negotiation.nucleus_handles_messaging)
  {
    SPDLOG_DEBUG("delegate post message to nucleus");
    return nucleus_window_post_message(handle, message);
  }

  // post message
  auto iws = shell_webserver_registry.left.find(handle);
  if (iws != shell_webserver_registry.left.end())
  {
    SPDLOG_DEBUG("posting message to frontend");
    return webserver_post_message(iws->second, std::wstring(message));
  }
  else
  {
    SPDLOG_ERROR("could not find webserver for window handle");
    return;
  }
}

void audience_window_post_message(AudienceWindowHandle handle, const wchar_t *message)
{
  return SAFE_FN(shell_unsafe_window_post_message)(handle, message);
}

static inline void shell_unsafe_window_destroy(AudienceWindowHandle handle)
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING(SHELL_DISPATCH_SYNC_VOID(audience_window_destroy, handle));

  // ensure initialization
  if (!audience_is_initialized.load() || audience_is_shutdown.load())
  {
    SPDLOG_DEBUG("cannot call api in unitialized state");
    return;
  }

  // destroy window
  return nucleus_window_destroy(handle);
}

void audience_window_destroy(AudienceWindowHandle handle)
{
  return SAFE_FN(shell_unsafe_window_destroy)(handle);
}

static inline void shell_unsafe_quit()
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING(SHELL_DISPATCH_SYNC_VOID(audience_quit));

  // ensure initialization
  if (!audience_is_initialized.load() || audience_is_shutdown.load())
  {
    SPDLOG_DEBUG("cannot call api in unitialized state");
    return;
  }

  // quit
  SPDLOG_TRACE("shell_unsafe_quit() called");
  audience_is_shutdown = true;
  return nucleus_quit();
}

void audience_quit()
{
  return SAFE_FN(shell_unsafe_quit)();
}

static inline void shell_unsafe_main()
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING_THROW;

  // ensure initialization
  if (!audience_is_initialized.load() || audience_is_shutdown.load())
  {
    SPDLOG_DEBUG("cannot call api in unitialized state");
    return;
  }

  // run loop
  return nucleus_main();
}

void audience_main()
{
  return SAFE_FN(shell_unsafe_main)();
}

static inline void shell_unsafe_on_window_message(AudienceWindowHandle handle, const wchar_t *message)
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING_THROW;

  // call user event handler
  auto ehi = audience_window_event_handler.find(handle);
  if (ehi != audience_window_event_handler.end())
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

static inline void shell_unsafe_on_window_close_intent(AudienceWindowHandle handle)
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING_THROW;

  // call user event handler
  auto ehi = audience_window_event_handler.find(handle);
  if (ehi != audience_window_event_handler.end())
  {
    if (ehi->second.on_close_intent.handler != nullptr)
    {
      ehi->second.on_close_intent.handler(
          handle,
          ehi->second.on_close_intent.context);
    }
  }
}

static inline void shell_unsafe_on_window_close(AudienceWindowHandle handle, bool is_last_window)
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING_THROW;

  // call user event handler
  auto ehi = audience_window_event_handler.find(handle);
  if (ehi != audience_window_event_handler.end())
  {
    if (ehi->second.on_close.handler != nullptr)
    {
      ehi->second.on_close.handler(
          handle,
          ehi->second.on_close.context,
          is_last_window);
    }
  }

  // check if we have to stop a running webservice
  auto wsi = shell_webserver_registry.left.find(handle);
  if (wsi != shell_webserver_registry.left.end())
  {
    auto ws = wsi->second;
    shell_webserver_registry.left.erase(wsi);
    webserver_stop(ws);
  }
}

static inline void shell_unsafe_on_app_quit()
{
  // validate thread binding
  SHELL_CHECK_THREAD_BINDING_THROW;

  // disable dispatching from now on
  // nucleus_dispatch_sync = nullptr;
  // nucleus_dispatch_async = nullptr;

  // call user event handler
  if (audience_app_event_handler.on_quit.handler != nullptr)
  {
    audience_app_event_handler.on_quit.handler(
        audience_app_event_handler.on_quit.context);
  }
}
