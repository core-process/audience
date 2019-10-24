#include <thread>
#include <iostream>

#include "listener.impl.h"
#include "process.h"

struct WebserverHandleData
{
  // The io_context is required for all I/O
  boost::asio::io_context ioc;
  std::vector<std::thread> threads;

  WebserverHandleData(int concurrency_hint)
      : ioc(concurrency_hint)
  {
    threads.reserve(concurrency_hint);
  }
};

WebserverHandle webserver_start(std::string address, unsigned short &port, std::string doc_root, int threads)
{
  auto handle = std::make_shared<WebserverHandleData>(threads);

  // Create and launch a listening port
  auto l = std::make_shared<listener>(
      handle->ioc,
      boost::asio::ip::tcp::endpoint{boost::asio::ip::make_address(address), port},
      std::make_shared<std::string>(doc_root));

  port = l->local_endpoint().port();
  TRACEA(info, "webserver started at " << address << ":" << port);

  l->run();

  // Run the I/O service on the requested number of threads
  for (auto i = 0; i < threads; ++i)
  {
    handle->threads.emplace_back(
        [handle] {
          handle->ioc.run();
        });
  }

  return handle;
}

void webserver_stop(WebserverHandle &handle)
{
  handle->ioc.stop();

  // Block until all the threads exit
  for (auto &thread : handle->threads)
  {
    thread.join();
  }

  handle.reset();

  TRACEA(info, "webserver stopped");
}
