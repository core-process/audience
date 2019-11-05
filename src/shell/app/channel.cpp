#include <uvw.hpp>
#include <vector>
#include <memory>
#include <thread>
#include <numeric>
#include <atomic>
#include <set>
#include <regex>
#include <mutex>
#include <spdlog/spdlog.h>
#include <json.hpp>

#include <audience.h>

#include "../../common/fmt_exception.h"
#include "../../common/memory_scope.h"
#include "../../common/utf.h"
#include "channel.h"

using json = nlohmann::json;

static std::unique_ptr<std::thread> loop_thread; // can only be used from controlling thread

static std::mutex mutex;                                        // protects the following variables (except before loop thread started):
std::shared_ptr<uvw::Loop> loop;                                // can be used from controlling and from loop thread
static std::vector<uvw::DataEvent> outgoing_events;             // can be used from controlling and from loop thread
static std::shared_ptr<uvw::AsyncHandle> async_handle_emit;     // can be used from controlling and from loop thread
static std::shared_ptr<uvw::AsyncHandle> async_handle_activate; // can be used from controlling and from loop thread
static std::shared_ptr<uvw::AsyncHandle> async_handle_shutdown; // can be used from controlling and from loop thread

static bool is_activated;                             // can only be used from loop thread
static bool is_connected;                             // can only be used from loop thread
static std::shared_ptr<uvw::PipeHandle> peer;         // can only be used from loop thread (except before loop thread started)
static std::vector<uvw::DataEvent> incoming_chunks;   // can only be used from loop thread
static std::vector<uvw::DataEvent> incoming_commands; // can only be used from loop thread

static void _async_emit_handler(const uvw::AsyncEvent &, uvw::AsyncHandle &);     // called on loop thread only
static void _async_activate_handler(const uvw::AsyncEvent &, uvw::AsyncHandle &); // called on loop thread only
static void _async_shutdown_handler(const uvw::AsyncEvent &, uvw::AsyncHandle &); // called on loop thread only

static void _peer_execute_command(const uvw::DataEvent &); // called on loop thread only

static void _peer_on_data(uvw::DataEvent &, uvw::PipeHandle &);               // called on loop thread only
static void _peer_on_connect(const uvw::ConnectEvent &, uvw::PipeHandle &);   // called on loop thread only
static void _peer_on_shutdown(const uvw::ShutdownEvent &, uvw::PipeHandle &); // called on loop thread only
static void _peer_on_end(const uvw::EndEvent &, uvw::PipeHandle &);           // called on loop thread only
static void _peer_on_close(const uvw::CloseEvent &, uvw::PipeHandle &);       // called on loop thread only
static void _peer_on_error(const uvw::ErrorEvent &, uvw::PipeHandle &);       // called on loop thread only

static void _push_queues(); // called on loop thread only

void channel_create(const std::string &path) // called on controlling thread
{
  SPDLOG_DEBUG("create channel: {}", path);

#if defined(WIN32)
  if (!std::regex_match(path, std::regex(R"(^\\\\\.\\pipe\\[\w]+$)", std::regex::ECMAScript | std::regex::icase)))
  {
    throw std::invalid_argument("channel path should be formatted \\\\.\\pipe\\<NAME>");
  }
#endif

  loop = uvw::Loop::create();

  async_handle_emit = loop->resource<uvw::AsyncHandle>();
  async_handle_activate = loop->resource<uvw::AsyncHandle>();
  async_handle_shutdown = loop->resource<uvw::AsyncHandle>();

  async_handle_emit->on<uvw::AsyncEvent>(_async_emit_handler);
  async_handle_activate->on<uvw::AsyncEvent>(_async_activate_handler);
  async_handle_shutdown->on<uvw::AsyncEvent>(_async_shutdown_handler);

  peer = loop->resource<uvw::PipeHandle>();
  peer->on<uvw::DataEvent>(_peer_on_data);
  peer->once<uvw::ConnectEvent>(_peer_on_connect);
  peer->once<uvw::ShutdownEvent>(_peer_on_shutdown);
  peer->once<uvw::EndEvent>(_peer_on_end);
  peer->once<uvw::CloseEvent>(_peer_on_close);
  peer->once<uvw::ErrorEvent>(_peer_on_error);

  peer->connect(path);

  // run the loop
  loop_thread = std::make_unique<std::thread>([]() {
    if (loop)
    {
      loop->run();
    }
  });
}

