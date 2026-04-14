#include "franbro/transport/transport_server.hpp"

#include <stdexcept>

namespace franbro
{

TransportServer::TransportServer(
  boost::asio::io_context & io_ctx,
  uint16_t port,
  NewConnectionHandler handler)
: acceptor_(io_ctx,
    boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
, new_connection_handler_(std::move(handler))
{
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  do_accept();
}

TransportServer::~TransportServer()
{
  stop();
}

void TransportServer::stop()
{
  boost::system::error_code ec;
  acceptor_.close(ec);
}

void TransportServer::do_accept()
{
  acceptor_.async_accept(
    [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
      if (ec) {
        // acceptor was closed or an error occurred – stop accepting
        return;
      }
      // Disable Nagle for low-latency framing
      socket.set_option(boost::asio::ip::tcp::no_delay(true));
      auto conn = std::make_shared<Connection>(std::move(socket));
      new_connection_handler_(conn);
      do_accept();
    });
}

}  // namespace franbro
