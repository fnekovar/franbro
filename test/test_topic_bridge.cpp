#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <memory>

#include "franbro/topic_bridge.hpp"
#include "franbro/transport/protocol.hpp"

namespace franbro
{

/// Mock Connection for testing
class MockConnectionForTopic : public std::enable_shared_from_this<MockConnectionForTopic>
{
public:
  using Ptr = std::shared_ptr<MockConnectionForTopic>;

  MockConnectionForTopic() = default;

  void send(Frame frame)
  {
    std::lock_guard<std::mutex> lock(frames_mutex_);
    sent_frames_.push_back(frame);
  }

  std::vector<Frame> get_sent_frames()
  {
    std::lock_guard<std::mutex> lock(frames_mutex_);
    return sent_frames_;
  }

  void clear_frames()
  {
    std::lock_guard<std::mutex> lock(frames_mutex_);
    sent_frames_.clear();
  }

  size_t get_frame_count()
  {
    std::lock_guard<std::mutex> lock(frames_mutex_);
    return sent_frames_.size();
  }

  std::string remote_endpoint() const
  {
    return "127.0.0.1:12345";
  }

  void close() {}

  void start(std::function<void(Ptr, Frame)>, std::function<void(Ptr, std::error_code)>) {}

private:
  std::mutex frames_mutex_;
  std::vector<Frame> sent_frames_;
};

/// Test fixture for TopicBridge tests
class TopicBridgeTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    rclcpp::init(0, nullptr);
    node_ = std::make_shared<rclcpp::Node>("test_topic_bridge_node");
    mock_connection_ = std::make_shared<MockConnectionForTopic>();
  }

  void TearDown() override
  {
    node_ = nullptr;
    mock_connection_ = nullptr;
    if (rclcpp::ok()) {
      rclcpp::shutdown();
    }
  }

  rclcpp::Node::SharedPtr node_;
  MockConnectionForTopic::Ptr mock_connection_;
};

/// Test TopicBridge initialization with empty topics list
TEST_F(TopicBridgeTest, InitializeWithEmptyTopicList)
{
  std::vector<TopicEntry> local_topics;
  std::vector<TopicEntry> remote_topics;

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<TopicBridge>(
      node_,
      conn,
      local_topics,
      remote_topics);
  });
}

/// Test TopicBridge initialization with local topics only
TEST_F(TopicBridgeTest, InitializeWithLocalTopicsOnly)
{
  std::vector<TopicEntry> local_topics;
  local_topics.push_back({"/cmd_vel", "geometry_msgs/msg/Twist"});
  local_topics.push_back({"/odom", "nav_msgs/msg/Odometry"});

  std::vector<TopicEntry> remote_topics;

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<TopicBridge>(
      node_,
      conn,
      local_topics,
      remote_topics);
  });
}

/// Test TopicBridge initialization with remote topics only
TEST_F(TopicBridgeTest, InitializeWithRemoteTopicsOnly)
{
  std::vector<TopicEntry> local_topics;

  std::vector<TopicEntry> remote_topics;
  remote_topics.push_back({"/sensor_data", "sensor_msgs/msg/LaserScan"});
  remote_topics.push_back({"/image", "sensor_msgs/msg/Image"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<TopicBridge>(
      node_,
      conn,
      local_topics,
      remote_topics);
  });
}

/// Test TopicBridge initialization with both local and remote topics
TEST_F(TopicBridgeTest, InitializeWithBothLocalAndRemoteTopics)
{
  std::vector<TopicEntry> local_topics;
  local_topics.push_back({"/cmd_vel", "geometry_msgs/msg/Twist"});
  local_topics.push_back({"/odom", "nav_msgs/msg/Odometry"});

  std::vector<TopicEntry> remote_topics;
  remote_topics.push_back({"/sensor_data", "sensor_msgs/msg/LaserScan"});
  remote_topics.push_back({"/image", "sensor_msgs/msg/Image"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<TopicBridge>(
      node_,
      conn,
      local_topics,
      remote_topics);
  });
}

