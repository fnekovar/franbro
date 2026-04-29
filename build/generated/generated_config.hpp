#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

#include "franbro/config.hpp"

namespace franbro
{
namespace generated
{

// ── Compile-time configuration constants ────────────────────────────────────

constexpr uint16_t SERVER_PORT = 7890;

// ── Topic entries ────────────────────────────────────────────────────────────

namespace topics
{
  constexpr std::string_view topic_0_name = "/robot/cmd_vel";
  constexpr std::string_view topic_0_type = "geometry_msgs/msg/Twist";
  constexpr std::string_view topic_1_name = "/robot/odom";
  constexpr std::string_view topic_1_type = "nav_msgs/msg/Odometry";
}

// ── Service entries ────────────────────────────────────────────────────────────

namespace services
{
  constexpr std::string_view service_0_name = "/robot/set_bool";
  constexpr std::string_view service_0_type = "std_srvs/srv/SetBool";
  constexpr std::string_view service_1_name = "/robot/get_param";
  constexpr std::string_view service_1_type = "rcl_interfaces/srv/GetParameters";
}

// ── Action entries ────────────────────────────────────────────────────────────

namespace actions
{
  constexpr std::string_view action_0_name = "/robot/navigate";
  constexpr std::string_view action_0_type = "nav2_msgs/action/NavigateToPose";
}

// ── Remote entries ─────────────────────────────────────────────────────────────

namespace remotes
{
  constexpr std::string_view remote_0_host = "192.168.1.100";
  constexpr uint16_t remote_0_port = 7890;
  constexpr std::string_view remote_1_host = "robot2.local";
  constexpr uint16_t remote_1_port = 7890;
}

// ── Configuration builders ──────────────────────────────────────────────────
// These functions construct the runtime Config structure from compile-time constants

inline Config build_config()
{
  Config cfg;
  cfg.port = SERVER_PORT;

  // Topics
  cfg.topics.push_back({
    std::string(topics::topic_0_name),
    std::string(topics::topic_0_type)
  });
  cfg.topics.push_back({
    std::string(topics::topic_1_name),
    std::string(topics::topic_1_type)
  });

  // Services
  cfg.services.push_back({
    std::string(services::service_0_name),
    std::string(services::service_0_type)
  });
  cfg.services.push_back({
    std::string(services::service_1_name),
    std::string(services::service_1_type)
  });

  // Actions
  cfg.actions.push_back({
    std::string(actions::action_0_name),
    std::string(actions::action_0_type)
  });

  // Remotes
  cfg.remotes.push_back({
    std::string(remotes::remote_0_host),
    remotes::remote_0_port
  });
  cfg.remotes.push_back({
    std::string(remotes::remote_1_host),
    remotes::remote_1_port
  });

  return cfg;
}

}  // namespace generated
}  // namespace franbro
