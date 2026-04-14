#include "franbro/franbro_node.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "franbro/config_parser.hpp"

namespace franbro
{

using json = nlohmann::json;

// ── Construction / destruction ────────────────────────────────────────────────

FranBroNode::FranBroNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("franbro", options)
, work_guard_(boost::asio::make_work_guard(io_ctx_))
{
  this->declare_parameter<std::string>("config_file", "");
  init();
}

FranBroNode::~FranBroNode()
{
  // Stop server and all clients
  if (server_) {
    server_->stop();
  }
  for (auto & c : clients_) {
    c->stop();
  }

  // Drain io_context
  work_guard_.reset();
  for (auto & t : io_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
}

// ── Initialisation ────────────────────────────────────────────────────────────

void FranBroNode::init()
{
  const std::string config_path =
    this->get_parameter("config_file").as_string();

  if (config_path.empty()) {
    throw std::runtime_error(
      "FranBroNode: 'config_file' parameter must be set to the path of the YAML config.");
  }

  config_ = parse_config(config_path);

  RCLCPP_INFO(get_logger(),
    "FranBro starting on port %u with %zu topic(s), %zu service(s), %zu action(s), "
    "%zu remote(s)",
    config_.port,
    config_.topics.size(),
    config_.services.size(),
    config_.actions.size(),
    config_.remotes.size());

  // Start io_context threads
  const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
  const unsigned n_threads = std::min(hw, 4u);
  for (unsigned i = 0; i < n_threads; ++i) {
    io_threads_.emplace_back([this] { io_ctx_.run(); });
  }

  setup_server();
  setup_clients();

  // Periodic keepalive every 5 s
  keepalive_timer_ = this->create_wall_timer(
    std::chrono::seconds(5),
    [this]() {
      std::lock_guard<std::mutex> lk(ctx_mu_);
      for (auto & [conn_ptr, ctx] : contexts_) {
        Frame kf;
        kf.type = FrameType::KEEPALIVE;
        ctx.connection->send(kf);
      }
    });
}

void FranBroNode::setup_server()
{
  try {
    server_ = std::make_unique<TransportServer>(
      io_ctx_,
      config_.port,
      [this](Connection::Ptr conn) {
        on_new_connection(conn);  // no reconnect for server-accepted connections
      });
    RCLCPP_INFO(get_logger(), "FranBro server listening on port %u", config_.port);
  } catch (const std::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Failed to start server: %s", ex.what());
    throw;
  }
}

void FranBroNode::setup_clients()
{
  for (const auto & remote : config_.remotes) {
    RCLCPP_INFO(get_logger(),
      "Connecting to remote FranBro at %s:%u", remote.host.c_str(), remote.port);

    clients_.push_back(std::make_unique<TransportClient>(
      io_ctx_,
      remote.host,
      remote.port,
      [this, host = remote.host, port = remote.port](Connection::Ptr conn) {
        // The on_close callback logs and schedules reconnect via the client.
        // The client will re-fire on_connected once it reconnects.
        on_new_connection(conn, [this, host, port]() {
          RCLCPP_WARN(get_logger(),
            "Disconnected from %s:%u – will reconnect", host.c_str(), port);
        });
      },
      [this, host = remote.host, port = remote.port]() {
        RCLCPP_WARN(get_logger(),
          "Disconnected from %s:%u – will reconnect", host.c_str(), port);
      }));
  }
}

// ── Connection management ─────────────────────────────────────────────────────

void FranBroNode::on_new_connection(Connection::Ptr conn, std::function<void()> on_close)
{
  RCLCPP_INFO(get_logger(),
    "New connection from %s", conn->remote_endpoint().c_str());

  {
    std::lock_guard<std::mutex> lk(ctx_mu_);
    ConnectionContext ctx;
    ctx.connection = conn;
    contexts_[conn.get()] = std::move(ctx);
  }

  // Start receiving frames
  conn->start(
    [this](Connection::Ptr c, Frame f) {
      on_frame(c, std::move(f));
    },
    [this, on_close = std::move(on_close)](Connection::Ptr c, std::error_code ec) {
      on_error(c, ec);
      if (on_close) {
        on_close();
      }
    });

  send_handshake(conn);
}

void FranBroNode::send_handshake(Connection::Ptr conn)
{
  json manifest;

  json topics = json::array();
  for (const auto & t : config_.topics) {
    topics.push_back({{"name", t.name}, {"type", t.type}});
  }
  manifest["topics"] = topics;

  json services = json::array();
  for (const auto & s : config_.services) {
    services.push_back({{"name", s.name}, {"type", s.type}});
  }
  manifest["services"] = services;

  json actions = json::array();
  for (const auto & a : config_.actions) {
    actions.push_back({{"name", a.name}, {"type", a.type}});
  }
  manifest["actions"] = actions;

  const std::string payload_str = manifest.dump();

  Frame frame;
  frame.type = FrameType::HANDSHAKE;
  frame.payload.assign(payload_str.begin(), payload_str.end());
  conn->send(std::move(frame));
}

// ── Frame routing ─────────────────────────────────────────────────────────────

void FranBroNode::on_frame(Connection::Ptr conn, Frame frame)
{
  std::lock_guard<std::mutex> lk(ctx_mu_);
  auto it = contexts_.find(conn.get());
  if (it == contexts_.end()) {
    return;
  }

  ConnectionContext & ctx = it->second;

  switch (frame.type) {
    case FrameType::HANDSHAKE:
      handle_handshake(ctx, frame);
      break;

    case FrameType::TOPIC_MSG:
      if (ctx.topic_bridge) {
        ctx.topic_bridge->on_topic_frame(frame);
      }
      break;

    case FrameType::SERVICE_REQUEST:
      // A SERVICE_REQUEST arrives when *we* are the service owner and the
      // remote is relaying a client call.  We forward it to the real local
      // service by acting as a client.
      // For now, log and drop – full client-side forwarding is left as an
      // extension point (the remote proxies call us, which the ROS 2 service
      // executor will handle automatically via the local service server).
      RCLCPP_DEBUG(get_logger(), "SERVICE_REQUEST frame received (should be handled by ROS 2)");
      break;

    case FrameType::SERVICE_RESPONSE:
      if (ctx.service_bridge) {
        ctx.service_bridge->on_service_response(frame);
      }
      break;

    case FrameType::ACTION_GOAL:
      RCLCPP_DEBUG(get_logger(), "ACTION_GOAL frame received");
      break;

    case FrameType::ACTION_FEEDBACK:
      if (ctx.action_bridge) {
        ctx.action_bridge->on_action_feedback(frame);
      }
      break;

    case FrameType::ACTION_RESULT:
      if (ctx.action_bridge) {
        ctx.action_bridge->on_action_result(frame);
      }
      break;

    case FrameType::ACTION_CANCEL:
      RCLCPP_DEBUG(get_logger(), "ACTION_CANCEL frame received");
      break;

    case FrameType::KEEPALIVE:
      // No-op
      break;

    default:
      RCLCPP_WARN(get_logger(),
        "Unknown frame type 0x%02X from %s",
        static_cast<unsigned>(frame.type),
        conn->remote_endpoint().c_str());
      break;
  }
}

void FranBroNode::on_error(Connection::Ptr conn, std::error_code ec)
{
  RCLCPP_WARN(get_logger(),
    "Connection %s error: %s",
    conn->remote_endpoint().c_str(),
    ec.message().c_str());

  std::lock_guard<std::mutex> lk(ctx_mu_);
  contexts_.erase(conn.get());
}

void FranBroNode::handle_handshake(ConnectionContext & ctx, const Frame & frame)
{
  std::string json_str(frame.payload.begin(), frame.payload.end());

  json manifest;
  try {
    manifest = json::parse(json_str);
  } catch (const json::exception & ex) {
    RCLCPP_ERROR(get_logger(), "Failed to parse handshake JSON: %s", ex.what());
    return;
  }

  // Build remote capability lists
  std::vector<TopicEntry>   remote_topics;
  std::vector<ServiceEntry> remote_services;
  std::vector<ActionEntry>  remote_actions;

  if (manifest.contains("topics")) {
    for (const auto & t : manifest["topics"]) {
      remote_topics.push_back({t["name"].get<std::string>(), t["type"].get<std::string>()});
    }
  }
  if (manifest.contains("services")) {
    for (const auto & s : manifest["services"]) {
      remote_services.push_back({s["name"].get<std::string>(), s["type"].get<std::string>()});
    }
  }
  if (manifest.contains("actions")) {
    for (const auto & a : manifest["actions"]) {
      remote_actions.push_back({a["name"].get<std::string>(), a["type"].get<std::string>()});
    }
  }

  // Determine which remote capabilities we don't already own locally.
  // We create proxy publishers/servers only for things the remote has that we don't.

  auto not_local_topic = [this](const TopicEntry & t) {
    return std::none_of(config_.topics.begin(), config_.topics.end(),
      [&t](const TopicEntry & lt) { return lt.name == t.name; });
  };
  auto not_local_service = [this](const ServiceEntry & s) {
    return std::none_of(config_.services.begin(), config_.services.end(),
      [&s](const ServiceEntry & ls) { return ls.name == s.name; });
  };
  auto not_local_action = [this](const ActionEntry & a) {
    return std::none_of(config_.actions.begin(), config_.actions.end(),
      [&a](const ActionEntry & la) { return la.name == a.name; });
  };

  std::vector<TopicEntry> proxy_topics;
  for (const auto & t : remote_topics) {
    if (not_local_topic(t)) {
      proxy_topics.push_back(t);
    }
  }

  std::vector<ServiceEntry> proxy_services;
  for (const auto & s : remote_services) {
    if (not_local_service(s)) {
      proxy_services.push_back(s);
    }
  }

  std::vector<ActionEntry> proxy_actions;
  for (const auto & a : remote_actions) {
    if (not_local_action(a)) {
      proxy_actions.push_back(a);
    }
  }

  RCLCPP_INFO(get_logger(),
    "Handshake: creating %zu topic proxy(ies), %zu service proxy(ies), %zu action proxy(ies)",
    proxy_topics.size(), proxy_services.size(), proxy_actions.size());

  auto node_shared = shared_from_this();

  if (!proxy_topics.empty() || !config_.topics.empty()) {
    ctx.topic_bridge = std::make_shared<TopicBridge>(
      node_shared,
      ctx.connection,
      config_.topics,    // locally owned → subscribe and forward
      proxy_topics);     // remotely owned → create publishers
  }

  if (!proxy_services.empty()) {
    ctx.service_bridge = std::make_shared<ServiceBridge>(
      node_shared,
      ctx.connection,
      proxy_services);
  }

  if (!proxy_actions.empty()) {
    ctx.action_bridge = std::make_shared<ActionBridge>(
      node_shared,
      ctx.connection,
      proxy_actions);
  }
}

}  // namespace franbro
