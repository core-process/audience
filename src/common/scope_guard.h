#pragma once

#include <functional>
#include <deque>
#include <spdlog/spdlog.h>

#include "fmt_exception.h"

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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    if (policy == always || (std::uncaught_exception() == (policy == exception)))
#pragma GCC diagnostic pop
    {
      for (auto &f : handlers)
        try
        {
          f(); // should not throw
        }
        catch (const std::exception &e)
        {
          SPDLOG_ERROR("{}", e);
        }
        catch (...)
        {
          SPDLOG_ERROR("unknown exception");
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
