#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>

#include "../../common/trace.h"

// Echoes back all received WebSocket messages
class websocket_session : public std::enable_shared_from_this<websocket_session>
{
  boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
  boost::beast::flat_buffer buffer_;

public:
  // Take ownership of the socket
  explicit websocket_session(boost::asio::ip::tcp::socket &&socket)
      : ws_(std::move(socket))
  {
  }

  // Start the asynchronous accept operation
  template <class Body, class Allocator>
  void
  do_accept(boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>> req)
  {
    // Set suggested timeout settings for the websocket
    ws_.set_option(
        boost::beast::websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::response_type &res) {
          res.set(boost::beast::http::field::server,
                  std::string(BOOST_BEAST_VERSION_STRING) +
                      " advanced-server");
        }));

    // Accept the websocket handshake
    ws_.async_accept(
        req,
        boost::beast::bind_front_handler(
            &websocket_session::on_accept,
            shared_from_this()));
  }

private:
  void
  on_accept(boost::beast::error_code ec)
  {
    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }

    // Read a message
    do_read();
  }

  void
  do_read()
  {
    // Read a message into our buffer
    ws_.async_read(
        buffer_,
        boost::beast::bind_front_handler(
            &websocket_session::on_read,
            shared_from_this()));
  }

  void
  on_read(
      boost::beast::error_code ec,
      std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    // This indicates that the websocket_session was closed
    if (ec == boost::beast::websocket::error::closed)
      return;

    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }

    // Echo the message
    ws_.text(ws_.got_text());
    ws_.async_write(
        buffer_.data(),
        boost::beast::bind_front_handler(
            &websocket_session::on_write,
            shared_from_this()));
  }

  void
  on_write(
      boost::beast::error_code ec,
      std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Do another read
    do_read();
  }
};
