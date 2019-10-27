#pragma once

#include <vector>

#include "../../common/trace.h"
#include "whereami.h"

#if defined(WIN32)
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

static inline std::string dir_of_exe()
{
  auto length = audience_getExecutablePath(nullptr, 0, nullptr);
  if (length == -1)
  {
    TRACEA(warning, "could not retrieve path of executable");
    return "";
  }
  std::vector<char> buffer(length + 1, 0);
  int dir_length = 0;
  if (audience_getExecutablePath(&buffer[0], length, &dir_length) == -1)
  {
    TRACEA(warning, "could not retrieve path of executable");
    return "";
  }
  buffer[dir_length] = 0;
  std::string path(&buffer[0]);
  TRACEA(info, "executable directory found: " << path);
  return path;
}
