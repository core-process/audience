#pragma once

#include <chrono>
#include <ratio>
#include <functional>
#include <string>

typedef std::chrono::system_clock::time_point ping_time_point;
typedef std::chrono::duration<double, std::milli> ping_duration;

extern void ping_start(
    std::function<void(ping_time_point, ping_duration)> on_echo_reply,
    std::function<void(std::string)> on_error);
extern void ping_stop();
