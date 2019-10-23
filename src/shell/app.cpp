#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include <iostream>
#include <vector>
#include <string>
#include <locale>
#include <codecvt>

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
  if (!audience_init())
  {
    TRACEA(error, "could not initialize audience");
    return 2;
  }

  AudienceWindowDetails window_details{
      AUDIENCE_WEBAPP_TYPE_DIRECTORY,
      app_dir.c_str(),
      nullptr};

  auto window = audience_window_create(&window_details);

  if (window == nullptr)
  {
    TRACEA(error, "could not create audience window");
    return 2;
  }

  audience_loop();
  audience_window_destroy(window);

  return 0;
}
