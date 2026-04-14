#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include "franbro/config.hpp"
#include "franbro/transport/protocol.hpp"

namespace franbro
{

/// Manages topic bridging for one connection.
///
/// Outbound  (local → remote): For each locally-owned topic, a
///   GenericSubscription listens and forwards TOPIC_MSG frames.
///
/// Inbound   (remote → local): For each topic in the remote manifest, a
///   GenericPublisher republishes arriving TOPIC_MSG frames locally.
class TopicBridge
{
public:
  TopicBridge(
    rclcpp::Node::SharedPtr node,
    Connection::Ptr         connection,
    const std::vector<TopicEntry> & local_topics,
    const std::vector<TopicEntry> & remote_topics);

  ~TopicBridge() = default;

  /// Called by the frame dispatcher with every TOPIC_MSG frame.
  void on_topic_frame(const Frame & frame);

private:
  rclcpp::Node::SharedPtr node_;
  Connection::Ptr         connection_;

  // name → subscription (for locally-owned topics)
  std::unordered_map<std::string, rclcpp::GenericSubscription::SharedPtr> subscriptions_;

  // name → publisher (for remotely-owned topics)
  std::unordered_map<std::string, rclcpp::GenericPublisher::SharedPtr>    publishers_;
};

}  // namespace franbro
