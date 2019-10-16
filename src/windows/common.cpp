#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define AUDIENCE_COMPILING_INNER
#include "../inner.h"

HINSTANCE hInstanceEXE = nullptr;
HINSTANCE hInstanceDLL = nullptr;

BOOL WINAPI DllMain(_In_ HINSTANCE hinstDLL, _In_ DWORD fdwReason, _In_ LPVOID lpvReserved)
{
  hInstanceEXE = GetModuleHandleW(NULL);
  hInstanceDLL = hinstDLL;
  return true;
}

void audience_inner_loop()
{
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}
