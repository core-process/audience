#pragma once

#include <boost/beast/http.hpp>
#include <memory>

#include "../../../common/trace.h"
#include "websocket_session.impl.h"
#include "handle_request.impl.h"

// Handles an HTTP server connection
class http_session : public std::enable_shared_from_this<http_session>
{
  // This queue is used for HTTP pipelining.
  class queue
  {
    enum
    {
      // Maximum number of responses we will queue
      limit = 8
    };

    // The type-erased, saved work item
    struct work
    {
      virtual ~work() = default;
      virtual void operator()() = 0;
    };

    http_session &self_;
    std::vector<std::unique_ptr<work>> items_;

  public:
    explicit queue(http_session &self)
        : self_(self)
    {
      static_assert(limit > 0, "queue limit must be positive");
      items_.reserve(limit);
    }

    // Returns `true` if we have reached the queue limit
    bool
    is_full() const
    {
      return items_.size() >= limit;
    }

    // Called when a message finishes sending
    // Returns `true` if the caller should initiate a read
    bool
    on_write()
    {
      BOOST_ASSERT(!items_.empty());
      auto const was_full = is_full();
      items_.erase(items_.begin());
      if (!items_.empty())
        (*items_.front())();
      return was_full;
    }

    // Called by the HTTP handler to send a response.
    template <bool isRequest, class Body, class Fields>
    void
    operator()(boost::beast::http::message<isRequest, Body, Fields> &&msg)
    {
      // This holds a work item
      struct work_impl : work
      {
        http_session &self_;
        boost::beast::http::message<isRequest, Body, Fields> msg_;

        work_impl(
            http_session &self,
            boost::beast::http::message<isRequest, Body, Fields> &&msg)
            : self_(self), msg_(std::move(msg))
        {
        }

        void
        operator()()
        {
          boost::beast::http::async_write(
              self_.stream_,
              msg_,
              boost::beast::bind_front_handler(
                  &http_session::on_write,
                  self_.shared_from_this(),
                  msg_.need_eof()));
        }
      };

      // Allocate and store the work
      items_.push_back(
          boost::make_unique<work_impl>(self_, std::move(msg)));

      // If there was no previous work, start this one
      if (items_.size() == 1)
        (*items_.front())();
    }
  };

  WebserverContextWeak context_;
  boost::beast::tcp_stream stream_;
  boost::beast::flat_buffer buffer_;
  std::shared_ptr<std::string const> doc_root_;
  queue queue_;

  // The parser is stored in an optional container so we can
  // construct it from scratch it at the beginning of each new message.
  boost::optional<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;

public:
  // Take ownership of the socket
  http_session(
      WebserverContextWeak context,
      boost::asio::ip::tcp::socket &&socket,
      std::shared_ptr<std::string const> const &doc_root)
      : context_(context), stream_(std::move(socket)), doc_root_(doc_root), queue_(*this)
  {
    TRACEA(info, "http session created");
  }

  ~http_session()
  {
    TRACEA(info, "http session closed");
  }

  // Start the session
  void
  run()
  {
    do_read();
  }

private:
  void
  do_read()
  {
    // Construct a new parser for each message
    parser_.emplace();

    // Apply a reasonable limit to the allowed size
    // of the body in bytes to prevent abuse.
    //parser_->body_limit(10000);

    // Set the timeout.
    //stream_.expires_after(std::chrono::seconds(30));

    // Read a request using the parser-oriented interface
    boost::beast::http::async_read(
        stream_,
        buffer_,
        *parser_,
        boost::beast::bind_front_handler(
            &http_session::on_read,
            shared_from_this()));
  }

  void
  on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    // This means they closed the connection
    if (ec == boost::beast::http::error::end_of_stream)
      return do_close();

    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }

    // See if it is a WebSocket Upgrade
    if (boost::beast::websocket::is_upgrade(parser_->get()))
    {
      // Create a websocket session, transferring ownership
      // of both the socket and the HTTP request.
      auto ws = std::make_shared<websocket_session>(
          context_,
          stream_.release_socket());
      ws->do_accept(parser_->release());

      auto ctx = context_.lock();
      if (ctx)
      {
        ctx->add_ws_session(ws);
      }

      return;
    }

    // Send the response
    handle_request(*doc_root_, parser_->release(), queue_);

    // If we aren't at the queue limit, try to pipeline another request
    if (!queue_.is_full())
      do_read();
  }

  void
  on_write(bool close, boost::beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if (ec)
    {
      TRACEA(error, ec.message());
      return;
    }

    if (close)
    {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      return do_close();
    }

    // Inform the queue that a write completed
    if (queue_.on_write())
    {
      // Read another request
      do_read();
    }
  }

  void
  do_close()
  {
    // Send a TCP shutdown
    boost::beast::error_code ec;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
  }
};
