#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <sstream>

// #include <thread>
// #include <chrono>

#include <audience.h>

#include "../../common/trace.h"
#include "../../common/utf.h"
#include "args.h"

extern std::vector<std::wstring> some_quotes;

#ifdef WIN32
int WINAPI WinMain(_In_ HINSTANCE hInt, _In_opt_ HINSTANCE hPrevInst, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
#else
int main(int argc, char **argv)
{
#endif
  // set up program options
  cxxopts::Options options("audience", "Small adaptive cross-plattform webview window solution");
  options.add_options()("a,app", "Web app", cxxopts::value<std::string>());

  // parse arguments
  auto args = PARSE_OPTS(options);

  // read arguments
  std::wstring app_dir;

  if (args.count("app") == 0)
  {
#if defined(WIN32)
    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(BROWSEINFOW));
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpszTitle = L"Please select web app folder:";

    LPITEMIDLIST pidl = NULL;
    if ((pidl = SHBrowseForFolderW(&bi)) != NULL)
    {
      wchar_t buffer[MAX_PATH + 1];
      if (SHGetPathFromIDListW(pidl, buffer))
      {
        app_dir = buffer;
      }
      else
      {
        return 1;
      }
    }
    else
    {
      return 1;
    }
#else
    std::cerr << options.help() << std::endl;
    return 1;
#endif
  }
  else
  {
    app_dir = utf8_to_utf16(args["app"].as<std::string>());
  }

  std::srand(std::time(nullptr));

  // init audience
  AudienceAppDetails ad{};
  ad.load_order.windows[0] = AUDIENCE_NUCLEUS_WINDOWS_EDGE;
  ad.load_order.windows[1] = AUDIENCE_NUCLEUS_WINDOWS_IE11;
  ad.load_order.macos[0] = AUDIENCE_NUCLEUS_MACOS_WEBKIT;
  ad.load_order.unix[0] = AUDIENCE_NUCLEUS_UNIX_WEBKIT;
  ad.icon_set[0] = L"./examples/ping/icons/16.png";
  ad.icon_set[1] = L"./examples/ping/icons/32.png";
  ad.icon_set[2] = L"./examples/ping/icons/64.png";
  ad.icon_set[3] = L"./examples/ping/icons/128.png";
  ad.icon_set[4] = L"./examples/ping/icons/512.png";

  AudienceAppEventHandler aeh{};
  aeh.on_will_quit.handler = [](void *context, bool *prevent_quit) { TRACEA(info, "event will_quit"); *prevent_quit = false; };
  aeh.on_quit.handler = [](void *context) { TRACEA(info, "event quit"); };

  if (!audience_init(&ad, &aeh))
  {
    TRACEA(error, "could not initialize audience");
    return 2;
  }

  // create window
  AudienceWindowDetails wd{};
  wd.webapp_type = AUDIENCE_WEBAPP_TYPE_DIRECTORY;
  wd.webapp_location = app_dir.c_str();
  wd.dev_mode = true;

  // wd.styles.not_decorated = true;
  // wd.styles.not_resizable = true;
  // wd.styles.always_on_top = true;

  {
    auto screens = audience_screen_list();
    auto workspace = screens.screens[screens.focused].workspace;

    TRACEA(debug, "workspace: origin="
                      << workspace.origin.x << "," << workspace.origin.y << " size="
                      << workspace.size.width << "x" << workspace.size.height);

    wd.position = {workspace.origin, {workspace.size.width * 0.5, workspace.size.height}};
  }

  AudienceWindowEventHandler weh{};
  weh.on_message.handler = [](AudienceWindowHandle handle, void *context, const wchar_t *message) {
    TRACEW(info, L"event window::message -> " << message);
    std::wstring command(message);
    if (command == L"quote")
    {
      audience_window_post_message(handle, some_quotes[std::rand() % some_quotes.size()].c_str());
    }
    else if (command.substr(0, 4) == L"pos:")
    {
      auto where = command.substr(4);
      auto screens = audience_screen_list();
      auto workspace = screens.screens[screens.focused].workspace;
      if (where == L"left")
      {
        audience_window_update_position(handle, {workspace.origin, {workspace.size.width * 0.5, workspace.size.height}});
      }
      else if (where == L"top")
      {
        audience_window_update_position(handle, {workspace.origin,
                                                 {workspace.size.width, workspace.size.height * 0.5}});
      }
      else if (where == L"right")
      {
        audience_window_update_position(handle, {{workspace.origin.x + workspace.size.width * 0.5,
                                                  workspace.origin.y},
                                                 {workspace.size.width * 0.5, workspace.size.height}});
      }
      else if (where == L"bottom")
      {
        audience_window_update_position(handle, {{workspace.origin.x,
                                                  workspace.origin.y + workspace.size.height * 0.5},
                                                 {workspace.size.width, workspace.size.height * 0.5}});
      }
      else if (where == L"center")
      {
        audience_window_update_position(handle, {{workspace.origin.x + workspace.size.width * 0.25,
                                                  workspace.origin.y + workspace.size.height * 0.25},
                                                 {workspace.size.width * 0.5, workspace.size.height * 0.5}});
      }
      else
      {
        audience_window_post_message(handle, (std::wstring(L"Unknown position: ") + where).c_str());
      }
    }
    else if (command == L"screens")
    {
      auto screens = audience_screen_list();
      std::wostringstream str;
      for (size_t i = 0; i < screens.count; ++i)
      {
        str << std::endl;
        str << L"Screen " << i << std::endl;
        if (i == screens.primary)
        {
          str << L"- Primary Screen" << std::endl;
        }
        if (i == screens.focused)
        {
          str << L"- Focused Screen" << std::endl;
        }
        str << L"- Frame: origin=" << screens.screens[i].frame.origin.x
            << L"," << screens.screens[i].frame.origin.y
            << L" size=" << screens.screens[i].frame.size.width
            << L"x" << screens.screens[i].frame.size.height << std::endl;
        str << L"- Workspace: origin=" << screens.screens[i].workspace.origin.x
            << L"," << screens.screens[i].workspace.origin.y
            << L" size=" << screens.screens[i].workspace.size.width
            << L"x" << screens.screens[i].workspace.size.height << std::endl;
      }
      audience_window_post_message(handle, str.str().c_str());
    }
    else if (command == L"windows")
    {
      auto windows = audience_window_list();
      std::wostringstream str;
      for (size_t i = 0; i < windows.count; ++i)
      {
        str << std::endl;
        str << L"Window " << i << L" with handle 0x" << std::hex << windows.windows[i].handle << std::dec << std::endl;
        if (i == windows.focused)
        {
          str << L"- Focused Window" << std::endl;
        }
        str << L"- Frame: origin=" << windows.windows[i].frame.origin.x
            << L"," << windows.windows[i].frame.origin.y
            << L" size=" << windows.windows[i].frame.size.width
            << L"x" << windows.windows[i].frame.size.height << std::endl;
        str << L"- Workspace: size=" << windows.windows[i].workspace.width
            << L"x" << windows.windows[i].workspace.height << std::endl;
      }
      audience_window_post_message(handle, str.str().c_str());
    }
    else if (command == L"quit")
    {
      audience_window_destroy(handle);
    }
    else
    {
      audience_window_post_message(handle, (std::wstring(L"Unknown command: ") + command).c_str());
    }
  };
  weh.on_will_close.handler = [](AudienceWindowHandle handle, void *context, bool *prevent_close) { TRACEA(info, "event window::will_close"); *prevent_close = false; };
  weh.on_close.handler = [](AudienceWindowHandle handle, void *context, bool *prevent_quit) { TRACEA(info, "event window::close"); *prevent_quit = false; };

  if (!audience_window_create(&wd, &weh))
  {
    TRACEA(error, "could not create audience window");
    return 2;
  }

  // if (!audience_window_create(&wd, &weh))
  // {
  //   TRACEA(error, "could not create audience window");
  //   return 2;
  // }

  // std::thread background_thread([&]() {
  //   std::this_thread::sleep_for(std::chrono::seconds(3));
  //   if (!audience_window_create(&wd, &weh))
  //   {
  //     TRACEA(error, "could not create audience window");
  //   }
  // });

  audience_main(); // calls exit by itself
  return 0;        // just for the compiler
}
