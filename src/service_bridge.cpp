#include "franbro/service_bridge.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>

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

    auto server = node_->create_generic_service(
      svc.name,
      svc.type,
      [this, name = svc.name](
        std::shared_ptr<rmw_request_id_t> /*req_id*/,
        std::shared_ptr<rclcpp::SerializedMessage> request,
        std::shared_ptr<rclcpp::SerializedMessage> response)
      {
        uint32_t call_id = next_call_id();

        // Register pending call
        auto pending = std::make_shared<PendingCall>();
        {
          std::lock_guard<std::mutex> lk(calls_mu_);
          pending_calls_[call_id] = pending;
        }

        // Build SERVICE_REQUEST frame: [call_id(4)][name_len(4)][name][cdr_bytes]
        Frame frame;
        frame.type = FrameType::SERVICE_REQUEST;
        encode_uint32(frame.payload, call_id);
        encode_string(frame.payload, name);

        const auto & rcl_req = request->get_rcl_serialized_message();
        frame.payload.insert(
          frame.payload.end(),
          rcl_req.buffer,
          rcl_req.buffer + rcl_req.buffer_length);

        connection_->send(std::move(frame));

        // Block until response arrives (with 30 s timeout)
        {
          std::unique_lock<std::mutex> lk(pending->mu);
          pending->cv.wait_for(
            lk,
            std::chrono::seconds(30),
            [&pending]{ return pending->ready; });
        }

        {
          std::lock_guard<std::mutex> lk(calls_mu_);
          pending_calls_.erase(call_id);
        }

        if (!pending->ready) {
          // Timeout: return an empty response
          return;
        }

        // Copy serialised response bytes into the response message
        const auto & resp_bytes = pending->response_payload;
        auto & rcl_resp = response->get_rcl_serialized_message();

        // Resize buffer if needed (use rcutils allocator)
        if (rcl_resp.buffer_capacity < resp_bytes.size()) {
          uint8_t * new_buf = static_cast<uint8_t *>(
            rcl_resp.allocator.reallocate(
              rcl_resp.buffer, resp_bytes.size(), rcl_resp.allocator.state));
          if (!new_buf) {
            RCLCPP_ERROR(node_->get_logger(), "ServiceBridge: failed to allocate response buffer");
            return;
          }
          rcl_resp.buffer = new_buf;
          rcl_resp.buffer_capacity = resp_bytes.size();
        }
        std::memcpy(rcl_resp.buffer, resp_bytes.data(), resp_bytes.size());
        rcl_resp.buffer_length = resp_bytes.size();
      },
      rclcpp::ServicesQoS());

    servers_.push_back(server);
  }
}

uint32_t ServiceBridge::next_call_id()
{
  return call_id_counter_.fetch_add(1, std::memory_order_relaxed);
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
