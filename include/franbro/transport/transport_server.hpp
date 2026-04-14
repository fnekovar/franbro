#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

#include <boost/asio.hpp>

#include "franbro/transport/protocol.hpp"

namespace franbro
{

/// Listens on a TCP port and accepts incoming FranBro connections.
/// For each accepted connection a Connection object is created and the
/// provided new_connection_handler is invoked.
class TransportServer
{
public:
  using NewConnectionHandler = std::function<void(Connection::Ptr)>;

  TransportServer(
    boost::asio::io_context & io_ctx,
    uint16_t port,
    NewConnectionHandler handler);

  ~TransportServer();

  /// Stop accepting new connections (already-accepted ones are unaffected).
  void stop();

private:
  void do_accept();

  boost::asio::ip::tcp::acceptor acceptor_;
  NewConnectionHandler           new_connection_handler_;
};

}  // namespace franbro
