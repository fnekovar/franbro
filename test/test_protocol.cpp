#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

#include "franbro/transport/protocol.hpp"

namespace franbro
{

/// Test suite for protocol encoding/decoding
class ProtocolTest : public ::testing::Test
{
protected:
  void SetUp() override {}
  void TearDown() override {}
};

/// Test encoding a uint32
TEST_F(ProtocolTest, EncodeUint32)
{
  std::vector<uint8_t> buffer;

  encode_uint32(buffer, 0x12345678);

  EXPECT_EQ(buffer.size(), 4);
  EXPECT_EQ(buffer[0], 0x12);
  EXPECT_EQ(buffer[1], 0x34);
  EXPECT_EQ(buffer[2], 0x56);
  EXPECT_EQ(buffer[3], 0x78);
}

/// Test encoding multiple uint32 values
TEST_F(ProtocolTest, EncodeMultipleUint32)
{
  std::vector<uint8_t> buffer;

  encode_uint32(buffer, 0x11223344);
  encode_uint32(buffer, 0xAABBCCDD);

  EXPECT_EQ(buffer.size(), 8);
  EXPECT_EQ(buffer[0], 0x11);
  EXPECT_EQ(buffer[1], 0x22);
  EXPECT_EQ(buffer[2], 0x33);
  EXPECT_EQ(buffer[3], 0x44);
  EXPECT_EQ(buffer[4], 0xAA);
  EXPECT_EQ(buffer[5], 0xBB);
  EXPECT_EQ(buffer[6], 0xCC);
  EXPECT_EQ(buffer[7], 0xDD);
}

/// Test decoding a uint32
TEST_F(ProtocolTest, DecodeUint32)
{
  std::vector<uint8_t> buffer = {0x12, 0x34, 0x56, 0x78};
  size_t offset = 0;

  uint32_t value = decode_uint32(buffer, offset);

  EXPECT_EQ(value, 0x12345678);
  EXPECT_EQ(offset, 4);
}

/// Test decoding multiple uint32 values
TEST_F(ProtocolTest, DecodeMultipleUint32)
{
  std::vector<uint8_t> buffer = {0x11, 0x22, 0x33, 0x44, 0xAA, 0xBB, 0xCC, 0xDD};
  size_t offset = 0;

  uint32_t value1 = decode_uint32(buffer, offset);
  uint32_t value2 = decode_uint32(buffer, offset);

  EXPECT_EQ(value1, 0x11223344);
  EXPECT_EQ(value2, 0xAABBCCDD);
  EXPECT_EQ(offset, 8);
}

/// Test uint32 round-trip encoding/decoding
TEST_F(ProtocolTest, Uint32RoundTrip)
{
  const uint32_t original = 0xDEADBEEF;
  std::vector<uint8_t> buffer;

  encode_uint32(buffer, original);
  size_t offset = 0;
  uint32_t decoded = decode_uint32(buffer, offset);

  EXPECT_EQ(decoded, original);
}

/// Test encoding a string
TEST_F(ProtocolTest, EncodeString)
{
  std::vector<uint8_t> buffer;
  std::string test_string = "hello";

  encode_string(buffer, test_string);

  // Should be 4 bytes (length) + 5 bytes (string) = 9 bytes
  EXPECT_EQ(buffer.size(), 9);

  // Check length (big-endian)
  EXPECT_EQ(buffer[0], 0x00);
  EXPECT_EQ(buffer[1], 0x00);
  EXPECT_EQ(buffer[2], 0x00);
  EXPECT_EQ(buffer[3], 0x05);

  // Check string content
  EXPECT_EQ(buffer[4], 'h');
  EXPECT_EQ(buffer[5], 'e');
  EXPECT_EQ(buffer[6], 'l');
  EXPECT_EQ(buffer[7], 'l');
  EXPECT_EQ(buffer[8], 'o');
}

/// Test encoding an empty string
TEST_F(ProtocolTest, EncodeEmptyString)
{
  std::vector<uint8_t> buffer;
  std::string empty_string;

  encode_string(buffer, empty_string);

  // Should be 4 bytes (length only)
  EXPECT_EQ(buffer.size(), 4);
  EXPECT_EQ(buffer[0], 0x00);
  EXPECT_EQ(buffer[1], 0x00);
  EXPECT_EQ(buffer[2], 0x00);
  EXPECT_EQ(buffer[3], 0x00);
}

/// Test decoding a string
TEST_F(ProtocolTest, DecodeString)
{
  std::vector<uint8_t> buffer;
  std::string original = "world";

  encode_string(buffer, original);
  size_t offset = 0;
  std::string decoded = decode_string(buffer, offset);

  EXPECT_EQ(decoded, original);
  EXPECT_EQ(offset, 9); // 4 bytes length + 5 bytes string
}

/// Test string round-trip encoding/decoding
TEST_F(ProtocolTest, StringRoundTrip)
{
  std::vector<std::string> test_strings = {
    "hello",
    "world",
    "/ros/topic/name",
    "std_srvs/srv/SetBool",
    "",
    "a",
    "very_long_string_with_many_characters_for_testing_purposes"
  };

  for (const auto & original : test_strings) {
    std::vector<uint8_t> buffer;
    encode_string(buffer, original);

    size_t offset = 0;
    std::string decoded = decode_string(buffer, offset);

    EXPECT_EQ(decoded, original);
  }
}

/// Test encoding a large string
TEST_F(ProtocolTest, EncodeLargeString)
{
  std::vector<uint8_t> buffer;
  std::string large_string(10000, 'x');

  encode_string(buffer, large_string);

  EXPECT_EQ(buffer.size(), 10004); // 4 bytes length + 10000 bytes string
}

/// Test decoding a large string
TEST_F(ProtocolTest, DecodeLargeString)
{
  std::string large_string(10000, 'y');
  std::vector<uint8_t> buffer;

  encode_string(buffer, large_string);
  size_t offset = 0;
  std::string decoded = decode_string(buffer, offset);

  EXPECT_EQ(decoded, large_string);
  EXPECT_EQ(decoded.size(), 10000);
}

/// Test mixed encoding (uint32 + string)
TEST_F(ProtocolTest, MixedEncoding)
{
  std::vector<uint8_t> buffer;

  uint32_t call_id = 42;
  std::string service_name = "test_service";

  encode_uint32(buffer, call_id);
  encode_string(buffer, service_name);

  size_t offset = 0;
  uint32_t decoded_id = decode_uint32(buffer, offset);
  std::string decoded_name = decode_string(buffer, offset);

  EXPECT_EQ(decoded_id, call_id);
  EXPECT_EQ(decoded_name, service_name);
}

/// Test FrameType enum values
TEST_F(ProtocolTest, FrameTypeValues)
{
  EXPECT_EQ(static_cast<uint32_t>(FrameType::HANDSHAKE), 0x01);
  EXPECT_EQ(static_cast<uint32_t>(FrameType::TOPIC_MSG), 0x02);
  EXPECT_EQ(static_cast<uint32_t>(FrameType::SERVICE_REQUEST), 0x03);
  EXPECT_EQ(static_cast<uint32_t>(FrameType::SERVICE_RESPONSE), 0x04);
  EXPECT_EQ(static_cast<uint32_t>(FrameType::ACTION_GOAL), 0x05);
  EXPECT_EQ(static_cast<uint32_t>(FrameType::ACTION_FEEDBACK), 0x06);
  EXPECT_EQ(static_cast<uint32_t>(FrameType::ACTION_RESULT), 0x07);
  EXPECT_EQ(static_cast<uint32_t>(FrameType::ACTION_CANCEL), 0x08);
  EXPECT_EQ(static_cast<uint32_t>(FrameType::KEEPALIVE), 0x09);
}

/// Test Frame struct
TEST_F(ProtocolTest, FrameStructure)
{
  Frame frame;

  frame.type = FrameType::TOPIC_MSG;
  frame.payload = {0x01, 0x02, 0x03, 0x04};

  EXPECT_EQ(frame.type, FrameType::TOPIC_MSG);
  EXPECT_EQ(frame.payload.size(), 4);
  EXPECT_EQ(frame.payload[0], 0x01);
}

/// Test boundary condition: zero value
TEST_F(ProtocolTest, BoundaryZeroValue)
{
  std::vector<uint8_t> buffer;

  encode_uint32(buffer, 0);
  size_t offset = 0;
  uint32_t decoded = decode_uint32(buffer, offset);

  EXPECT_EQ(decoded, 0);
}

/// Test boundary condition: max uint32 value
TEST_F(ProtocolTest, BoundaryMaxUint32Value)
{
  std::vector<uint8_t> buffer;

  encode_uint32(buffer, 0xFFFFFFFF);
  size_t offset = 0;
  uint32_t decoded = decode_uint32(buffer, offset);

  EXPECT_EQ(decoded, 0xFFFFFFFF);
}

/// Test string with special characters
TEST_F(ProtocolTest, StringWithSpecialCharacters)
{
  std::vector<std::string> special_strings = {
    "/robot/cmd_vel",
    "std_srvs/srv/SetBool",
    "sensor_msgs/msg/LaserScan",
    "nav2_msgs/action/NavigateToPose"
  };

  for (const auto & original : special_strings) {
    std::vector<uint8_t> buffer;
    encode_string(buffer, original);

    size_t offset = 0;
    std::string decoded = decode_string(buffer, offset);

    EXPECT_EQ(decoded, original);
  }
}

}  // namespace franbro

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

