#pragma once

#include <mutex>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#ifdef WIN32
#include <spdlog/sinks/msvc_sink.h>
#else
#include <spdlog/sinks/syslog_sink.h>
#endif

inline void setup_logger(std::string name)
{
  static std::mutex mutex;
  static bool initialized = false;
  std::lock_guard<std::mutex> lock(mutex);
  if (!initialized)
  {
    // prepare sinks
    auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>(spdlog::color_mode::automatic);
#ifdef AUDIENCE_LOG_LEVEL_CONSOLE
    console_sink->set_level(spdlog::level::level_enum(AUDIENCE_LOG_LEVEL_CONSOLE));
#endif
    dist_sink->add_sink(console_sink);
#ifdef WIN32
    dist_sink->add_sink(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#else
    dist_sink->add_sink(std::make_shared<spdlog::sinks::syslog_sink_mt>(name, 0, LOG_USER, false));
#endif
    // prepare logger
    // spdlog::init_thread_pool(8192, 1);
    // auto logger = std::make_shared<spdlog::async_logger>(name, dist_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    auto logger = std::make_shared<spdlog::logger>(name, dist_sink);
    logger->flush_on(spdlog::level::err);
    // set default logger and log level
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::trace);
    // done
    SPDLOG_DEBUG("audience logger '{}' initialized", name);
    initialized = true;
  }
  else
  {
    SPDLOG_DEBUG("audience logger '{}' NOT initialized, because another logger is already installed", name);
  }
}