/// Test topic frame handling
TEST_F(TopicBridgeTest, HandleTopicFrame)
{
  std::vector<TopicEntry> local_topics;

  std::vector<TopicEntry> remote_topics;
  remote_topics.push_back({"/sensor_data", "sensor_msgs/msg/LaserScan"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<TopicBridge>(
    node_,
    conn,
    local_topics,
    remote_topics);

  // Create a mock topic frame
  Frame topic_frame;
  topic_frame.type = FrameType::TOPIC_MSG;

  // Encode topic name
  std::string topic_name = "/sensor_data";
  encode_string(topic_frame.payload, topic_name);

  // Add mock message data
  std::vector<uint8_t> message_data = {0x01, 0x02, 0x03, 0x04, 0x05};
  topic_frame.payload.insert(
    topic_frame.payload.end(),
    message_data.begin(),
    message_data.end());

  // Should not throw
  EXPECT_NO_THROW({
    bridge->on_topic_frame(topic_frame);
  });
}

/// Test handling of empty topic frame
TEST_F(TopicBridgeTest, HandleEmptyTopicFrame)
{
  std::vector<TopicEntry> local_topics;
  std::vector<TopicEntry> remote_topics;

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<TopicBridge>(
    node_,
    conn,
    local_topics,
    remote_topics);

  Frame topic_frame;
  topic_frame.type = FrameType::TOPIC_MSG;
  topic_frame.payload.clear();

  // Should handle gracefully (not throw)
  EXPECT_NO_THROW({
    bridge->on_topic_frame(topic_frame);
  });
}

/// Test handling of topic frame with non-existent topic
TEST_F(TopicBridgeTest, HandleTopicFrameForNonExistentTopic)
{
  std::vector<TopicEntry> local_topics;
  std::vector<TopicEntry> remote_topics;
  remote_topics.push_back({"/sensor_data", "sensor_msgs/msg/LaserScan"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<TopicBridge>(
    node_,
    conn,
    local_topics,
    remote_topics);

  Frame topic_frame;
  topic_frame.type = FrameType::TOPIC_MSG;

  // Use non-existent topic name
  std::string topic_name = "/non_existent_topic";
  encode_string(topic_frame.payload, topic_name);

  std::vector<uint8_t> message_data = {0x01, 0x02, 0x03};
  topic_frame.payload.insert(
    topic_frame.payload.end(),
    message_data.begin(),
    message_data.end());

  // Should handle gracefully (silently ignore)
  EXPECT_NO_THROW({
    bridge->on_topic_frame(topic_frame);
  });
}

/// Test topic bridge with special characters in topic names
TEST_F(TopicBridgeTest, InitializeWithSpecialCharacterTopicNames)
{
  std::vector<TopicEntry> local_topics;
  local_topics.push_back({"/robot/arm/cmd_vel", "geometry_msgs/msg/Twist"});
  local_topics.push_back({"/sensors/imu/data", "sensor_msgs/msg/Imu"});

  std::vector<TopicEntry> remote_topics;
  remote_topics.push_back({"/remote/camera/image", "sensor_msgs/msg/Image"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<TopicBridge>(
      node_,
      conn,
      local_topics,
      remote_topics);
  });
}

/// Test topic frame with large payload
TEST_F(TopicBridgeTest, HandleTopicFrameWithLargePayload)
{
  std::vector<TopicEntry> local_topics;
  std::vector<TopicEntry> remote_topics;
  remote_topics.push_back({"/image", "sensor_msgs/msg/Image"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<TopicBridge>(
    node_,
    conn,
    local_topics,
    remote_topics);

  Frame topic_frame;
  topic_frame.type = FrameType::TOPIC_MSG;

  std::string topic_name = "/image";
  encode_string(topic_frame.payload, topic_name);

  // Add large payload (simulating image data)
  const size_t large_size = 100000; // 100KB
  for (size_t i = 0; i < large_size; ++i) {
    topic_frame.payload.push_back(static_cast<uint8_t>(i % 256));
  }

  // Should handle large payloads
  EXPECT_NO_THROW({
    bridge->on_topic_frame(topic_frame);
  });
}

/// Test topic frame encoding/decoding round-trip
TEST_F(TopicBridgeTest, TopicFrameEncodingRoundTrip)
{
  std::vector<TopicEntry> local_topics;
  std::vector<TopicEntry> remote_topics;
  remote_topics.push_back({"/test_topic", "std_msgs/msg/String"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<TopicBridge>(
    node_,
    conn,
    local_topics,
    remote_topics);

  // Create frame with specific data
  Frame original_frame;
  original_frame.type = FrameType::TOPIC_MSG;

  std::string topic_name = "/test_topic";
  encode_string(original_frame.payload, topic_name);

  std::vector<uint8_t> message_data = {0xAA, 0xBB, 0xCC, 0xDD};
  original_frame.payload.insert(
    original_frame.payload.end(),
    message_data.begin(),
    message_data.end());

  // Decode and verify
  size_t offset = 0;
  std::string decoded_topic = decode_string(original_frame.payload, offset);

  EXPECT_EQ(decoded_topic, topic_name);
  EXPECT_EQ(offset, 4 + topic_name.length()); // 4 bytes for length + topic name

  std::vector<uint8_t> decoded_message(
    original_frame.payload.begin() + offset,
    original_frame.payload.end());

  EXPECT_EQ(decoded_message, message_data);
}

/// Test multiple topics in bridge
TEST_F(TopicBridgeTest, MultipleLargeTopicBridge)
{
  std::vector<TopicEntry> local_topics;
  for (int i = 0; i < 10; ++i) {
    local_topics.push_back(
      {"/local_topic_" + std::to_string(i), "geometry_msgs/msg/Twist"});
  }

  std::vector<TopicEntry> remote_topics;
  for (int i = 0; i < 10; ++i) {
    remote_topics.push_back(
      {"/remote_topic_" + std::to_string(i), "sensor_msgs/msg/LaserScan"});
  }

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<TopicBridge>(
      node_,
      conn,
      local_topics,
      remote_topics);
  });
}

}  // namespace franbro

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

