#include <thread>
#include <iostream>
#include <set>

#include "listener.impl.h"
#include "process.h"
#include "context.h"

WebserverContext webserver_start(std::string address, unsigned short &port, std::string doc_root, int threads, std::function<void(WebserverContext, const std::wstring&)> on_message_handler)
{
  auto context = std::make_shared<WebserverContextData>(threads);
  context->on_message_handler = on_message_handler;

  // Create and launch a listening port
  auto l = std::make_shared<listener>(
      context,
      context->ioc,
      boost::asio::ip::tcp::endpoint{boost::asio::ip::make_address(address), port},
      std::make_shared<std::string>(doc_root));

  port = l->local_endpoint().port();
  TRACEA(info, "webserver started at " << address << ":" << port);

  l->run();

  // Run the I/O service on the requested number of threads
  for (auto i = 0; i < threads; ++i)
  {
    context->threads.emplace_back(
        [context] {
          context->ioc.run();
        });
  }

  return context;
}

void webserver_post_message(WebserverContext context, const std::wstring& message)
{
  auto sessions = context->get_ws_sessions();
  TRACEA(debug, "found " << sessions.size() << " valid sessions");

  for (auto &session : sessions)
  {
    session->queue_write(message);
  }
}

void webserver_stop(WebserverContext context)
{
  context->ioc.stop();

  // Block until all the threads exit
  for (auto &thread : context->threads)
  {
    thread.join();
  }

  context.reset();

  TRACEA(info, "webserver stopped");
}
