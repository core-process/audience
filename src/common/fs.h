#pragma once

#if WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif
#include <string>

#include "trace.h"
#include "utf.h"

static std::wstring normalize_path(const std::wstring& path)
{
#if WIN32
    constexpr auto normalized_path_length = 4096;
    wchar_t normalized_path[normalized_path_length]{};
    auto res = GetFullPathNameW(path.c_str(), normalized_path_length, normalized_path, nullptr);
    if (res == 0 || res > normalized_path_length)
    {
      TRACEW(error, "could not normalize path: " << path);
      throw std::invalid_argument("could not normalize path");
    }
    return std::wstring(normalized_path);
#else
    std::string path_utf8 = utf16_to_utf8(path);
    char normalized_path[PATH_MAX + 1]{};
    if (realpath(path_utf8.c_str(), normalized_path) == nullptr)
    {
      TRACEA(error, "could not normalize path: " << path_utf8);
      throw std::invalid_argument("could not normalize path");
    }
    return utf8_to_utf16(normalized_path);
#endif
}
