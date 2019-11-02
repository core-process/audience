#pragma once

#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

inline void setup_logger(std::string name)
{
  static std::mutex mutex;
  static bool initialized = false;
  std::lock_guard<std::mutex> lock(mutex);
  if (!initialized)
  {
    auto logger = spdlog::stdout_color_mt(name);
    logger->flush_on(spdlog::level::err);
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::trace);
    SPDLOG_DEBUG("audience logger '{}' initialized", name);
    initialized = true;
  }
  else
  {
    SPDLOG_DEBUG("audience logger '{}' NOT initialized, because another logger is already installed", name);
  }
}
