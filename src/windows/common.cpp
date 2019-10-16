#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define AUDIENCE_COMPILING_INNER
#include "../inner.h"

void audience_inner_loop()
{
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}
