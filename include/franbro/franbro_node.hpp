#pragma once

#include <memory>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <rclcpp/rclcpp.hpp>

#include "franbro/config.hpp"
#include "franbro/transport/protocol.hpp"
#include "franbro/transport/transport_server.hpp"
#include "franbro/transport/transport_client.hpp"
#include "franbro/topic_bridge.hpp"
#include "franbro/service_bridge.hpp"
#include "franbro/action_bridge.hpp"

namespace franbro
{

/// The central ROS 2 node.
///
/// Lifecycle:
///   1. Load Config from the "config_file" ROS parameter.
///   2. Start boost::asio io_context on a background thread pool.
///   3. Start TransportServer to accept incoming FranBro connections.
///   4. Start one TransportClient per configured remote.
///   5. On each new connection (incoming or outgoing):
///      a. Send HANDSHAKE with local capability manifest.
///      b. On receiving the peer's HANDSHAKE, create TopicBridge /
///         ServiceBridge / ActionBridge for that connection.
///   6. Dispatch frames arriving on each connection to the appropriate bridge.
class FranBroNode : public rclcpp::Node
{
public:
  explicit FranBroNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions{});
  ~FranBroNode() override;

private:
  // ── Initialisation helpers ────────────────────────────────────────────────

  void init();
  void setup_server();
  void setup_clients();

  // ── Connection management ─────────────────────────────────────────────────

  /// Called for both server-accepted and client-initiated connections.
  /// \param on_close  Optional callback invoked when the connection drops.
  ///                  Used by TransportClient connections to trigger reconnect.
  void on_new_connection(Connection::Ptr conn, std::function<void()> on_close = nullptr);
  void send_handshake(Connection::Ptr conn);

  // ── Frame routing ─────────────────────────────────────────────────────────

  struct ConnectionContext
  {
    Connection::Ptr               connection;
    std::shared_ptr<TopicBridge>   topic_bridge;
    std::shared_ptr<ServiceBridge> service_bridge;
    std::shared_ptr<ActionBridge>  action_bridge;
  };

  void on_frame(Connection::Ptr conn, Frame frame);
  void on_error(Connection::Ptr conn, std::error_code ec);

  void handle_handshake(ConnectionContext & ctx, const Frame & frame);

  // ── State ─────────────────────────────────────────────────────────────────

  Config config_;

  boost::asio::io_context              io_ctx_;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
  std::vector<std::thread>             io_threads_;

  std::unique_ptr<TransportServer>     server_;
  std::vector<std::unique_ptr<TransportClient>> clients_;

  std::mutex                                        ctx_mu_;
  std::unordered_map<Connection *, ConnectionContext> contexts_;

  rclcpp::TimerBase::SharedPtr keepalive_timer_;
};

}  // namespace franbro
