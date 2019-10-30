#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <queue>

#include "../../../common/trace.h"
#include "../../../common/utf.h"
#include "context.h"

// Echoes back all received WebSocket messages
class websocket_session : public std::enable_shared_from_this<websocket_session>
{
  WebserverContextWeak context_;
  boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
  boost::beast::flat_buffer read_buffer_;

  std::queue<std::wstring> write_queue_;
  bool pending_write_;
  std::string pending_write_data_;
  std::mutex write_mutex_;

public:
  // Take ownership of the socket
  explicit websocket_session(
      WebserverContextWeak context,
      boost::asio::ip::tcp::socket &&socket)
      : context_(context), ws_(std::move(socket)), pending_write_(false)
  {
    TRACEA(info, "websocket session created");
  }

  ~websocket_session()
  {
    TRACEA(info, "websocket session closed");
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

  void
  queue_write(const std::wstring& body)
  {
    {
      std::lock_guard<std::mutex> lock(write_mutex_);
      write_queue_.push(body);
    }
    do_write();
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
        read_buffer_,
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

    // pass to handler
    auto ctx = context_.lock();
    if (ctx && ctx->on_message_handler)
    {
      ctx->on_message_handler(ctx, utf8_to_utf16(boost::beast::buffers_to_string(read_buffer_.data())));
    }

    // Clear the buffer
    read_buffer_.consume(read_buffer_.size());

    // Do another read
    do_read();
  }

  void
  do_write()
  {
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (pending_write_ == false && write_queue_.size() > 0)
    {
      TRACEA(debug, "writing message to websocket");

      // pop and buffer data
      pending_write_ = true;
      pending_write_data_ = utf16_to_utf8(write_queue_.front());
      write_queue_.pop();

      // trigger write
      ws_.text(true);
      ws_.async_write(
          boost::asio::buffer(pending_write_data_),
          boost::beast::bind_front_handler(
              &websocket_session::on_write,
              shared_from_this()));
    }
  }

  void
  on_write(
      boost::beast::error_code ec,
      std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);
    TRACEA(debug, "write operation completed");

    // unset pending write flag
    {
      std::lock_guard<std::mutex> lock(write_mutex_);
      pending_write_ = false;
    }

    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }

    // write next
    do_write();
  }
};
