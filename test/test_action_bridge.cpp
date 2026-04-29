#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <memory>
#include <thread>
#include <chrono>

#include "franbro/action_bridge.hpp"
#include "franbro/transport/protocol.hpp"

namespace franbro
{

/// Mock Connection for testing
class MockConnectionForAction : public std::enable_shared_from_this<MockConnectionForAction>
{
public:
  using Ptr = std::shared_ptr<MockConnectionForAction>;

  MockConnectionForAction() = default;

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

  std::string remote_endpoint() const
  {
    return "127.0.0.1:12345";
  }

  void close() {}

  void start(
    std::function<void(Ptr, Frame)>,
    std::function<void(Ptr, std::error_code)>) {}

private:
  std::mutex frames_mutex_;
  std::vector<Frame> sent_frames_;
};

/// Test fixture for ActionBridge tests
class ActionBridgeTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    rclcpp::init(0, nullptr);
    node_ = std::make_shared<rclcpp::Node>("test_action_bridge_node");
    mock_connection_ = std::make_shared<MockConnectionForAction>();
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
  MockConnectionForAction::Ptr mock_connection_;
};

/// Test ActionBridge initialization with empty actions list
TEST_F(ActionBridgeTest, InitializeWithEmptyActionList)
{
  std::vector<ActionEntry> remote_actions;

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<ActionBridge>(
      node_,
      conn,
      remote_actions);
  });
}

/// Test ActionBridge initialization with single action
TEST_F(ActionBridgeTest, InitializeWithSingleAction)
{
  std::vector<ActionEntry> remote_actions;
  remote_actions.push_back({"move_action", "nav2_msgs/action/NavigateToPose"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<ActionBridge>(
      node_,
      conn,
      remote_actions);
  });
}

/// Test ActionBridge initialization with multiple actions
TEST_F(ActionBridgeTest, InitializeWithMultipleActions)
{
  std::vector<ActionEntry> remote_actions;
  remote_actions.push_back({"move_action", "nav2_msgs/action/NavigateToPose"});
  remote_actions.push_back({"dock_action", "nav2_msgs/action/DockRobot"});
  remote_actions.push_back({"spin_action", "nav2_msgs/action/Spin"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<ActionBridge>(
      node_,
      conn,
      remote_actions);
  });
}

