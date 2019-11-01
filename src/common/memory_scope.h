#pragma once

#include "scope_guard.h"

class memory_scope
{
private:
  scope_guard scope_always;

public:
  memory_scope() : scope_always(scope_guard::execution::always) {}
  ~memory_scope() {}

  template <typename T>
  T *alloc()
  {
    auto m = new T{};
    scope_always += [m]() { delete m; };
    return m;
  }

  template <typename T>
  T *alloc_array(size_t size)
  {
    auto m = new T[size]{};
    scope_always += [m]() { delete[] m; };
    return m;
  }

  template <typename T>
  typename T::value_type *alloc_string(const T &s)
  {
    auto ms = alloc_array<typename T::value_type>(s.length() + 1);
    std::copy(s.begin(), s.end(), ms);
    ms[s.length()] = {};
    return ms;
  }
};
