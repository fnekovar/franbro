#include "franbro/action_bridge.hpp"

#include <chrono>
#include <cstring>

#include <rclcpp/serialized_message.hpp>

namespace franbro
{

// ── ROS 2 action protocol sub-topic/service suffixes ─────────────────────────
// The action stack creates:
//   <name>/_action/send_goal       (service, GoalRequestMessage)
//   <name>/_action/cancel_goal     (service, CancelGoal)
//   <name>/_action/get_result      (service, GetResultService)
//   <name>/_action/feedback        (topic,   FeedbackMessage)
//   <name>/_action/status          (topic,   GoalStatusArray)   [read-only here]

ActionBridge::ActionBridge(
  rclcpp::Node::SharedPtr          node,
  Connection::Ptr                  connection,
  const std::vector<ActionEntry> & remote_actions)
: node_(node)
, connection_(connection)
{
  for (const auto & action : remote_actions) {
    ActionProxy proxy;
    proxy.action_name = action.name;
    proxy.action_type = action.type;

    const std::string base   = action.name + "/_action";
    const std::string a_type = action.type;

    // ── Goal service proxy ────────────────────────────────────────────────────
    proxy.goal_service = node_->create_generic_service(
      base + "/send_goal",
      a_type + "_SendGoal",
      [this, action_name = action.name](
        std::shared_ptr<rmw_request_id_t> /*req_id*/,
        std::shared_ptr<rclcpp::SerializedMessage> request,
        std::shared_ptr<rclcpp::SerializedMessage> response)
      {
        uint32_t call_id = next_call_id();

        auto handle = std::make_shared<GoalHandle>();
        handle->call_id = call_id;
        {
          std::lock_guard<std::mutex> lk(goals_mu_);
          active_goals_[call_id] = handle;
        }

        // Frame: [call_id(4)][action_name_len(4)][action_name][cdr_goal_bytes]
        Frame frame;
        frame.type = FrameType::ACTION_GOAL;
        encode_uint32(frame.payload, call_id);
        encode_string(frame.payload, action_name);

        const auto & rcl = request->get_rcl_serialized_message();
        frame.payload.insert(frame.payload.end(), rcl.buffer, rcl.buffer + rcl.buffer_length);
        connection_->send(std::move(frame));

        // Block until result arrives
        {
          std::unique_lock<std::mutex> lk(handle->mu);
          handle->cv.wait_for(
            lk, std::chrono::seconds(60),
            [&handle] { return handle->result_ready; });
        }

        {
          std::lock_guard<std::mutex> lk(goals_mu_);
          active_goals_.erase(call_id);
        }

        if (!handle->result_ready) {
          return;
        }

        const auto & rb = handle->result_payload;
        auto & rcl_resp = response->get_rcl_serialized_message();
        if (rcl_resp.buffer_capacity < rb.size()) {
          uint8_t * new_buf = static_cast<uint8_t *>(
            rcl_resp.allocator.reallocate(
              rcl_resp.buffer, rb.size(), rcl_resp.allocator.state));
          if (!new_buf) { return; }
          rcl_resp.buffer = new_buf;
          rcl_resp.buffer_capacity = rb.size();
        }
        std::memcpy(rcl_resp.buffer, rb.data(), rb.size());
        rcl_resp.buffer_length = rb.size();
      },
      rclcpp::ServicesQoS());

    // ── Cancel service proxy ──────────────────────────────────────────────────
    proxy.cancel_service = node_->create_generic_service(
      base + "/cancel_goal",
      "action_msgs/srv/CancelGoal",
      [this](
        std::shared_ptr<rmw_request_id_t> /*req_id*/,
        std::shared_ptr<rclcpp::SerializedMessage> request,
        std::shared_ptr<rclcpp::SerializedMessage> /*response*/)
      {
        // Forward the cancel request as an ACTION_CANCEL frame.
        // We use call_id=0 to indicate "cancel all" for simplicity;
        // a more complete implementation would parse the goal-id from request.
        Frame frame;
        frame.type = FrameType::ACTION_CANCEL;
        encode_uint32(frame.payload, 0u);
        const auto & rcl = request->get_rcl_serialized_message();
        frame.payload.insert(frame.payload.end(), rcl.buffer, rcl.buffer + rcl.buffer_length);
        connection_->send(std::move(frame));
      },
      rclcpp::ServicesQoS());

    // ── Get-result service proxy ──────────────────────────────────────────────
    // Results are delivered via the goal-service callback above.
    // We still expose the service so the action client can find it.
    proxy.result_service = node_->create_generic_service(
      base + "/get_result",
      a_type + "_GetResult",
      [](
        std::shared_ptr<rmw_request_id_t>,
        std::shared_ptr<rclcpp::SerializedMessage>,
        std::shared_ptr<rclcpp::SerializedMessage>) {},
      rclcpp::ServicesQoS());

    // ── Feedback publisher ────────────────────────────────────────────────────
    proxy.feedback_publisher = node_->create_generic_publisher(
      base + "/feedback",
      a_type + "_FeedbackMessage",
      rclcpp::QoS(10));

    proxies_.push_back(std::move(proxy));
  }
}

uint32_t ActionBridge::next_call_id()
{
  return call_id_counter_.fetch_add(1, std::memory_order_relaxed);
}

void ActionBridge::on_action_feedback(const Frame & frame)
{
  // Parse: [call_id(4)][action_name_len(4)][action_name][cdr_feedback_bytes]
  if (frame.payload.size() < 8) {
    return;
  }
  size_t offset = 0;
  /*uint32_t call_id =*/ decode_uint32(frame.payload, offset);
  std::string action_name = decode_string(frame.payload, offset);

  // Find the feedback publisher for this action
  for (const auto & proxy : proxies_) {
    if (proxy.action_name == action_name) {
      const size_t cdr_len = frame.payload.size() - offset;
      rclcpp::SerializedMessage msg(cdr_len);
      auto & rcl = msg.get_rcl_serialized_message();
      std::memcpy(rcl.buffer, frame.payload.data() + offset, cdr_len);
      rcl.buffer_length = cdr_len;
      proxy.feedback_publisher->publish(msg);
      return;
    }
  }
}

void ActionBridge::on_action_result(const Frame & frame)
{
  // Parse: [call_id(4)][status(4)][cdr_result_bytes...]
  if (frame.payload.size() < 8) {
    return;
  }
  size_t offset = 0;
  uint32_t call_id = decode_uint32(frame.payload, offset);
  /*uint32_t status  =*/ decode_uint32(frame.payload, offset);

  std::shared_ptr<GoalHandle> handle;
  {
    std::lock_guard<std::mutex> lk(goals_mu_);
    auto it = active_goals_.find(call_id);
    if (it == active_goals_.end()) {
      return;
    }
    handle = it->second;
  }

  {
    std::lock_guard<std::mutex> lk(handle->mu);
    handle->result_payload.assign(
      frame.payload.begin() + static_cast<std::ptrdiff_t>(offset),
      frame.payload.end());
    handle->result_ready = true;
  }
  handle->cv.notify_one();
}

}  // namespace franbro
