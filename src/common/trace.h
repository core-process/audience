#pragma once

#ifdef ENABLE_TRACE

#define _TRACE_STRINGIFY2(m) #m
#define _TRACE_STRINGIFY(m) _TRACE_STRINGIFY2(m)
#define _TRACE_WIDE2(m) L##m
#define _TRACE_WIDE(m) _TRACE_WIDE2(m)

#ifdef _MSC_VER

#include <windows.h>
#include <sstream>
#define TRACEA(level, message)                        \
  {                                                   \
    std::ostringstream str;                           \
    str                                               \
        << __FILE__                                   \
        << "(" << _TRACE_STRINGIFY(__LINE__) << "): " \
        << "[" << _TRACE_STRINGIFY(level) << "] "     \
        << message                                    \
        << std::endl;                                 \
    OutputDebugStringA(str.str().c_str());            \
  }
#define TRACEW(level, message)                                       \
  {                                                                  \
    std::wostringstream str;                                         \
    str                                                              \
        << _TRACE_WIDE(__FILE__)                                     \
        << L"(" << _TRACE_WIDE(_TRACE_STRINGIFY(__LINE__)) << L"): " \
        << L"[" << _TRACE_WIDE(_TRACE_STRINGIFY(level)) << L"] "     \
        << message                                                   \
        << std::endl;                                                \
    OutputDebugStringW(str.str().c_str());                           \
  }

#else // _MSC_VER

#include <iostream>
#define TRACEA(level, message)                      \
  std::cerr                                         \
      << __FILE__                                   \
      << "(" << _TRACE_STRINGIFY(__LINE__) << "): " \
      << "[" << _TRACE_STRINGIFY(level) << "] "     \
      << message                                    \
      << std::endl;
#define TRACEW(level, message)                                     \
  std::wcerr                                                       \
      << _TRACE_WIDE(__FILE__)                                     \
      << L"(" << _TRACE_WIDE(_TRACE_STRINGIFY(__LINE__)) << L"): " \
      << L"[" << _TRACE_WIDE(_TRACE_STRINGIFY(level)) << L"] "     \
      << message                                                   \
      << std::endl;

#endif

#else // ENABLE_TRACE

#define TRACEA(level, message)
#define TRACEW(level, message)

#endif // ENABLE_TRACE

#ifdef WIN32
#include <windows.h>
#include <string>
inline std::wstring GetLastErrorString()
{
  wchar_t buf[256] = {0};
  FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
  return std::wstring(buf);
}
#endif
