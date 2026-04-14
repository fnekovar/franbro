#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include "franbro/config.hpp"
#include "franbro/transport/protocol.hpp"

namespace franbro
{

/// Manages action bridging for one connection.
///
/// For each action in the remote manifest, a local rclcpp_action server is
/// created that proxies goal / cancel / result / feedback over the wire.
///
/// Note: rclcpp_action does not currently expose a type-erased
/// "GenericActionServer" API the way it does for topics/services.  We therefore
/// use the internal client APIs of rclcpp_action with serialised messages, which
/// requires the concrete action type at compile time.
///
/// For a fully generic implementation we instead model the action server using
/// the lower-level rclcpp service/topic primitives that the action stack is
/// built on (goal, cancel, result services; feedback topic), mirroring the
/// ROS 2 action protocol directly.
class ActionBridge
{
public:
  ActionBridge(
    rclcpp::Node::SharedPtr node,
    Connection::Ptr         connection,
    const std::vector<ActionEntry> & remote_actions);

  ~ActionBridge() = default;

  /// Called by the frame dispatcher for ACTION_FEEDBACK frames.
  void on_action_feedback(const Frame & frame);

  /// Called by the frame dispatcher for ACTION_RESULT frames.
  void on_action_result(const Frame & frame);

private:
  struct GoalHandle
  {
    uint32_t                                     call_id;
    rclcpp::GenericService::SharedPtr            result_service_proxy;
    rclcpp::GenericPublisher::SharedPtr          feedback_pub;
    std::vector<uint8_t>                         result_payload;
    bool                                         result_ready{false};
    std::mutex                                   mu;
    std::condition_variable                      cv;
  };

  uint32_t next_call_id();

  rclcpp::Node::SharedPtr node_;
  Connection::Ptr         connection_;

  // Servers / publishers / services backing each proxied action
  // (goal service, cancel service, result service, feedback publisher)
  struct ActionProxy
  {
    std::string                       action_name;
    std::string                       action_type;
    rclcpp::GenericService::SharedPtr goal_service;
    rclcpp::GenericService::SharedPtr cancel_service;
    rclcpp::GenericService::SharedPtr result_service;
    rclcpp::GenericPublisher::SharedPtr feedback_publisher;
  };

  std::vector<ActionProxy> proxies_;

  std::mutex                                              goals_mu_;
  std::unordered_map<uint32_t, std::shared_ptr<GoalHandle>> active_goals_;

  std::atomic<uint32_t> call_id_counter_{0};
};

}  // namespace franbro
