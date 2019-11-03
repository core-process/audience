#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#elif __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <optional>
#include <spdlog/spdlog.h>

#include <audience.h>

#include "../../common/utf.h"
#include "../../common/memory_scope.h"
#include "../../common/logger.h"
#include "args.h"
#include "channel.h"

void display_message(const std::string &message, bool is_error);

#ifdef WIN32
int WINAPI WinMain(_In_ HINSTANCE hInt, _In_opt_ HINSTANCE hPrevInst, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
  // activate COM/OLE always
  {
    auto r = OleInitialize(NULL);
    if (r != S_OK && r != S_FALSE)
    {
      display_message("Could not initialize COM/OLE.", true);
      return 2;
    }
  }
#else
int main(int argc, char **argv)
{
#endif
  try
  {
    // setup logger
    setup_logger("audience.shell.app");

    // our memory guard
    memory_scope mem;

    // init random generator always
    std::srand(std::time(nullptr));

    // set up program options
    cxxopts::Options options("audience", "Small adaptive cross-plattform webview window solution");
    options.add_options()("win", "Nucleus load order for Windows; supported: edge, ie11", cxxopts::value<std::vector<std::string>>()->default_value("edge,ie11"));
    options.add_options()("mac", "Nucleus load order for macOS; supported: webkit", cxxopts::value<std::vector<std::string>>()->default_value("webkit"));
    options.add_options()("unix", "Nucleus load order for Unix; supported: webkit", cxxopts::value<std::vector<std::string>>()->default_value("webkit"));
    options.add_options()("i,icons", "Icon set", cxxopts::value<std::vector<std::string>>());
    options.add_options()("d,dir", "Web app directory; local file system path", cxxopts::value<std::string>());
    options.add_options()("u,url", "Web app URL", cxxopts::value<std::string>());
    options.add_options()("t,title", "Loading title", cxxopts::value<std::string>());
    options.add_options()("p,pos", "Position of window", cxxopts::value<std::vector<double>>());
    options.add_options()("s,size", "Size of window", cxxopts::value<std::vector<double>>());
    options.add_options()("decorated", "Decorated window; use =false for undecorated window", cxxopts::value<bool>());
    options.add_options()("resizable", "Resizable window; use =false for non-resizable window", cxxopts::value<bool>());
    options.add_options()("top", "Window should stay on top always", cxxopts::value<bool>());
    options.add_options()("dev", "Developer mode; if supported by web view", cxxopts::value<bool>());
    options.add_options()("c,channel", "Command and event channel; a named pipe", cxxopts::value<std::string>());
    options.add_options()("h,help", "Print help", cxxopts::value<bool>());

    // parse arguments
    auto args_or_error = PARSE_OPTS(options);

    auto display_help = [&options](std::optional<std::string> e = std::nullopt) {
      std::string message;
      if (e)
      {
        message = std::string("Invalid argument: ") + *e + "\n\n";
      }
      message += options.help();
      display_message(message, !!e);
    };

    if (std::holds_alternative<cxxopts::OptionParseException>(args_or_error))
    {
      display_help(std::string(std::get<cxxopts::OptionParseException>(args_or_error).what()) + ".");
      return 1;
    }

    auto args = std::get<cxxopts::ParseResult>(args_or_error);

    if (args["help"].count() > 0 && args["help"].as<bool>())
    {
      display_help();
      return 1;
    }

    if (args["dir"].count() > 0 && args["url"].count() > 0)
    {
      display_help("Use either --dir or --url, not both at the same time.");
      return 1;
    }

#if defined(WIN32)
    std::wstring selected_app_dir;
#endif
    if (args["dir"].count() == 0 && args["url"].count() == 0 && args["channel"].count() == 0)
    {
#if defined(WIN32)
      BROWSEINFOW bi;
      ZeroMemory(&bi, sizeof(BROWSEINFOW));
      bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
      bi.lpszTitle = L"Please select web app folder:";
      LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
      if (pidl == nullptr)
      {
        return 1;
      }
      wchar_t buffer[MAX_PATH + 1];
      if (!SHGetPathFromIDListW(pidl, buffer))
      {
        return 1;
      }
      selected_app_dir = buffer;
#else
      display_help("Use either --dir or --url and/or --channel, otherwise there is nothing we can do for you.");
      return 1;
#endif
    }

    // prepare channel
    bool do_activate_channel = args["channel"].count() > 0;
    if (do_activate_channel)
    {
      channel_prepare(args["channel"].as<std::string>());
    }

    // init audience
    AudienceAppDetails ad{};

    {
      size_t i = 0;
      for (auto n : args["win"].as<std::vector<std::string>>())
      {
        if (i >= AUDIENCE_APP_DETAILS_LOAD_ORDER_ENTRIES)
          break;
        if (n == "edge")
          ad.load_order.windows[i] = AUDIENCE_NUCLEUS_WINDOWS_EDGE;
        else if (n == "ie11")
          ad.load_order.windows[i] = AUDIENCE_NUCLEUS_WINDOWS_IE11;
        else
        {
          display_help("Invalid nucleus \"" + n + "\" for Windows.");
          return 1;
        }
        i += 1;
      }
    }

    {
      size_t i = 0;
      for (auto n : args["mac"].as<std::vector<std::string>>())
      {
        if (i >= AUDIENCE_APP_DETAILS_LOAD_ORDER_ENTRIES)
          break;
        if (n == "webkit")
          ad.load_order.macos[i] = AUDIENCE_NUCLEUS_MACOS_WEBKIT;
        else
        {
          display_help("Invalid nucleus \"" + n + "\" for macOS.");
          return 1;
        }
        i += 1;
      }
    }

    {
      size_t i = 0;
      for (auto n : args["unix"].as<std::vector<std::string>>())
      {
        if (i >= AUDIENCE_APP_DETAILS_LOAD_ORDER_ENTRIES)
          break;
        if (n == "webkit")
          ad.load_order.unix[i] = AUDIENCE_NUCLEUS_UNIX_WEBKIT;
        else
        {
          display_help("Invalid nucleus \"" + n + "\" for Unix.");
          return 1;
        }
        i += 1;
      }
    }

    if (args["icons"].count() > 0)
    {
      size_t i = 0;
      for (auto icon : args["icons"].as<std::vector<std::string>>())
      {
        if (i >= AUDIENCE_APP_DETAILS_ICON_SET_ENTRIES)
          break;
        ad.icon_set[i] = mem.alloc_string(utf8_to_utf16(icon));
        i += 1;
      }
    }

    AudienceAppEventHandler aeh{};
    aeh.on_will_quit.handler = [](void *context, bool *prevent_quit) {
      SPDLOG_DEBUG("event will_quit");
    };
    if (do_activate_channel)
    {
      aeh.on_quit.handler = [](void *context) {
        SPDLOG_DEBUG("event quit");
        channel_shutdown();
      };
    }
    else
    {
      aeh.on_quit.handler = [](void *context) {
        SPDLOG_DEBUG("event quit");
      };
    }

    if (!audience_init(&ad, &aeh))
    {
      display_message("Could not initialize Audience framework.", true);
      return 2;
    }

    // create window
    bool do_create_window = false;
    AudienceWindowDetails wd{};

#ifdef WIN32
    if (selected_app_dir.length() > 0)
    {
      do_create_window = true;
      wd.webapp_type = AUDIENCE_WEBAPP_TYPE_DIRECTORY;
      wd.webapp_location = selected_app_dir.c_str();
    }
#endif

    if (args["dir"].count() > 0)
    {
      do_create_window = true;
      wd.webapp_type = AUDIENCE_WEBAPP_TYPE_DIRECTORY;
      wd.webapp_location = mem.alloc_string(utf8_to_utf16(args["dir"].as<std::string>()));
    }

    if (args["url"].count() > 0)
    {
      do_create_window = true;
      wd.webapp_type = AUDIENCE_WEBAPP_TYPE_URL;
      wd.webapp_location = mem.alloc_string(utf8_to_utf16(args["url"].as<std::string>()));
    }

    if (args["title"].count() > 0)
    {
      wd.loading_title = mem.alloc_string(utf8_to_utf16(args["title"].as<std::string>()));
    }

    {
      auto screens = audience_screen_list();
      auto workspace = screens.screens[screens.focused].workspace;

      if (args["size"].count() > 0)
      {
        auto size = args["size"].as<std::vector<double>>();
        if (size.size() != 2)
        {
          display_help("Size needs to be formatted as width,height");
          return 1;
        }
        wd.position.size.width = size[0];
        wd.position.size.height = size[1];
      }
      else
      {
        wd.position.size.width = workspace.size.width * 0.6;
        wd.position.size.height = workspace.size.height * 0.6;
      }

      if (args["pos"].count() > 0)
      {
        auto pos = args["pos"].as<std::vector<double>>();
        if (pos.size() != 2)
        {
          display_help("Position needs to be formatted as x,y");
          return 1;
        }
        wd.position.origin.x = std::max(pos[0], workspace.origin.x);
        wd.position.origin.y = std::max(pos[1], workspace.origin.y);
      }
      else
      {
        wd.position.size.width = std::min(wd.position.size.width, workspace.size.width);
        wd.position.size.height = std::min(wd.position.size.height, workspace.size.height);
        wd.position.origin.x = workspace.origin.x + (workspace.size.width - wd.position.size.width) * 0.5;
        wd.position.origin.y = workspace.origin.y + (workspace.size.height - wd.position.size.height) * 0.5;
      }
    }

    if (args["decorated"].count() > 0)
    {
      wd.styles.not_decorated = !args["decorated"].as<bool>();
    }

    if (args["resizable"].count() > 0)
    {
      wd.styles.not_resizable = !args["resizable"].as<bool>();
    }

    if (args["top"].count() > 0)
    {
      wd.styles.always_on_top = args["top"].as<bool>();
    }

    if (args["dev"].count() > 0)
    {
      wd.dev_mode = args["dev"].as<bool>();
    }

    AudienceWindowEventHandler weh{};
    if (do_activate_channel)
    {
      weh.on_message.handler = [](AudienceWindowHandle handle, void *context, const wchar_t *message) {
        SPDLOG_DEBUG("event window::message");
        channel_emit_window_message(handle, message);
      };
      weh.on_will_close.handler = [](AudienceWindowHandle handle, void *context, bool *prevent_close) {
        SPDLOG_DEBUG("event window::will_close");
        *prevent_close = true;
        channel_emit_window_close_intent(handle);
      };
    }
    else
    {
      weh.on_message.handler = [](AudienceWindowHandle handle, void *context, const wchar_t *message) {
        SPDLOG_DEBUG("event window::message");
      };
      weh.on_will_close.handler = [](AudienceWindowHandle handle, void *context, bool *prevent_close) {
        SPDLOG_DEBUG("event window::will_close");
      };
    }
    weh.on_close.handler = [](AudienceWindowHandle handle, void *context, bool *prevent_quit) {
      SPDLOG_DEBUG("event window::close");
    };

    if (do_create_window)
    {
      if (!audience_window_create(&wd, &weh))
      {
        display_message("Could not create Audience window.", true);
        return 2;
      }
    }

    if (do_activate_channel)
    {
      channel_activate();
    }

    audience_main(); // calls exit by itself
    return 0;        // just for the compiler
  }
  catch (const std::exception &e)
  {
    display_message(std::string("An error occured: ") + e.what(), true);
    return 2;
  }
  catch (...)
  {
    display_message(std::string("An unknown error occured."), true);
    return 2;
  }
}

void display_message(const std::string &message, bool is_error)
{
  std::cerr << message << std::endl;
#ifdef WIN32
  MessageBoxW(NULL, utf8_to_utf16(message).c_str(), L"Audience", MB_OK | (is_error ? MB_ICONERROR : MB_ICONINFORMATION));
#elif __APPLE__
  CFStringRef cf_header = CFStringCreateWithCString(kCFAllocatorDefault, "Audience", kCFStringEncodingUTF8);
  CFStringRef cf_message = CFStringCreateWithCString(kCFAllocatorDefault, message.c_str(), kCFStringEncodingUTF8);
  CFOptionFlags result;
  CFUserNotificationDisplayAlert(
      0,
      is_error ? kCFUserNotificationStopAlertLevel : kCFUserNotificationNoteAlertLevel,
      nullptr,
      nullptr,
      nullptr,
      cf_header,
      cf_message,
      CFSTR("OK"),
      nullptr,
      nullptr,
      &result);
  CFRelease(cf_header);
  CFRelease(cf_message);
#else
  std::string pmsg;
  pmsg.reserve(message.length() + 1);
  std::copy_if(message.begin(), message.end(), std::back_inserter(pmsg), [](char c) {
    return std::isprint(c) || c == '\n';
  });
  auto p = popen("xmessage -center -default okay -file -", "w");
  if (p != nullptr)
  {
    fwrite(pmsg.c_str(), 1, pmsg.length(), p);
    pclose(p);
  }
#endif
}
