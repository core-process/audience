#pragma once

#include <boost/asio/strand.hpp>
#include <memory>

#include "../../../common/trace.h"
#include "http_session.impl.h"
#include "context.h"

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
  WebserverContextWeak context_;
  boost::asio::io_context &ioc_;
  boost::asio::ip::tcp::acceptor acceptor_;
  std::shared_ptr<std::string const> doc_root_;

public:
  listener(
      WebserverContextWeak context,
      boost::asio::io_context &ioc,
      boost::asio::ip::tcp::endpoint endpoint,
      std::shared_ptr<std::string const> const &doc_root)
      : context_(context), ioc_(ioc), acceptor_(boost::asio::make_strand(ioc)), doc_root_(doc_root)
  {
    boost::beast::error_code ec;

    // Open the acceptor
    acceptor_.open(endpoint.protocol(), ec);
    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }

    // Allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }

    // Bind to the server address
    acceptor_.bind(endpoint, ec);
    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }

    // Start listening for connections
    acceptor_.listen(
        boost::asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }
  }

  boost::asio::ip::tcp::endpoint local_endpoint()
  {
    return acceptor_.local_endpoint();
  }

  // Start accepting incoming connections
  void
  run()
  {
    do_accept();
  }

private:
  void
  do_accept()
  {
    // The new connection gets its own strand
    acceptor_.async_accept(
        boost::asio::make_strand(ioc_),
        boost::beast::bind_front_handler(
            &listener::on_accept,
            shared_from_this()));
  }

  void
  on_accept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket)
  {
    if (ec)
    {
      TRACEA(error, ec.message());
    }
    else
    {
      // Create the http session and run it
      std::make_shared<http_session>(
          context_,
          std::move(socket),
          doc_root_)
          ->run();
    }

    // Accept another connection
    do_accept();
  }
};
