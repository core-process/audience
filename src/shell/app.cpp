#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include <iostream>
#include <vector>
#include <string>
#include <locale>
#include <codecvt>

// #include <thread>
// #include <chrono>

#include <audience.h>
#include "../common/trace.h"

#ifdef WIN32
int WINAPI WinMain(_In_ HINSTANCE hInt, _In_opt_ HINSTANCE hPrevInst, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
  // prepare arguments
  std::vector<std::wstring> args;
  {
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv != nullptr)
    {
      for (int i = 0; i < argc; ++i)
      {
        args.push_back(argv[i]);
      }
    }
  }
#else
int main(int argc, char **argv)
{
  // prepare arguments
  std::vector<std::wstring> args;
  {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    for (int i = 0; i < argc; ++i)
    {
      args.push_back(converter.from_bytes(argv[i]));
    }
  }
#endif

  // read arguments
  std::wstring app_dir;

  if (args.size() < 2)
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
    std::wcerr << L"Usage: " << args[0] << L" <APP_DIR>" << std::endl;
    return 1;
#endif
  }
  else
  {
    app_dir = args[1];
  }

  // create and show window
  AudienceEventHandler peh{};
  peh.on_will_quit.handler = [](void *context, bool *prevent_quit) { TRACEA(info, "event will_quit"); *prevent_quit = false; };
  peh.on_quit.handler = [](void *context) { TRACEA(info, "event quit"); };

  if (!audience_init(&peh))
  {
    TRACEA(error, "could not initialize audience");
    return 2;
  }

  AudienceWindowDetails wd{};
  wd.webapp_type = AUDIENCE_WEBAPP_TYPE_DIRECTORY;
  wd.webapp_location = app_dir.c_str();

  AudienceWindowEventHandler weh{};
  weh.on_message.handler = [](AudienceWindowHandle handle, void *context, const char *message) { TRACEA(info, "event window::message -> " << message); };
  weh.on_will_close.handler = [](AudienceWindowHandle handle, void *context, bool *prevent_close) { TRACEA(info, "event window::will_close"); *prevent_close = false; };
  weh.on_close.handler = [](AudienceWindowHandle handle, void *context, bool *prevent_quit) { TRACEA(info, "event window::close"); *prevent_quit = false; };

  if (!audience_window_create(&wd, &weh))
  {
    TRACEA(error, "could not create audience window");
    return 2;
  }

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
