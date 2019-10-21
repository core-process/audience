#pragma once

#include <string>

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>

inline std::string demangle(const char *name)
{
  int status = -1;
  std::unique_ptr<char, void (*)(void *)> res{
      abi::__cxa_demangle(name, NULL, NULL, &status),
      std::free};
  return (status == 0) ? res.get() : name;
}

#else

inline std::string demangle(const char *name)
{
  return name;
}

#endif
