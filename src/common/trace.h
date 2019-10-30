#pragma once

#include <string>
#include <exception>

#if AUDIENCE_ENABLE_TRACE

#include <sstream>

#include "demangle.h"
#include "utf.h"

#define _TRACE_STRINGIFY2(m) #m
#define _TRACE_STRINGIFY(m) _TRACE_STRINGIFY2(m)
#define _TRACE_WIDE2(m) L##m
#define _TRACE_WIDE(m) _TRACE_WIDE2(m)

#ifdef _MSC_VER

#include <windows.h>
#define TRACEA(level, message)                        \
  try                                                 \
  {                                                   \
    std::ostringstream str;                           \
    str                                               \
        << __FILE__                                   \
        << "(" << _TRACE_STRINGIFY(__LINE__) << "): " \
        << "[" << _TRACE_STRINGIFY(level) << "] "     \
        << message                                    \
        << std::endl;                                 \
    OutputDebugStringA(str.str().c_str());            \
  }                                                   \
  catch (...)                                         \
  {                                                   \
  }
#define TRACEW(level, message)                                       \
  try                                                                \
  {                                                                  \
    std::wostringstream str;                                         \
    str                                                              \
        << _TRACE_WIDE(__FILE__)                                     \
        << L"(" << _TRACE_WIDE(_TRACE_STRINGIFY(__LINE__)) << L"): " \
        << L"[" << _TRACE_WIDE(_TRACE_STRINGIFY(level)) << L"] "     \
        << message                                                   \
        << std::endl;                                                \
    OutputDebugStringW(str.str().c_str());                           \
  }                                                                  \
  catch (...)                                                        \
  {                                                                  \
  }

#else // _MSC_VER

#include <iostream>
#define TRACEA(level, message)                        \
  try                                                 \
  {                                                   \
    std::cerr                                         \
        << __FILE__                                   \
        << "(" << _TRACE_STRINGIFY(__LINE__) << "): " \
        << "[" << _TRACE_STRINGIFY(level) << "] "     \
        << message                                    \
        << std::endl;                                 \
  }                                                   \
  catch (...)                                         \
  {                                                   \
  }
#define TRACEW(level, message)                                       \
  try                                                                \
  {                                                                  \
    std::wostringstream str;                                         \
    str                                                              \
        << _TRACE_WIDE(__FILE__)                                     \
        << L"(" << _TRACE_WIDE(_TRACE_STRINGIFY(__LINE__)) << L"): " \
        << L"[" << _TRACE_WIDE(_TRACE_STRINGIFY(level)) << L"] "     \
        << message;                                                  \
    std::cerr << utf16_to_utf8(str.str()) << std::endl;              \
  }                                                                  \
  catch (...)                                                        \
  {                                                                  \
  }

#endif

#define TRACEE(ex)                          \
  TRACEA(error,                             \
         "exception of type "               \
             << demangle(typeid(ex).name()) \
             << " with reason: "            \
             << get_reason_from_exception(ex))

#else // AUDIENCE_ENABLE_TRACE

#define TRACEA(level, message)
#define TRACEW(level, message)
#define TRACEE(ex)

#endif // AUDIENCE_ENABLE_TRACE

#ifdef WIN32
#include <windows.h>
inline std::wstring GetLastErrorString()
{
  wchar_t buf[256] = {0};
  FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 buf, (sizeof(buf) / sizeof(wchar_t)), NULL);
  return std::wstring(buf);
}
#endif

inline std::string get_reason_from_exception(const std::exception &e)
{
  return e.what();
}

template <typename ExceptionT>
inline std::string get_reason_from_exception(const ExceptionT &e)
{
  return "unknown reason";
}
