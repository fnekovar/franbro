# FranBro Test Suite

This directory contains comprehensive unit and integration tests for the FranBro middleware bridge.

## Overview

The test suite covers the following components:

- **test_protocol.cpp**: Protocol layer encoding/decoding tests
- **test_service_bridge.cpp**: Service bridge implementation tests
- **test_action_bridge.cpp**: Action bridge implementation tests
- **test_topic_bridge.cpp**: Topic bridge implementation tests

## Running the Tests

### Build with Tests

To build the project with tests enabled:

```bash
cd /home/nekovfra/git/franbro
mkdir -p cmake-build-debug
cd cmake-build-debug
cmake -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

### Run All Tests

```bash
cd cmake-build-debug
ctest --output-on-failure
```

### Run Specific Test

```bash
cd cmake-build-debug
ctest -R test_protocol --output-on-failure
ctest -R test_service_bridge --output-on-failure
ctest -R test_action_bridge --output-on-failure
ctest -R test_topic_bridge --output-on-failure
```

### Run Tests with Verbose Output

```bash
cd cmake-build-debug
ctest --output-on-failure -VV
```

## Test Details

### Protocol Tests (`test_protocol.cpp`)

Tests for the low-level protocol encoding/decoding functions:

- **EncodeUint32**: Verify uint32 big-endian encoding
- **DecodeUint32**: Verify uint32 big-endian decoding
- **Uint32RoundTrip**: Verify encode/decode round-trip for uint32
- **EncodeString**: Verify string encoding with length prefix
- **DecodeString**: Verify string decoding from encoded format
- **StringRoundTrip**: Test various strings for encode/decode correctness
- **EncodeLargeString**: Test encoding of large strings (10KB+)
- **DecodeLargeString**: Test decoding of large strings
- **MixedEncoding**: Test combined uint32 and string encoding
- **FrameTypeValues**: Verify all frame type enum values
- **BoundaryConditions**: Test edge cases (zero, max uint32 values)
- **SpecialCharacters**: Test strings with ROS-specific characters

### Service Bridge Tests (`test_service_bridge.cpp`)

Tests for service bridge initialization and frame handling:

- **InitializeWithEmptyServiceList**: Test bridge creation with no services
- **InitializeWithSingleService**: Test bridge creation with one service
- **InitializeWithMultipleServices**: Test bridge creation with multiple services
- **HandleServiceResponse**: Test processing SERVICE_RESPONSE frames
- **HandleEmptyServiceResponse**: Test handling of empty response payloads
- **HandleMalformedServiceResponse**: Test error handling for truncated frames
- **InitializeWithSpecialCharacterServiceNames**: Test ROS naming conventions
- **ConcurrentServiceResponses**: Test thread-safe response handling
- **CallIdIncrement**: Verify call ID generation

### Action Bridge Tests (`test_action_bridge.cpp`)

Tests for action bridge initialization and frame handling:

- **InitializeWithEmptyActionList**: Test bridge creation with no actions
- **InitializeWithSingleAction**: Test bridge creation with one action
- **InitializeWithMultipleActions**: Test bridge creation with multiple actions
- **HandleActionFeedback**: Test processing ACTION_FEEDBACK frames
- **HandleActionResult**: Test processing ACTION_RESULT frames
- **HandleEmptyFeedbackFrame**: Test handling of empty feedback payloads
- **HandleMalformedFeedbackFrame**: Test error handling for truncated feedback
- **HandleEmptyResultFrame**: Test handling of empty result payloads
- **HandleMalformedResultFrame**: Test error handling for truncated results
- **InitializeWithSpecialCharacterActionNames**: Test ROS naming conventions
- **ConcurrentFeedbackHandling**: Test thread-safe feedback processing
- **ConcurrentResultHandling**: Test thread-safe result processing
- **FeedbackForNonExistentAction**: Test handling of unknown action names

### Topic Bridge Tests (`test_topic_bridge.cpp`)

Tests for topic bridge initialization and frame handling:

- **InitializeWithEmptyTopicList**: Test bridge creation with no topics
- **InitializeWithLocalTopicsOnly**: Test bridge with only local topics
- **InitializeWithRemoteTopicsOnly**: Test bridge with only remote topics
- **InitializeWithBothLocalAndRemoteTopics**: Test bridge with both topic sets
- **HandleTopicFrame**: Test processing TOPIC_MSG frames
- **HandleEmptyTopicFrame**: Test handling of empty payloads
- **HandleTopicFrameForNonExistentTopic**: Test handling of unknown topics
- **InitializeWithSpecialCharacterTopicNames**: Test ROS naming conventions
- **HandleTopicFrameWithLargePayload**: Test large message handling (1MB+)
- **TopicFrameEncodingRoundTrip**: Test encode/decode correctness
- **MultipleLargeTopicBridge**: Test bridge with many topics (20 total)

## Mock Objects

Each test module uses mock `Connection` objects to simulate network communication without actual network I/O.

### MockConnection Interface

```cpp
class MockConnection {
  void send(Frame frame);              // Queue frame for sending
  std::vector<Frame> get_sent_frames(); // Retrieve sent frames
  void clear_frames();                 // Clear frame history
  std::string remote_endpoint() const; // Get peer address
  void close();                        // Close connection
  void start(...);                     // Start async operations
};
```

## Test Coverage

The test suite provides coverage for:

- **Happy path scenarios**: Normal operation with valid inputs
- **Edge cases**: Empty payloads, boundary values, large data
- **Error handling**: Malformed frames, missing data, unknown entities
- **Concurrency**: Thread-safe frame processing
- **Protocol correctness**: Encoding/decoding round-trips
- **Integration**: Multiple frames and mixed frame types

## Adding New Tests

To add a new test:

1. Create a new test file in the `test/` directory
2. Add test fixture class inheriting from `::testing::Test`
3. Implement `SetUp()` and `TearDown()` methods
4. Add test cases using `TEST_F()` macro
5. Update `CMakeLists.txt` with new `ament_add_gtest()` target

Example:

```cpp
class MyBridgeTest : public ::testing::Test {
protected:
  void SetUp() override { /* initialization */ }
  void TearDown() override { /* cleanup */ }
};

TEST_F(MyBridgeTest, TestSomething) {
  EXPECT_NO_THROW({ /* test code */ });
}
```

## Building with Sanitizers

To enable address and memory sanitizers:

```bash
cmake -DBUILD_TESTING=ON \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
      ..
```

## Dependencies

- Google Test (gtest)
- ROS 2 (rclcpp, rclcpp_action)
- Boost ASIO

## Continuous Integration

The test suite is designed to run in CI/CD pipelines. All tests should:

- Complete in under 60 seconds
- Not require external services
- Produce deterministic results
- Report all failures with meaningful messages

## Troubleshooting

### Tests fail with "Node not initialized"

Ensure `rclcpp::init()` is called in `SetUp()` and `rclcpp::shutdown()` in `TearDown()`.

### Memory leaks detected

Check that all allocated resources are properly released in `TearDown()`.

### Timeout errors

Some tests use `std::chrono` for timing. Increase timeout values if running on slow systems.

