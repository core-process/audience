#pragma once

#include <exception>
#include "trace.h"

#define SAFE_FN(fn, ...) SafeFn<decltype(&fn), &fn>::Catch<__VA_ARGS__>::exec

template <typename FnT, FnT fn>
struct SafeFn;

template <typename RetT, typename... ArgTn, RetT (*fn)(ArgTn...)>
struct SafeFn<RetT (*)(ArgTn...), fn>
{
  template <RetT return_value, typename... ExceptionTn>
  struct Catch
  {
    static inline RetT exec(ArgTn... argn)
    {
      try
      {
        return _Catch<return_value, ExceptionTn...>::exec(argn...);
      }
      catch (const std::exception &e)
      {
        TRACEE(e);
        return return_value;
      }
      catch (...)
      {
        TRACEA(error, "unknown exception");
        return return_value;
      }
    }
  };

  template <RetT return_value, typename... ExceptionTn>
  struct _Catch;

  template <RetT return_value, typename ExceptionT1, typename... ExceptionTn>
  struct _Catch<return_value, ExceptionT1, ExceptionTn...>
  {
    static inline RetT exec(ArgTn... argn)
    {
      try
      {
        return _Catch<return_value, ExceptionTn...>::exec(argn...);
      }
      catch (const ExceptionT1 &e)
      {
        TRACEE(e);
        return return_value;
      }
    }
  };

  template <RetT return_value>
  struct _Catch<return_value>
  {
    static inline RetT exec(ArgTn... argn)
    {
      return fn(argn...);
    }
  };
};

template <typename... ArgTn, void (*fn)(ArgTn...)>
struct SafeFn<void (*)(ArgTn...), fn>
{
  template <typename... ExceptionTn>
  struct Catch
  {
    static inline void exec(ArgTn... argn)
    {
      try
      {
        _Catch<true, ExceptionTn...>::exec(argn...);
        return;
      }
      catch (const std::exception &e)
      {
        TRACEE(e);
        return;
      }
      catch (...)
      {
        TRACEA(error, "unknown exception");
        return;
      }
    }
  };

  template <bool Dummy, typename... ExceptionTn>
  struct _Catch;

  template <typename ExceptionT1, typename... ExceptionTn>
  struct _Catch<true, ExceptionT1, ExceptionTn...>
  {
    static inline void exec(ArgTn... argn)
    {
      try
      {
        _Catch<true, ExceptionTn...>::exec(argn...);
        return;
      }
      catch (const ExceptionT1 &e)
      {
        TRACEE(e);
        return;
      }
    }
  };

  template <bool Dummy>
  struct _Catch<Dummy>
  {
    static inline void exec(ArgTn... argn)
    {
      fn(argn...);
    }
  };
};
