#pragma once

#include <boost/asio/io_context.hpp>
#include <thread>
#include <set>
#include <memory>
#include <mutex>

class websocket_session;

struct WebserverContextData;
typedef std::shared_ptr<WebserverContextData> WebserverContext;
typedef std::weak_ptr<WebserverContextData> WebserverContextWeak;

struct WebserverContextData
{
  // The io_context is required for all I/O
  boost::asio::io_context ioc;
  std::vector<std::thread> threads;

  std::set<std::weak_ptr<websocket_session>, std::owner_less<std::weak_ptr<websocket_session>>> websocket_sessions;
  std::mutex websocket_sessions_mutex;

  void add_ws_session(std::shared_ptr<websocket_session> &ws)
  {
    std::lock_guard<std::mutex> lock(websocket_sessions_mutex);
    websocket_sessions.insert(ws);
  }

  std::set<std::shared_ptr<websocket_session>> get_ws_sessions()
  {
    std::lock_guard<std::mutex> lock(websocket_sessions_mutex);
    std::set<std::shared_ptr<websocket_session>> result;
    for (auto i = websocket_sessions.begin(); i != websocket_sessions.end(); ++i)
    {
      auto is = i->lock();
      if (is)
      {
        result.insert(is);
      }
      else
      {
        i = websocket_sessions.erase(i);
        --i;
      }
    }
    return result;
  }

  std::function<void(WebserverContext, std::string)> on_message_handler;

  WebserverContextData(int concurrency_hint)
      : ioc(concurrency_hint)
  {
    threads.reserve(concurrency_hint);
  }
};