void channel_activate() // called on controlling thread
{
  SPDLOG_DEBUG("activating channel");

  {
    std::lock_guard<std::mutex> lock(mutex);
    if (async_handle_activate)
    {
      async_handle_activate->send();
    }
  }
}

void channel_shutdown() // called on controlling thread
{
  SPDLOG_DEBUG("shutting down channel");

  {
    std::lock_guard<std::mutex> lock(mutex);
    if (async_handle_shutdown)
    {
      SPDLOG_DEBUG("... sending async shutdown");
      async_handle_shutdown->send();
    }
  }

  if (loop_thread)
  {
    SPDLOG_DEBUG("... waiting for loop thread");
    loop_thread->join();
    loop_thread.reset();
  }

  if (loop)
  {
    SPDLOG_DEBUG("... take over last iterations of loop");
    loop->run();
    loop.reset();
  }

  SPDLOG_DEBUG("channel shutdown completed");
}

static void _push_queues()
{
  // execute commands
  if (is_activated)
  {
    SPDLOG_DEBUG("executing commands (count={})", incoming_commands.size());
    for (auto &command : incoming_commands)
    {
      _peer_execute_command(command);
    }
    incoming_commands.clear();
  }
  else
  {
    SPDLOG_DEBUG("delay execution of commands (count={})", incoming_commands.size());
  }

  // send events
  if (is_connected)
  {
    std::lock_guard<std::mutex> lock(mutex);
    SPDLOG_DEBUG("sending events (count={})", outgoing_events.size());
    for (auto &event : outgoing_events)
    {
      if (peer)
      {
        peer->write(std::move(event.data), event.length);
      }
    }
    outgoing_events.clear();
  }
  else
  {
    SPDLOG_DEBUG("delay sending of events (count={})", outgoing_events.size());
  }
}

static void _channel_emit(std::string name, json data) // called on controlling or loop thread (applies to all emit function)
{
  // serialize event json
  auto event = json{{"name", name}, {"data", data}}.dump() + "\n";

  // prepare raw event data
  uvw::DataEvent event_raw(std::make_unique<char[]>(event.length()), event.length());
  std::copy(event.begin(), event.end(), event_raw.data.get());

  // queue and trigger async emit on loop thread
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (async_handle_emit)
    {
      outgoing_events.push_back(std::move(event_raw));
      async_handle_emit->send();
    }
  }
}

void channel_emit_window_message(AudienceWindowHandle handle, const wchar_t *message)
{
  _channel_emit("window_message", json{{"handle", handle}, {"message", utf16_to_utf8(message)}});
}

void channel_emit_window_close_intent(AudienceWindowHandle handle)
{
  _channel_emit("window_close_intent", json{{"handle", handle}});
}

void channel_emit_window_close(AudienceWindowHandle handle, bool is_last_window)
{
  _channel_emit("window_close", json{{"handle", handle}, {"is_last_window", is_last_window}});
}

void channel_emit_app_quit()
{
  _channel_emit("app_quit", json{});
}

static void _channel_emit_command_succeeded(std::string id, json result)
{
  _channel_emit("command_succeeded", json{{"id", id}, {"result", result}});
}

static void _channel_emit_command_succeeded(std::string id)
{
  _channel_emit("command_succeeded", json{{"id", id}});
}

static void _channel_emit_command_failed(std::string id, const std::string &reason)
{
  _channel_emit("command_failed", json{{"id", id}, {"reason", reason}});
}

