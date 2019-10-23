#ifdef WIN32
#include <windows.h>
#endif

#include <vector>
#include <string>
#include <locale>
#include <codecvt>

#include <audience.h>
#include "../common/trace.h"

#include "webserver/process.h"
#include <thread>
#include <chrono>

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

  unsigned short ws_port = 0;
  auto ws = webserver_start("127.0.0.1", ws_port, "/Users/niklas/Dev/avantgarde/avantgarde-mythicalcards/src/frontend", 3);

  // create and show window
  if (!audience_init())
  {
    TRACEA(error, "could not initialize audience");
    return 1;
  }

  auto window = audience_window_create(
      args.size() > 1 ? args[1].c_str() : L"Loading...",
      args.size() > 2 ? args[2].c_str() : (std::wstring(L"http://127.0.0.1:") + std::to_wstring(ws_port) + L"/").c_str());

  if (window == nullptr)
  {
    TRACEA(error, "could not create audience window");
    return 1;
  }

  audience_loop();
  audience_window_destroy(window);

  webserver_stop(ws);

  return 0;
}
