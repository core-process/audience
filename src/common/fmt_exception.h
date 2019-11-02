#pragma once

#include <spdlog/fmt/fmt.h>
//#include <boost/stacktrace.hpp>

FMT_BEGIN_NAMESPACE

// Extend fmt lib to support std::exception
// see http://fmtlib.net/latest/api.html#formatting-user-defined-types
//
// Logs the exception message (`what()`) followed by the stack trace
// (in DEBUG build).
//
// @usage:
//   try { ... }
//   catch (const std::exception& ex) {
//     logger->error("exception caught: {}", ex);
//   }

template <>
struct formatter<std::exception>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const std::exception &ex, FormatContext &ctx)
  {
    auto it = format_to(ctx.out(), "{}", ex.what());
// #ifndef NDEBUG
//     boost::stacktrace::stacktrace st{12, static_cast<std::size_t>(-1)};
//     if (st)
//     {
//       for (unsigned i = 0; i < st.size(); ++i)
//         it = format_to(it, "\n\t{:2d}# {}", i, boost::stacktrace::to_string(st[i]));
//     }
// #endif
    return it;
  }
};

FMT_END_NAMESPACE
