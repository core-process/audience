#pragma once

#include <functional>
#include <deque>

#include "trace.h"

class scope_guard
{
public:
  enum execution
  {
    always,
    no_exception,
    exception
  };

  scope_guard(scope_guard &&) = default;
  explicit scope_guard(execution policy = always) : policy(policy) {}

  template <class Callable>
  scope_guard(Callable &&func, execution policy = always) : policy(policy)
  {
    this->operator+=<Callable>(std::forward<Callable>(func));
  }

  template <class Callable>
  scope_guard &operator+=(Callable &&func) try
  {
    handlers.emplace_front(std::forward<Callable>(func));
    return *this;
  }
  catch (...)
  {
    if (policy != no_exception)
      func();
    throw;
  }

  ~scope_guard()
  {
    if (policy == always || (std::uncaught_exception() == (policy == exception)))
    {
      for (auto &f : handlers)
        try
        {
          f(); // should not throw
        }
        catch (const std::exception &e)
        {
          TRACEE(e);
        }
        catch (...)
        {
          TRACEA(error, "unknown exception");
        }
    }
  }

  void dismiss() noexcept
  {
    handlers.clear();
  }

private:
  scope_guard(const scope_guard &) = delete;
  void operator=(const scope_guard &) = delete;

  std::deque<std::function<void()>> handlers;
  execution policy = always;
};