static void _peer_execute_command(const uvw::DataEvent &command_raw)
{
  try
  {
    auto command_json = json::parse(command_raw.data.get(), command_raw.data.get() + command_raw.length);
    auto id_json = command_json.value("id", json());
    auto id = id_json.is_string() ? id_json.get<std::string>() : id_json.dump();

    try
    {
      auto func = command_json.at("func").get<std::string>();
      auto args = command_json.value("args", json::object());

      SPDLOG_DEBUG("trying to execute command {}", func);

      if (func == "screen_list")
      {
        auto result = audience_screen_list();

        json result_json;
        result_json["focused"] = result.focused;
        result_json["primary"] = result.primary;
        result_json["screens"] = json::array();
        for (size_t i = 0; i < result.count; ++i)
        {
          json screen;
          screen["frame"] = {
              {"x", result.screens[i].frame.origin.x},
              {"y", result.screens[i].frame.origin.y},
              {"width", result.screens[i].frame.size.width},
              {"height", result.screens[i].frame.size.height}};
          screen["workspace"] = {
              {"x", result.screens[i].workspace.origin.x},
              {"y", result.screens[i].workspace.origin.y},
              {"width", result.screens[i].workspace.size.width},
              {"height", result.screens[i].workspace.size.height}};
          result_json["screens"].push_back(screen);
        }

        _channel_emit_command_succeeded(id, std::move(result_json));
      }
      else if (func == "window_list")
      {
        auto result = audience_window_list();

        json result_json;
        result_json["focused"] = result.focused;
        result_json["windows"] = json::array();
        for (size_t i = 0; i < result.count; ++i)
        {
          json window;
          window["handle"] = result.windows[i].handle;
          window["frame"] = {
              {"x", result.windows[i].frame.origin.x},
              {"y", result.windows[i].frame.origin.y},
              {"width", result.windows[i].frame.size.width},
              {"height", result.windows[i].frame.size.height}};
          window["workspace"] = {
              {"width", result.windows[i].workspace.width},
              {"height", result.windows[i].workspace.height}};
          result_json["windows"].push_back(window);
        }

        _channel_emit_command_succeeded(id, std::move(result_json));
      }
      else if (func == "window_create")
      {
        memory_scope mem;

        // construct window details
        AudienceWindowDetails wd{};

        if (args.count("dir") == 0 && args.count("url") == 0)
        {
          throw std::invalid_argument("either dir or url argument required");
        }

        if (args.count("dir") > 0)
        {
          wd.webapp_type = AUDIENCE_WEBAPP_TYPE_DIRECTORY;
          wd.webapp_location = mem.alloc_string(utf8_to_utf16(args["dir"].get<std::string>()));
        }

        if (args.count("url") > 0)
        {
          wd.webapp_type = AUDIENCE_WEBAPP_TYPE_URL;
          wd.webapp_location = mem.alloc_string(utf8_to_utf16(args["url"].get<std::string>()));
        }

        if (args.count("title") > 0)
        {
          wd.loading_title = mem.alloc_string(utf8_to_utf16(args["title"].get<std::string>()));
        }

        {
          auto screens = audience_screen_list();
          auto workspace = screens.screens[screens.focused].workspace;

          if (args.count("size") > 0)
          {
            auto size = args["size"].get<std::vector<double>>();
            if (size.size() != 2)
            {
              throw std::invalid_argument("size needs to be formatted as width,height");
            }
            wd.position.size.width = size[0];
            wd.position.size.height = size[1];
          }
          else
          {
            wd.position.size.width = workspace.size.width * 0.6;
            wd.position.size.height = workspace.size.height * 0.6;
          }

          if (args.count("pos") > 0)
          {
            auto pos = args["pos"].get<std::vector<double>>();
            if (pos.size() != 2)
            {
              throw std::invalid_argument("position needs to be formatted as x,y");
            }
            wd.position.origin.x = std::max(pos[0], workspace.origin.x);
            wd.position.origin.y = std::max(pos[1], workspace.origin.y);
          }
          else
          {
            wd.position.size.width = std::min(wd.position.size.width, workspace.size.width);
            wd.position.size.height = std::min(wd.position.size.height, workspace.size.height);
            wd.position.origin.x = workspace.origin.x + (workspace.size.width - wd.position.size.width) * 0.5;
            wd.position.origin.y = workspace.origin.y + (workspace.size.height - wd.position.size.height) * 0.5;
          }
        }

        if (args.count("decorated") > 0)
        {
          wd.styles.not_decorated = !args["decorated"].get<bool>();
        }

        if (args.count("resizable") > 0)
        {
          wd.styles.not_resizable = !args["resizable"].get<bool>();
        }

        if (args.count("top") > 0)
        {
          wd.styles.always_on_top = args["top"].get<bool>();
        }

        if (args.count("dev") > 0)
        {
          wd.dev_mode = args["dev"].get<bool>();
        }

        // construct window handler
        AudienceWindowEventHandler weh{};
        weh.on_message.handler = [](AudienceWindowHandle handle, void *context, const wchar_t *message) {
          SPDLOG_DEBUG("event window::message");
          channel_emit_window_message(handle, message);
        };
        weh.on_close_intent.handler = [](AudienceWindowHandle handle, void *context) {
          SPDLOG_DEBUG("event window::close_intent");
          channel_emit_window_close_intent(handle);
        };
        weh.on_close.handler = [](AudienceWindowHandle handle, void *context, bool is_last_window) {
          SPDLOG_DEBUG("event window::close");
          channel_emit_window_close(handle, is_last_window);
        };

        // execute command
        auto result = audience_window_create(&wd, &weh);
        if (result == AudienceWindowHandle{})
        {
          throw std::runtime_error("could not create window");
        }

        _channel_emit_command_succeeded(id, json(result));
      }
      else if (func == "window_update_position")
      {
        auto handle = args.at("handle").get<AudienceWindowHandle>();

        AudienceRect position;
        position.origin.x = args.at("x").get<double>();
        position.origin.y = args.at("y").get<double>();
        position.size.width = args.at("width").get<double>();
        position.size.height = args.at("height").get<double>();

        audience_window_update_position(handle, position);
        _channel_emit_command_succeeded(id);
      }
      else if (func == "window_post_message")
      {
        auto handle = args.at("handle").get<AudienceWindowHandle>();
        auto message = utf8_to_utf16(args.at("message").get<std::string>());

        audience_window_post_message(handle, message.c_str());
        _channel_emit_command_succeeded(id);
      }
      else if (func == "window_destroy")
      {
        auto handle = args.at("handle").get<AudienceWindowHandle>();

        audience_window_destroy(handle);
        _channel_emit_command_succeeded(id);
      }
      else if (func == "quit")
      {
        audience_quit();
        _channel_emit_command_succeeded(id);
      }
      else
      {
        throw std::invalid_argument("unknown command function");
      }
    }
    catch (const std::exception &e)
    {
      SPDLOG_ERROR("{}", e);
      _channel_emit_command_failed(id, e.what());
    }
    catch (...)
    {
      SPDLOG_ERROR("unknown error");
      _channel_emit_command_failed(id, "unknown error");
    }
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

static void _async_emit_handler(const uvw::AsyncEvent &, uvw::AsyncHandle &)
{
  _push_queues();
}

static void _async_activate_handler(const uvw::AsyncEvent &, uvw::AsyncHandle &)
{
  is_activated = true;
  _push_queues();

  if (!peer || peer->closing())
  {
    SPDLOG_WARN("peer already closed on activation, quitting app...");
    audience_quit();
  }
}

static void _async_shutdown_handler(const uvw::AsyncEvent &, uvw::AsyncHandle &)
{
  is_activated = false;
  is_connected = false;

  if (peer)
  {
    SPDLOG_TRACE("initiate shutdown of peer");
    peer->shutdown();
  }

  {
    std::lock_guard<std::mutex> lock(mutex);

    if (async_handle_emit)
    {
      SPDLOG_TRACE("closing async handle: emit");
      async_handle_emit->close();
      async_handle_emit.reset();
    }

    if (async_handle_activate)
    {
      SPDLOG_TRACE("closing async handle: activate");
      async_handle_activate->close();
      async_handle_activate.reset();
    }

    if (async_handle_shutdown)
    {
      SPDLOG_TRACE("closing async handle: shutdown");
      async_handle_shutdown->close();
      async_handle_shutdown.reset();
    }

    if (loop)
    {
      SPDLOG_TRACE("stopping loop on loop thread");
      loop->stop();
    }
  }
}

static void _peer_on_data(uvw::DataEvent &cur_chunk, uvw::PipeHandle &)
{
  // scan for next newline
  for (size_t i = 0; i < cur_chunk.length;)
  {
    if (cur_chunk.data[i] == '\n')
    {
      // calculate length of continuous memory
      auto continuous_length = std::accumulate(incoming_chunks.begin(), incoming_chunks.end(), (size_t)0, [](auto a, auto &b) { return a + b.length; });
      continuous_length += i;

      SPDLOG_DEBUG("command of length {} detected ({} previous chunks, part of current chunk {}, total current chunk {})", continuous_length, incoming_chunks.size(), i, cur_chunk.length);

      // in case there is really some data ...
      if (continuous_length > 0)
      {
        // ... allocate continuous memory
        std::unique_ptr<char[]> continuous = std::make_unique<char[]>(continuous_length);
        char *continuous_next = continuous.get();

        // ... copy previously received incoming chunks into continuous memory
        for (auto &prev_chunk : incoming_chunks)
        {
          std::copy(prev_chunk.data.get(), prev_chunk.data.get() + prev_chunk.length, continuous_next);
          continuous_next += prev_chunk.length;
        }

        incoming_chunks.clear();

        // ... copy newly received part into continuous memory
        if (i > 0)
        {
          std::copy(cur_chunk.data.get(), cur_chunk.data.get() + i, continuous_next);
        }

        // ... place into incoming commands
        SPDLOG_TRACE("command: {}", std::string(continuous.get(), continuous_length));
        incoming_commands.push_back(uvw::DataEvent{std::move(continuous), continuous_length});
      }

      // shorten current chunk
      std::copy(cur_chunk.data.get() + i + 1, cur_chunk.data.get() + cur_chunk.length, cur_chunk.data.get());
      cur_chunk.length -= i + 1;
      i = 0;
    }
    else
    {
      i += 1;
    }
  }

  // save current chunk in case there is still some data in it
  if (cur_chunk.length > 0)
  {
    incoming_chunks.push_back(std::move(cur_chunk));
  }

  // push queues
  _push_queues();
}

static void _peer_on_connect(const uvw::ConnectEvent &, uvw::PipeHandle &)
{
  SPDLOG_DEBUG("peer stream connected");
  is_connected = true;
  if (peer)
  {
    peer->read();
  }
  _push_queues();
}

static void _peer_on_shutdown(const uvw::ShutdownEvent &, uvw::PipeHandle &)
{
  SPDLOG_DEBUG("peer stream shutdown");
  is_connected = false;
  if (peer)
  {
    peer->close();
    peer.reset();
  }
}

static void _peer_on_end(const uvw::EndEvent &, uvw::PipeHandle &)
{
  SPDLOG_DEBUG("peer stream ended");
  is_connected = false;
  if (peer)
  {
    peer->shutdown();
  }
}

static void _peer_on_close(const uvw::CloseEvent &, uvw::PipeHandle &)
{
  SPDLOG_DEBUG("peer pipe closed");
  is_connected = false;
  if (is_activated)
  {
    audience_quit();
  }
}

static void _peer_on_error(const uvw::ErrorEvent &event, uvw::PipeHandle &)
{
  SPDLOG_ERROR("peer error: {}", event.what());
  is_connected = false;
  if (peer)
  {
    peer->close();
    peer.reset();
  }
  if (is_activated)
  {
    audience_quit();
  }
}
