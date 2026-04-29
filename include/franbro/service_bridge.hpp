#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/generic_client.hpp>
#include <rclcpp/serialized_message.hpp>

#include "franbro/config.hpp"
#include "franbro/transport/protocol.hpp"

namespace franbro
{

/// Manages service bridging for one connection.
///
/// For each service in the remote manifest, a local GenericClient proxy is
/// created.  When a local client calls a service through the network:
///   1. The service request is serialized
///   2. A SERVICE_REQUEST frame is sent to the remote node
///   3. We block (with timeout) waiting for SERVICE_RESPONSE
///   4. The response is deserialized and returned to the caller
///
/// The remote node receives SERVICE_REQUEST frames and forwards them to its
/// local service server, then sends back SERVICE_RESPONSE frames through
/// on_service_response().
class ServiceBridge
{
public:
  ServiceBridge(
    rclcpp::Node::SharedPtr node,
    Connection::Ptr         connection,
    const std::vector<ServiceEntry> & remote_services);

  ~ServiceBridge() = default;

  /// Called by the frame dispatcher with every SERVICE_RESPONSE frame.
  /// Payload format: [call_id(4)][cdr_response_bytes...]
  void on_service_response(const Frame & frame);

  /// Send a service request and wait for response
  /// Returns the serialized response, or empty vector on timeout/error
  std::vector<uint8_t> call_service(
    const std::string & service_name,
    const std::vector<uint8_t> & request_payload,
    std::chrono::milliseconds timeout = std::chrono::seconds(5));

private:
  struct PendingCall
  {
    std::vector<uint8_t>    response_payload;
    bool                    ready{false};
    std::condition_variable cv;
    std::mutex              mu;
  };

  uint32_t next_call_id();

  rclcpp::Node::SharedPtr node_;
  Connection::Ptr         connection_;

  // service name → type string (kept for client creation)
  std::unordered_map<std::string, std::string> service_types_;

  // Generic service clients created for remote services
  std::vector<rclcpp::GenericClient::SharedPtr> clients_;

  // call_id → pending call state
  std::mutex                                             calls_mu_;
  std::unordered_map<uint32_t, std::shared_ptr<PendingCall>> pending_calls_;

  std::atomic<uint32_t> call_id_counter_{0};
};

}  // namespace franbro
