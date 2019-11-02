#pragma once

#include <spdlog/fmt/fmt.h>

FMT_BEGIN_NAMESPACE

template <>
struct formatter<std::exception>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const std::exception &ex, FormatContext &ctx)
  {
    return format_to(ctx.out(), "{}", ex.what());
  }
};

FMT_END_NAMESPACE
