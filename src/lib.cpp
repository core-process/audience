#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <vector>
#include <string>

#include "lib.h"
#include "inner.h"

audience_inner_init_t audience_inner_init = nullptr;
audience_inner_window_create_t audience_inner_window_create = nullptr;
audience_inner_window_destroy_t audience_inner_window_destroy = nullptr;
audience_inner_loop_t audience_inner_loop = nullptr;

#ifdef WIN32
std::wstring GetLastErrorString()
{
  wchar_t buf[256] = {0};
  FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
  return std::wstring(buf);
}
#endif

bool audience_is_initialized()
{
  return audience_inner_init != nullptr && audience_inner_window_create != nullptr && audience_inner_window_destroy != nullptr && audience_inner_loop != nullptr;
}

bool audience_init()
{
  if (audience_is_initialized())
  {
    return true;
  }

#ifdef WIN32
  std::vector<std::string> dlls{"audience_windows_edge.dll", "audience_windows_ie11.dll"};
  for (auto dll : dlls)
  {
    auto instance = LoadLibraryA(dll.c_str());
    if (instance != nullptr)
    {
      audience_inner_init = (audience_inner_init_t)GetProcAddress(instance, "audience_inner_init");
      audience_inner_window_create = (audience_inner_window_create_t)GetProcAddress(instance, "audience_inner_window_create");
      audience_inner_window_destroy = (audience_inner_window_destroy_t)GetProcAddress(instance, "audience_inner_window_destroy");
      audience_inner_loop = (audience_inner_loop_t)GetProcAddress(instance, "audience_inner_loop");

      if (audience_is_initialized() && audience_inner_init())
      {
        return true;
      }

      audience_inner_init = nullptr;
      audience_inner_window_create = nullptr;
      audience_inner_window_destroy = nullptr;
      audience_inner_loop = nullptr;
    }
    else
    {
      OutputDebugStringW(GetLastErrorString().c_str());
      OutputDebugStringW(L"\n");
    }
  }
#else
#endif

  return false;
}

void *audience_window_create(const wchar_t *const title, const wchar_t *const url)
{
  if (!audience_is_initialized())
  {
    return nullptr;
  }
  return audience_inner_window_create(title, url);
}

void audience_window_destroy(void *window)
{
  if (!audience_is_initialized())
  {
    return;
  }
  audience_inner_window_destroy(window);
}

void audience_loop()
{
  if (!audience_is_initialized())
  {
    return;
  }
  audience_inner_loop();
}
