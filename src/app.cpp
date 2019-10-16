#ifdef WIN32
#include <windows.h>
#endif

#include <vector>
#include <string>
#include <locale>
#include <codecvt>

#include "lib.h"

#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow)
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

  // create and show window
  if (!audience_init())
  {
    return 1;
  }

  auto window = audience_window_create(
      args.size() > 1 ? args[1].c_str() : L"Loading...",
      args.size() > 2 ? args[2].c_str() : L"https://mikripatrida.com/en");

  if (window == nullptr)
  {
    return 1;
  }

  audience_loop();
  audience_window_destroy(window);

  return 0;
}
