#include "franbro/action_bridge.hpp"

#include <chrono>
#include <cstring>

#include <rclcpp/create_generic_client.hpp>
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

    // ── Goal service client proxy ────────────────────────────────────────────────────
    // This client forwards goal requests to the remote action server
    proxy.goal_client = rclcpp::create_generic_client(
      node_,
      base + "/send_goal",
      a_type + "_SendGoal",
      rclcpp::ServicesQoS());

    // ── Cancel service client proxy ──────────────────────────────────────────────────
    // This client handles cancellation requests
    proxy.cancel_client = rclcpp::create_generic_client(
      node_,
      base + "/cancel_goal",
      "action_msgs/srv/CancelGoal",
      rclcpp::ServicesQoS());

    // ── Get-result service client proxy ──────────────────────────────────────────────
    // This client retrieves the result of a completed action
    proxy.result_client = rclcpp::create_generic_client(
      node_,
      base + "/get_result",
      a_type + "_GetResult",
      rclcpp::ServicesQoS());

    // ── Feedback publisher ────────────────────────────────────────────────────
    // This publisher forwards feedback from the remote action to local clients
    proxy.feedback_publisher = node_->create_generic_publisher(
      base + "/feedback",
      a_type + "_FeedbackMessage",
      rclcpp::QoS(10));

    proxies_.push_back(std::move(proxy));

    RCLCPP_DEBUG(node_->get_logger(),
      "ActionBridge: Created proxy for remote action '%s' (type: %s)",
      action.name.c_str(), action.type.c_str());
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
