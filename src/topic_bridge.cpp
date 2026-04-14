#include "franbro/topic_bridge.hpp"

#include <cstring>

#include <rclcpp/serialized_message.hpp>

namespace franbro
{

TopicBridge::TopicBridge(
  rclcpp::Node::SharedPtr         node,
  Connection::Ptr                 connection,
  const std::vector<TopicEntry> & local_topics,
  const std::vector<TopicEntry> & remote_topics)
: node_(node)
, connection_(connection)
{
  // ── Outbound: subscribe to locally-owned topics and forward over wire ──────
  for (const auto & topic : local_topics) {
    auto sub = node_->create_generic_subscription(
      topic.name,
      topic.type,
      rclcpp::QoS(10),
      [this, name = topic.name](std::shared_ptr<rclcpp::SerializedMessage> msg) {
        // Build TOPIC_MSG frame:  [name_len(4)][name][cdr_bytes]
        Frame frame;
        frame.type = FrameType::TOPIC_MSG;
        encode_string(frame.payload, name);

        const auto & rcl_msg = msg->get_rcl_serialized_message();
        frame.payload.insert(
          frame.payload.end(),
          rcl_msg.buffer,
          rcl_msg.buffer + rcl_msg.buffer_length);

        connection_->send(std::move(frame));
      });
    subscriptions_[topic.name] = sub;
  }

  // ── Inbound: create publishers for topics owned by the remote node ─────────
  for (const auto & topic : remote_topics) {
    auto pub = node_->create_generic_publisher(
      topic.name,
      topic.type,
      rclcpp::QoS(10));
    publishers_[topic.name] = pub;
  }
}

void TopicBridge::on_topic_frame(const Frame & frame)
{
  // Parse:  [name_len(4)][name][cdr_bytes]
  size_t offset = 0;
  std::string name = decode_string(frame.payload, offset);

  auto it = publishers_.find(name);
  if (it == publishers_.end()) {
    return;
  }

  const size_t cdr_len = frame.payload.size() - offset;
  rclcpp::SerializedMessage serialized_msg(cdr_len);
  auto & rcl_msg = serialized_msg.get_rcl_serialized_message();
  std::memcpy(rcl_msg.buffer, frame.payload.data() + offset, cdr_len);
  rcl_msg.buffer_length = cdr_len;

  it->second->publish(serialized_msg);
}

}  // namespace franbro
