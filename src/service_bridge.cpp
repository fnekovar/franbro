#include "franbro/service_bridge.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

#include <rclcpp/create_generic_client.hpp>
#include <rclcpp/serialized_message.hpp>

namespace franbro
{

ServiceBridge::ServiceBridge(
  rclcpp::Node::SharedPtr             node,
  Connection::Ptr                     connection,
  const std::vector<ServiceEntry> &   remote_services)
: node_(node)
, connection_(connection)
{
  for (const auto & svc : remote_services) {
    service_types_[svc.name] = svc.type;

    // Create a generic client to communicate with the remote service
    // This allows us to call the actual service on the remote host
    auto client = rclcpp::create_generic_client(
      node_,
      svc.name,
      svc.type,
      rclcpp::ServicesQoS());

    clients_.push_back(client);

    RCLCPP_DEBUG(node_->get_logger(),
      "ServiceBridge: Created client for remote service '%s' (type: %s)",
      svc.name.c_str(), svc.type.c_str());
  }
}

uint32_t ServiceBridge::next_call_id()
{
  return call_id_counter_.fetch_add(1, std::memory_order_relaxed);
}

std::vector<uint8_t> ServiceBridge::call_service(
  const std::string & service_name,
  const std::vector<uint8_t> & request_payload,
  std::chrono::milliseconds timeout)
{
  // Allocate pending call state
  uint32_t call_id = next_call_id();
  auto pending = std::make_shared<PendingCall>();

  {
    std::lock_guard<std::mutex> lk(calls_mu_);
    pending_calls_[call_id] = pending;
  }

  // Build SERVICE_REQUEST frame: [call_id(4)][service_name_len(4)][service_name][request_cdr...]
  Frame request_frame;
  request_frame.type = FrameType::SERVICE_REQUEST;
  encode_uint32(request_frame.payload, call_id);
  encode_string(request_frame.payload, service_name);
  request_frame.payload.insert(
    request_frame.payload.end(),
    request_payload.begin(),
    request_payload.end());

  // Send the request
  connection_->send(std::move(request_frame));

  // Wait for response with timeout
  {
    std::unique_lock<std::mutex> lk(pending->mu);
    if (!pending->cv.wait_for(lk, timeout, [&pending]() { return pending->ready; })) {
      // Timeout
      RCLCPP_WARN(node_->get_logger(),
        "Service call timeout for service=%s, call_id=%u",
        service_name.c_str(), call_id);
      std::lock_guard<std::mutex> calls_lk(calls_mu_);
      pending_calls_.erase(call_id);
      return {};
    }
  }

  // Extract and return response
  std::vector<uint8_t> response = pending->response_payload;

  {
    std::lock_guard<std::mutex> lk(calls_mu_);
    pending_calls_.erase(call_id);
  }

  return response;
}

void ServiceBridge::on_service_response(const Frame & frame)
{
  // Parse: [call_id(4)][cdr_bytes...]
  if (frame.payload.size() < 4) {
    return;
  }
  size_t offset = 0;
  uint32_t call_id = decode_uint32(frame.payload, offset);

  std::shared_ptr<PendingCall> pending;
  {
    std::lock_guard<std::mutex> lk(calls_mu_);
    auto it = pending_calls_.find(call_id);
    if (it == pending_calls_.end()) {
      return;
    }
    pending = it->second;
  }

  {
    std::lock_guard<std::mutex> lk(pending->mu);
    pending->response_payload.assign(
      frame.payload.begin() + static_cast<std::ptrdiff_t>(offset),
      frame.payload.end());
    pending->ready = true;
  }
  pending->cv.notify_one();
}

}  // namespace franbro
