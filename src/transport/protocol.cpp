#include "franbro/transport/protocol.hpp"

#include <cstring>
#include <stdexcept>

#include <boost/asio.hpp>

namespace franbro
{

// ── Connection ───────────────────────────────────────────────────────────────

Connection::Connection(boost::asio::ip::tcp::socket socket)
: socket_(std::move(socket))
, strand_(boost::asio::make_strand(socket_.get_executor()))
{}

void Connection::start(FrameHandler frame_handler, ErrorHandler error_handler)
{
  frame_handler_ = std::move(frame_handler);
  error_handler_ = std::move(error_handler);
  async_read_header();
}

void Connection::close()
{
  boost::system::error_code ec;
  socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
  socket_.close(ec);
}

std::string Connection::remote_endpoint() const
{
  boost::system::error_code ec;
  auto ep = socket_.remote_endpoint(ec);
  if (ec) {
    return "<unknown>";
  }
  return ep.address().to_string() + ":" + std::to_string(ep.port());
}

// ── Wire format: [4-byte type BE][4-byte payload-len BE][payload...] ─────────

void Connection::send(Frame frame)
{
  std::vector<uint8_t> wire;
  wire.reserve(8 + frame.payload.size());

  uint32_t type_val = static_cast<uint32_t>(frame.type);
  encode_uint32(wire, type_val);
  encode_uint32(wire, static_cast<uint32_t>(frame.payload.size()));
  wire.insert(wire.end(), frame.payload.begin(), frame.payload.end());

  boost::asio::post(strand_, [self = shared_from_this(), w = std::move(wire)]() mutable {
    self->do_send(std::move(w));
  });
}

void Connection::do_send(std::vector<uint8_t> wire)
{
  send_queue_.push_back(std::move(wire));
  if (sending_) {
    return;  // will be picked up after current write completes
  }
  sending_ = true;
  flush_send_queue();
}

void Connection::flush_send_queue()
{
  if (send_queue_.empty()) {
    sending_ = false;
    return;
  }

  auto & front = send_queue_.front();
  boost::asio::async_write(
    socket_,
    boost::asio::buffer(front),
    boost::asio::bind_executor(strand_,
      [self = shared_from_this()](boost::system::error_code ec, std::size_t /*bytes*/) {
        self->send_queue_.erase(self->send_queue_.begin());
        if (ec) {
          self->sending_ = false;
          if (self->error_handler_) {
            self->error_handler_(self, ec);
          }
          return;
        }
        self->flush_send_queue();
      }));
}

void Connection::async_read_header()
{
  boost::asio::async_read(
    socket_,
    boost::asio::buffer(header_buf_),
    [self = shared_from_this()](boost::system::error_code ec, std::size_t /*bytes*/) {
      if (ec) {
        if (self->error_handler_) {
          self->error_handler_(self, ec);
        }
        return;
      }
      size_t offset = 0;
      std::vector<uint8_t> hdr(self->header_buf_.begin(), self->header_buf_.end());
      uint32_t type_val    = decode_uint32(hdr, offset);
      uint32_t payload_len = decode_uint32(hdr, offset);
      self->async_read_payload(type_val, payload_len);
    });
}

void Connection::async_read_payload(uint32_t type_val, uint32_t payload_len)
{
  auto payload_buf = std::make_shared<std::vector<uint8_t>>(payload_len);

  auto do_read = [self = shared_from_this(), type_val, payload_buf]() {
    boost::asio::async_read(
      self->socket_,
      boost::asio::buffer(*payload_buf),
      [self, type_val, payload_buf](boost::system::error_code ec, std::size_t /*bytes*/) {
        if (ec) {
          if (self->error_handler_) {
            self->error_handler_(self, ec);
          }
          return;
        }
        Frame frame;
        frame.type    = static_cast<FrameType>(type_val);
        frame.payload = std::move(*payload_buf);
        if (self->frame_handler_) {
          self->frame_handler_(self, std::move(frame));
        }
        self->async_read_header();
      });
  };

  if (payload_len == 0) {
    Frame frame;
    frame.type = static_cast<FrameType>(type_val);
    if (frame_handler_) {
      frame_handler_(shared_from_this(), std::move(frame));
    }
    async_read_header();
  } else {
    do_read();
  }
}

}  // namespace franbro
