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

#include "franbro/config.hpp"
#include "franbro/transport/protocol.hpp"

namespace franbro
{

/// Manages service bridging for one connection.
///
/// For each service in the remote manifest, a local GenericService server is
/// created.  When called, it serialises the request, assigns a call-id, sends
/// a SERVICE_REQUEST frame, and blocks (with a timeout) until the matching
/// SERVICE_RESPONSE frame arrives.
class ServiceBridge
{
public:
  ServiceBridge(
    rclcpp::Node::SharedPtr node,
    Connection::Ptr         connection,
    const std::vector<ServiceEntry> & remote_services);

  ~ServiceBridge() = default;

  /// Called by the frame dispatcher with every SERVICE_RESPONSE frame.
  void on_service_response(const Frame & frame);

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

  // service name → type string (kept for server creation)
  std::unordered_map<std::string, std::string> service_types_;

  // Generic service servers created for remote services
  std::vector<rclcpp::GenericService::SharedPtr> servers_;

  // call_id → pending call state
  std::mutex                                             calls_mu_;
  std::unordered_map<uint32_t, std::shared_ptr<PendingCall>> pending_calls_;

  std::atomic<uint32_t> call_id_counter_{0};
};

}  // namespace franbro
