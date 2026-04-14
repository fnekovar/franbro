#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <system_error>

#include <boost/asio.hpp>

namespace franbro
{

// ── Frame type identifiers ───────────────────────────────────────────────────

enum class FrameType : uint32_t
{
  HANDSHAKE        = 0x01,
  TOPIC_MSG        = 0x02,
  SERVICE_REQUEST  = 0x03,
  SERVICE_RESPONSE = 0x04,
  ACTION_GOAL      = 0x05,
  ACTION_FEEDBACK  = 0x06,
  ACTION_RESULT    = 0x07,
  ACTION_CANCEL    = 0x08,
  KEEPALIVE        = 0x09,
};

// ── Raw frame ────────────────────────────────────────────────────────────────

struct Frame
{
  FrameType            type{FrameType::KEEPALIVE};
  std::vector<uint8_t> payload;
};

// ── Helpers to encode/decode typed payloads ─────────────────────────────────

/// Encode a 4-byte big-endian uint32 into dest.
inline void encode_uint32(std::vector<uint8_t> & dest, uint32_t v)
{
  dest.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
  dest.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  dest.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
  dest.push_back(static_cast<uint8_t>((v      ) & 0xFF));
}

/// Decode a 4-byte big-endian uint32 from src at offset; advances offset.
inline uint32_t decode_uint32(const std::vector<uint8_t> & src, size_t & offset)
{
  uint32_t v =
    (static_cast<uint32_t>(src[offset    ]) << 24) |
    (static_cast<uint32_t>(src[offset + 1]) << 16) |
    (static_cast<uint32_t>(src[offset + 2]) <<  8) |
    (static_cast<uint32_t>(src[offset + 3])      );
  offset += 4;
  return v;
}

/// Encode a length-prefixed string (4-byte len + bytes).
inline void encode_string(std::vector<uint8_t> & dest, const std::string & s)
{
  encode_uint32(dest, static_cast<uint32_t>(s.size()));
  dest.insert(dest.end(), s.begin(), s.end());
}

/// Decode a length-prefixed string; advances offset.
inline std::string decode_string(const std::vector<uint8_t> & src, size_t & offset)
{
  uint32_t len = decode_uint32(src, offset);
  std::string s(src.begin() + offset, src.begin() + offset + len);
  offset += len;
  return s;
}

// ── Async TCP connection (framing layer) ─────────────────────────────────────

/// Represents one live TCP connection.  Provides async frame send / receive.
/// Thread-safe: send() may be called from any thread.
class Connection : public std::enable_shared_from_this<Connection>
{
public:
  using Ptr         = std::shared_ptr<Connection>;
  using FrameHandler = std::function<void(Connection::Ptr, Frame)>;
  using ErrorHandler = std::function<void(Connection::Ptr, std::error_code)>;

  explicit Connection(boost::asio::ip::tcp::socket socket);

  /// Start async reading; frames are delivered to frame_handler.
  void start(FrameHandler frame_handler, ErrorHandler error_handler);

  /// Asynchronously send a frame (thread-safe).
  void send(Frame frame);

  /// Remote endpoint description, for logging.
  std::string remote_endpoint() const;

  /// Close the underlying socket.
  void close();

private:
  void async_read_header();
  void async_read_payload(uint32_t type, uint32_t payload_len);
  void do_send(std::vector<uint8_t> wire);
  void flush_send_queue();

  boost::asio::ip::tcp::socket socket_;
  std::array<uint8_t, 8>       header_buf_{};

  // Send queue – protected by strand so writes are serialised.
  boost::asio::strand<boost::asio::io_context::executor_type> strand_;
  std::vector<std::vector<uint8_t>> send_queue_;
  bool                              sending_{false};

  FrameHandler frame_handler_;
  ErrorHandler error_handler_;
};

}  // namespace franbro
