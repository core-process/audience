#include <uvw.hpp>
#include <vector>
#include <memory>
#include <thread>
#include <numeric>
#include <atomic>
#include <set>
#include <spdlog/spdlog.h>
#include <json.hpp>

#include <audience.h>

#include "../../common/fmt_exception.h"
#include "../../common/memory_scope.h"
#include "../../common/utf.h"
#include "channel.h"

using json = nlohmann::json;

static std::unique_ptr<std::thread> loop_thread;         // can only be used from controlling thread
static std::shared_ptr<uvw::AsyncHandle> async_shutdown; // can be used from controlling and loop thread
static std::shared_ptr<uvw::AsyncHandle> async_activate; // can be used from controlling and loop thread

static bool activated = false;                             // can only be used from loop thread
static std::shared_ptr<uvw::PipeHandle> server;            // can only be used from loop thread
static std::set<std::shared_ptr<uvw::PipeHandle>> clients; // can only be used from loop thread

std::vector<uvw::DataEvent> incoming_commands; // can only be used from loop thread
std::vector<uvw::DataEvent> outgoing_events;   // can only be used from loop thread

void _client_execute_command(const uvw::DataEvent &);

void _server_on_listen(const uvw::ListenEvent &, uvw::PipeHandle &);
void _server_on_error(const uvw::ErrorEvent &, uvw::PipeHandle &);

void _client_on_data(uvw::DataEvent &, uvw::PipeHandle &);
void _client_on_shutdown(const uvw::ShutdownEvent &, uvw::PipeHandle &);
void _client_on_end(const uvw::EndEvent &, uvw::PipeHandle &);
void _client_on_close(const uvw::CloseEvent &, uvw::PipeHandle &);
void _client_on_error(const uvw::ErrorEvent &, uvw::PipeHandle &);

struct client_data_t
{
  std::vector<uvw::DataEvent> incoming_chunks;
  client_data_t() : incoming_chunks{} {}
};

void channel_prepare(const std::string &path)
{
  SPDLOG_DEBUG("preparing channel");

  std::shared_ptr<uvw::Loop> loop = uvw::Loop::create();

  auto clean_stale_pipe = loop->resource<uvw::FsReq>();
  clean_stale_pipe->unlinkSync(path);
  clean_stale_pipe.reset();

  async_shutdown = loop->resource<uvw::AsyncHandle>();
  async_shutdown->on<uvw::AsyncEvent>([loop](const uvw::AsyncEvent &, uvw::AsyncHandle &) {
    // shutdown active clients
    for (auto &c : clients)
    {
      c->shutdown();
    }
    clients.clear();
    // shutdown listening server
    if (server)
    {
      SPDLOG_DEBUG("closing server pipe");
      server->close();
      server.reset();
    }
    // reset state
    if (async_shutdown)
    {
      SPDLOG_DEBUG("closing async shutdown resource");
      async_shutdown->close();
      async_shutdown.reset();
    }
    if (async_activate)
    {
      SPDLOG_DEBUG("closing async activate resource");
      async_activate->close();
      async_activate.reset();
    }
    activated = false;
  });

  async_activate = loop->resource<uvw::AsyncHandle>();
  async_activate->on<uvw::AsyncEvent>([](const uvw::AsyncEvent &, uvw::AsyncHandle &) {
    // execute all buffered incoming commands
    for (auto &command : incoming_commands)
    {
      _client_execute_command(command);
    }
    incoming_commands.clear();
    // push all buffered outgoing events
    for (auto &event : outgoing_events)
    {
      for (auto &client : clients)
      {
        auto data_copy = std::make_unique<char[]>(event.length);
        std::copy(event.data.get(), event.data.get() + event.length, data_copy.get());
        client->write(std::move(data_copy), event.length);
      }
    }
    outgoing_events.clear();
    // stop buffering of commands and events
    activated = true;
  });

  server = loop->resource<uvw::PipeHandle>();
  server->on<uvw::ErrorEvent>(_server_on_error);
  server->on<uvw::ListenEvent>(_server_on_listen);
  server->bind(path);
  server->listen();

  loop_thread = std::make_unique<std::thread>([loop]() {
    loop->run();
  });
}

void channel_activate()
{
  SPDLOG_DEBUG("activating channel");

  if (async_activate)
  {
    async_activate->send();
  }
}

void channel_shutdown()
{
  SPDLOG_DEBUG("shutting down channel");

  if (async_shutdown)
  {
    async_shutdown->send();
  }

  if (loop_thread)
  {
    SPDLOG_DEBUG("... waiting for loop thread");
    loop_thread->join();
    loop_thread.reset();
  }

  SPDLOG_DEBUG("channel shutdown completed");
}

