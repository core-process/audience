#include <windows.h>

#include "load.h"

HINSTANCE hInstanceEXE = nullptr;
HINSTANCE hInstanceDLL = nullptr;

BOOL WINAPI DllMain(_In_ HINSTANCE hinstDLL, _In_ DWORD fdwReason, _In_ LPVOID lpvReserved)
{
  hInstanceEXE = GetModuleHandleW(NULL);
  hInstanceDLL = hinstDLL;
  return true;
}
