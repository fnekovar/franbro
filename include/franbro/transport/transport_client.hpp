#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include <boost/asio.hpp>

#include "franbro/transport/protocol.hpp"

namespace franbro
{

/// Connects to a remote FranBro node and maintains the connection.
/// Reconnects with exponential backoff when the connection is lost.
class TransportClient
{
public:
  using ConnectedHandler    = std::function<void(Connection::Ptr)>;
  using DisconnectedHandler = std::function<void()>;

  TransportClient(
    boost::asio::io_context & io_ctx,
    std::string host,
    uint16_t    port,
    ConnectedHandler    on_connected,
    DisconnectedHandler on_disconnected);

  ~TransportClient();

  void stop();

private:
  void schedule_connect(std::chrono::milliseconds delay);
  void try_connect();

  boost::asio::io_context &              io_ctx_;
  boost::asio::ip::tcp::resolver         resolver_;
  boost::asio::steady_timer              retry_timer_;
  std::string                            host_;
  uint16_t                               port_;
  ConnectedHandler                       on_connected_;
  DisconnectedHandler                    on_disconnected_;
  std::atomic<bool>                      stopped_{false};
  std::chrono::milliseconds              backoff_{std::chrono::milliseconds(500)};
  static constexpr std::chrono::seconds  kMaxBackoff{30};
};

}  // namespace franbro
