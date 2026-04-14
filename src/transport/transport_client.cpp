#include "franbro/transport/transport_client.hpp"

#include <iostream>

namespace franbro
{

TransportClient::TransportClient(
  boost::asio::io_context & io_ctx,
  std::string host,
  uint16_t    port,
  ConnectedHandler    on_connected,
  DisconnectedHandler on_disconnected)
: io_ctx_(io_ctx)
, resolver_(io_ctx)
, retry_timer_(io_ctx)
, host_(std::move(host))
, port_(port)
, on_connected_(std::move(on_connected))
, on_disconnected_(std::move(on_disconnected))
{
  schedule_connect(std::chrono::milliseconds(0));
}

TransportClient::~TransportClient()
{
  stop();
}

void TransportClient::stop()
{
  stopped_ = true;
  boost::system::error_code ec;
  retry_timer_.cancel(ec);
}

void TransportClient::schedule_connect(std::chrono::milliseconds delay)
{
  if (stopped_) {
    return;
  }
  retry_timer_.expires_after(delay);
  retry_timer_.async_wait([this](boost::system::error_code ec) {
    if (ec || stopped_) {
      return;
    }
    try_connect();
  });
}

void TransportClient::try_connect()
{
  if (stopped_) {
    return;
  }

  resolver_.async_resolve(
    host_, std::to_string(port_),
    [this](boost::system::error_code ec,
           boost::asio::ip::tcp::resolver::results_type results)
    {
      if (ec || stopped_) {
        // exponential back-off
        backoff_ = std::min(
          std::chrono::duration_cast<std::chrono::milliseconds>(kMaxBackoff),
          backoff_ * 2);
        schedule_connect(backoff_);
        return;
      }

      auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_ctx_);
      boost::asio::async_connect(
        *socket, results,
        [this, socket](boost::system::error_code ec2,
                       const boost::asio::ip::tcp::endpoint &)
        {
          if (ec2 || stopped_) {
            backoff_ = std::min(
              std::chrono::duration_cast<std::chrono::milliseconds>(kMaxBackoff),
              backoff_ * 2);
            schedule_connect(backoff_);
            return;
          }

          socket->set_option(boost::asio::ip::tcp::no_delay(true));
          backoff_ = std::chrono::milliseconds(500);  // reset on success

          // on_connected_ is responsible for calling conn->start().
          // We capture a weak_ptr so that we can schedule reconnection via
          // on_disconnected_ if the caller reports the connection dropped.
          auto conn = std::make_shared<Connection>(std::move(*socket));
          on_connected_(conn);
        });
    });
}

}  // namespace franbro
