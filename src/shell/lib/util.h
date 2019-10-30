#pragma once

#include <vector>

#include "../../common/trace.h"
#include "../../common/utf.h"
#include "whereami.h"

static inline std::wstring dir_of_exe()
{
  auto length = audience_getExecutablePath(nullptr, 0, nullptr);
  if (length == -1)
  {
    TRACEA(error, "could not retrieve path of executable");
    throw std::runtime_error("could not retrieve path of executable");
  }
  std::vector<char> buffer(length + 1, 0);
  int dir_length = 0;
  if (audience_getExecutablePath(&buffer[0], length, &dir_length) == -1)
  {
    TRACEA(error, "could not retrieve path of executable");
    throw std::runtime_error("could not retrieve path of executable");
  }
  buffer[dir_length] = 0;
  std::string path(&buffer[0]);
  TRACEA(info, "executable directory found: " << path);
  return utf8_to_utf16(path);
}
