#ifdef WIN32
#include <windows.h>
#elif __APPLE__
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <sys/syslimits.h>
#include <stdlib.h>
#endif

#include <vector>
#include <string>

#include "lib.h"
#include "inner.h"
#include "trace.h"

audience_inner_init_t audience_inner_init = nullptr;
audience_inner_window_create_t audience_inner_window_create = nullptr;
audience_inner_window_destroy_t audience_inner_window_destroy = nullptr;
audience_inner_loop_t audience_inner_loop = nullptr;

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

  std::vector<std::string> dylibs{
#ifdef WIN32
      "audience_windows_edge.dll",
      "audience_windows_ie11.dll",
#elif __APPLE__
      "libaudience_macos_webkit.dylib",
#else
      "libaudience_linux_webkit.dylib",
#endif
  };

  for (auto dylib : dylibs)
  {
#ifdef WIN32
    auto dlh = LoadLibraryA(dylib.c_str());
#else
    char exe_path[PATH_MAX + 1] = {0};
    uint32_t exe_path_len = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &exe_path_len) != 0)
    {
      TRACEA(error, "could not find path of executable");
      return true;
    }

    char exe_dir_path[PATH_MAX + 1] = {0};
    if (realpath(exe_path, exe_dir_path) == nullptr)
    {
      TRACEA(error, "could not find real path of executable");
    }

    char *last_slash = nullptr;
    for (last_slash = &exe_dir_path[strlen(exe_dir_path) - 1]; last_slash != exe_dir_path && *last_slash != '/'; last_slash--)
    {
    }
    last_slash++;
    *last_slash = 0;

    auto dlh = dlopen((exe_dir_path + dylib).c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif

    if (dlh != nullptr)
    {
#ifdef WIN32
#define LookupFunction GetProcAddress
#else
#define LookupFunction dlsym
#endif

      audience_inner_init = (audience_inner_init_t)LookupFunction(dlh, "audience_inner_init");
      audience_inner_window_create = (audience_inner_window_create_t)LookupFunction(dlh, "audience_inner_window_create");
      audience_inner_window_destroy = (audience_inner_window_destroy_t)LookupFunction(dlh, "audience_inner_window_destroy");
      audience_inner_loop = (audience_inner_loop_t)LookupFunction(dlh, "audience_inner_loop");

      if (audience_is_initialized() && audience_inner_init())
      {
        TRACEA(info, "library " << dylib << " loaded successfully");
        return true;
      }
      else
      {
        TRACEA(info, "could not initialize library " << dylib);
      }

      audience_inner_init = nullptr;
      audience_inner_window_create = nullptr;
      audience_inner_window_destroy = nullptr;
      audience_inner_loop = nullptr;

#ifdef WIN32
      FreeLibrary(dlh);
#else
      dlclose(dlh);
#endif
    }
    else
    {
      TRACEA(info, "could not load library " << dylib);
#ifdef WIN32
      TRACEW(info, GetLastErrorString().c_str());
#endif
    }
  }

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