void channel_emit(std::string name, json data)
{
  auto event = json{{"name", name}, {"data", data}}.dump() + "\n";

  if (activated)
  {
    for (auto &client : clients)
    {
      uvw::DataEvent event_raw(std::make_unique<char[]>(event.length()), event.length());
      std::copy(event.begin(), event.end(), event_raw.data.get());

      client->write(std::move(event_raw.data), event_raw.length);
    }
  }
  else
  {
    uvw::DataEvent event_raw(std::make_unique<char[]>(event.length()), event.length());
    std::copy(event.begin(), event.end(), event_raw.data.get());

    outgoing_events.push_back(std::move(event_raw));
  }
}

void channel_emit_window_message(AudienceWindowHandle handle, const wchar_t *message)
{
  channel_emit("window_message", json{{"handle", handle}, {"message", utf16_to_utf8(message)}});
}

void channel_emit_window_close_intent(AudienceWindowHandle handle)
{
  channel_emit("window_close_intent", json{{"handle", handle}});
}

void channel_emit_window_close(AudienceWindowHandle handle, bool is_last_window)
{
  channel_emit("window_close", json{{"handle", handle}, {"is_last_window", is_last_window}});
}

void channel_emit_quit()
{
  channel_emit("quit", json{});
}

void channel_emit_command_succeeded(std::string id, json result)
{
  channel_emit("command_succeeded", json{{"id", id, "result", result}});
}

void channel_emit_command_succeeded(std::string id)
{
  channel_emit("command_succeeded", json{{"id", id}});
}

void channel_emit_command_failed(std::string id, const std::string &reason)
{
  channel_emit("command_failed", json{{"id", id}, {"reason", reason}});
}

void _client_execute_command(const uvw::DataEvent &command_raw)
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

        channel_emit_command_succeeded(id, std::move(result_json));
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

        channel_emit_command_succeeded(id, std::move(result_json));
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

        channel_emit_command_succeeded(id, json(result));
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
        channel_emit_command_succeeded(id);
      }
      else if (func == "window_post_message")
      {
        auto handle = args.at("handle").get<AudienceWindowHandle>();
        auto message = utf8_to_utf16(args.at("message").get<std::string>());

        audience_window_post_message(handle, message.c_str());
        channel_emit_command_succeeded(id);
      }
      else if (func == "window_destroy")
      {
        auto handle = args.at("handle").get<AudienceWindowHandle>();

        audience_window_destroy(handle);
        channel_emit_command_succeeded(id);
      }
      else if (func == "quit")
      {
        audience_quit();
        channel_emit_command_succeeded(id);
      }
      else
      {
        throw std::invalid_argument("unknown command function");
      }
    }
    catch (const std::exception &e)
    {
      SPDLOG_ERROR("{}", e);
      channel_emit_command_failed(id, e.what());
    }
    catch (...)
    {
      SPDLOG_ERROR("unknown error");
      channel_emit_command_failed(id, "unknown error");
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

void _server_on_listen(const uvw::ListenEvent &, uvw::PipeHandle &server)
{
  SPDLOG_DEBUG("accepting new client connection");

  std::shared_ptr<uvw::PipeHandle> client = server.loop().resource<uvw::PipeHandle>();

  client->data(std::make_shared<client_data_t>());

  client->on<uvw::ErrorEvent>(_client_on_error);
  client->on<uvw::DataEvent>(_client_on_data);
  client->on<uvw::ShutdownEvent>(_client_on_shutdown);
  client->on<uvw::EndEvent>(_client_on_end);
  client->on<uvw::CloseEvent>(_client_on_close);

  server.accept(*client);
  clients.insert(client);
  client->read();
}

void _server_on_error(const uvw::ErrorEvent &event, uvw::PipeHandle &server)
{
  SPDLOG_ERROR("{}", event.what());
  server.close();
}

void _client_on_data(uvw::DataEvent &cur_chunk, uvw::PipeHandle &client)
{
  auto &incoming_chunks = client.data<client_data_t>()->incoming_chunks;

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

  // execute all pending commands if activated
  if (activated)
  {
    for (auto &command : incoming_commands)
    {
      _client_execute_command(command);
    }
    incoming_commands.clear();
  }
}

void _client_on_shutdown(const uvw::ShutdownEvent &, uvw::PipeHandle &client)
{
  SPDLOG_DEBUG("client stream shutdown");
  clients.erase(client.shared_from_this());
  client.close();
}

void _client_on_end(const uvw::EndEvent &, uvw::PipeHandle &client)
{
  SPDLOG_DEBUG("client stream ended");
  clients.erase(client.shared_from_this());
  client.shutdown();
}

void _client_on_close(const uvw::CloseEvent &, uvw::PipeHandle &client)
{
  SPDLOG_DEBUG("client pipe closed");
  clients.erase(client.shared_from_this());
}

void _client_on_error(const uvw::ErrorEvent &event, uvw::PipeHandle &client)
{
  SPDLOG_ERROR("client error: {}", event.what());
  clients.erase(client.shared_from_this());
  client.close();
}
