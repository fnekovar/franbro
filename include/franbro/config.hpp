#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace franbro
{

// ── Registered message / service / action entries ──────────────────────────

struct TopicEntry
{
  std::string name;  // e.g. /robot/cmd_vel
  std::string type;  // e.g. geometry_msgs/msg/Twist
};

struct ServiceEntry
{
  std::string name;  // e.g. /set_bool
  std::string type;  // e.g. std_srvs/srv/SetBool
};

struct ActionEntry
{
  std::string name;  // e.g. /navigate
  std::string type;  // e.g. nav2_msgs/action/NavigateToPose
};

struct RemoteEntry
{
  std::string host;
  uint16_t    port{7890};
};

// ── Top-level configuration ─────────────────────────────────────────────────

struct Config
{
  uint16_t                  port{7890};
  std::vector<TopicEntry>   topics;
  std::vector<ServiceEntry> services;
  std::vector<ActionEntry>  actions;
  std::vector<RemoteEntry>  remotes;
};

}  // namespace franbro