/// Test action feedback frame handling
TEST_F(ActionBridgeTest, HandleActionFeedback)
{
  std::vector<ActionEntry> remote_actions;
  remote_actions.push_back({"move_action", "nav2_msgs/action/NavigateToPose"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<ActionBridge>(
    node_,
    conn,
    remote_actions);

  // Create a mock feedback frame
  Frame feedback_frame;
  feedback_frame.type = FrameType::ACTION_FEEDBACK;

  // Encode call_id (4 bytes)
  encode_uint32(feedback_frame.payload, 0);

  // Encode action name length and name
  std::string action_name = "move_action";
  encode_string(feedback_frame.payload, action_name);

  // Add mock feedback data
  std::vector<uint8_t> feedback_data = {0x01, 0x02, 0x03, 0x04};
  feedback_frame.payload.insert(
    feedback_frame.payload.end(),
    feedback_data.begin(),
    feedback_data.end());

  // Should not throw
  EXPECT_NO_THROW({
    bridge->on_action_feedback(feedback_frame);
  });
}

/// Test action result frame handling
TEST_F(ActionBridgeTest, HandleActionResult)
{
  std::vector<ActionEntry> remote_actions;
  remote_actions.push_back({"move_action", "nav2_msgs/action/NavigateToPose"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<ActionBridge>(
    node_,
    conn,
    remote_actions);

  // Create a mock result frame
  Frame result_frame;
  result_frame.type = FrameType::ACTION_RESULT;

  // Encode call_id (4 bytes)
  encode_uint32(result_frame.payload, 0);

  // Encode status (4 bytes)
  encode_uint32(result_frame.payload, 4); // SUCCESS status

  // Add mock result data
  std::vector<uint8_t> result_data = {0x01, 0x02, 0x03, 0x04};
  result_frame.payload.insert(
    result_frame.payload.end(),
    result_data.begin(),
    result_data.end());

  // Should not throw
  EXPECT_NO_THROW({
    bridge->on_action_result(result_frame);
  });
}

/// Test handling of empty feedback frame payload
TEST_F(ActionBridgeTest, HandleEmptyFeedbackFrame)
{
  std::vector<ActionEntry> remote_actions;
  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<ActionBridge>(
    node_,
    conn,
    remote_actions);

  Frame feedback_frame;
  feedback_frame.type = FrameType::ACTION_FEEDBACK;
  feedback_frame.payload.clear();

  // Should handle gracefully (not throw)
  EXPECT_NO_THROW({
    bridge->on_action_feedback(feedback_frame);
  });
}

/// Test handling of malformed feedback frame (too short)
TEST_F(ActionBridgeTest, HandleMalformedFeedbackFrame)
{
  std::vector<ActionEntry> remote_actions;
  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<ActionBridge>(
    node_,
    conn,
    remote_actions);

  Frame feedback_frame;
  feedback_frame.type = FrameType::ACTION_FEEDBACK;
  feedback_frame.payload = {0x01, 0x02}; // Too short (need at least 8 bytes)

  // Should handle gracefully
  EXPECT_NO_THROW({
    bridge->on_action_feedback(feedback_frame);
  });
}

/// Test handling of empty result frame payload
TEST_F(ActionBridgeTest, HandleEmptyResultFrame)
{
  std::vector<ActionEntry> remote_actions;
  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<ActionBridge>(
    node_,
    conn,
    remote_actions);

  Frame result_frame;
  result_frame.type = FrameType::ACTION_RESULT;
  result_frame.payload.clear();

  // Should handle gracefully (not throw)
  EXPECT_NO_THROW({
    bridge->on_action_result(result_frame);
  });
}

/// Test handling of malformed result frame (too short)
TEST_F(ActionBridgeTest, HandleMalformedResultFrame)
{
  std::vector<ActionEntry> remote_actions;
  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<ActionBridge>(
    node_,
    conn,
    remote_actions);

  Frame result_frame;
  result_frame.type = FrameType::ACTION_RESULT;
  result_frame.payload = {0x01, 0x02, 0x03}; // Too short (need at least 8 bytes)

  // Should handle gracefully
  EXPECT_NO_THROW({
    bridge->on_action_result(result_frame);
  });
}

/// Test action bridge with special characters in action names
TEST_F(ActionBridgeTest, InitializeWithSpecialCharacterActionNames)
{
  std::vector<ActionEntry> remote_actions;
  remote_actions.push_back({"/robot/arm/move", "nav2_msgs/action/NavigateToPose"});
  remote_actions.push_back({"/sensors/scan_action", "nav2_msgs/action/Spin"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);

  EXPECT_NO_THROW({
    auto bridge = std::make_shared<ActionBridge>(
      node_,
      conn,
      remote_actions);
  });
}

/// Test concurrent feedback handling
TEST_F(ActionBridgeTest, ConcurrentFeedbackHandling)
{
  std::vector<ActionEntry> remote_actions;
  remote_actions.push_back({"move_action", "nav2_msgs/action/NavigateToPose"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<ActionBridge>(
    node_,
    conn,
    remote_actions);

  std::vector<std::thread> threads;

  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([bridge, i]() {
      Frame feedback_frame;
      feedback_frame.type = FrameType::ACTION_FEEDBACK;
      encode_uint32(feedback_frame.payload, i);

      std::string action_name = "move_action";
      encode_string(feedback_frame.payload, action_name);

      for (int j = 0; j < 10; ++j) {
        feedback_frame.payload.push_back(static_cast<uint8_t>(i + j));
      }

      bridge->on_action_feedback(feedback_frame);
    });
  }

  for (auto & thread : threads) {
    thread.join();
  }
}

/// Test concurrent result handling
TEST_F(ActionBridgeTest, ConcurrentResultHandling)
{
  std::vector<ActionEntry> remote_actions;
  remote_actions.push_back({"move_action", "nav2_msgs/action/NavigateToPose"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<ActionBridge>(
    node_,
    conn,
    remote_actions);

  std::vector<std::thread> threads;

  for (int i = 0; i < 5; ++i) {
    threads.emplace_back([bridge, i]() {
      Frame result_frame;
      result_frame.type = FrameType::ACTION_RESULT;
      encode_uint32(result_frame.payload, i);
      encode_uint32(result_frame.payload, 4); // Status

      for (int j = 0; j < 10; ++j) {
        result_frame.payload.push_back(static_cast<uint8_t>(i + j));
      }

      bridge->on_action_result(result_frame);
    });
  }

  for (auto & thread : threads) {
    thread.join();
  }
}

/// Test feedback with non-existent action name
TEST_F(ActionBridgeTest, FeedbackForNonExistentAction)
{
  std::vector<ActionEntry> remote_actions;
  remote_actions.push_back({"move_action", "nav2_msgs/action/NavigateToPose"});

  auto conn = std::static_pointer_cast<Connection>(mock_connection_);
  auto bridge = std::make_shared<ActionBridge>(
    node_,
    conn,
    remote_actions);

  Frame feedback_frame;
  feedback_frame.type = FrameType::ACTION_FEEDBACK;
  encode_uint32(feedback_frame.payload, 0);

  // Use a non-existent action name
  std::string action_name = "non_existent_action";
  encode_string(feedback_frame.payload, action_name);

  // Should handle gracefully (silently ignore)
  EXPECT_NO_THROW({
    bridge->on_action_feedback(feedback_frame);
  });
}

}  // namespace franbro

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}




