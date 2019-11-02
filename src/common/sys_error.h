#pragma once

#ifdef WIN32
#include <windows.h>
inline std::wstring sys_get_last_error()
{
  wchar_t buf[256] = {0};
  FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
  return std::wstring(buf);
}
#endif
