#include "franbro/config_parser.hpp"

#include <stdexcept>
#include <string>

#include <yaml-cpp/yaml.h>

namespace franbro
{

Config parse_config(const std::string & yaml_path)
{
  YAML::Node root;
  try {
    root = YAML::LoadFile(yaml_path);
  } catch (const YAML::Exception & ex) {
    throw std::runtime_error("Failed to load config file '" + yaml_path + "': " + ex.what());
  }

  if (!root["franbro"]) {
    throw std::runtime_error("Config file '" + yaml_path + "' is missing top-level 'franbro' key");
  }

  YAML::Node fb = root["franbro"];
  Config cfg;

  if (fb["port"]) {
    cfg.port = fb["port"].as<uint16_t>();
  }

  if (fb["topics"]) {
    for (const auto & t : fb["topics"]) {
      TopicEntry e;
      e.name = t["name"].as<std::string>();
      e.type = t["type"].as<std::string>();
      cfg.topics.push_back(std::move(e));
    }
  }

  if (fb["services"]) {
    for (const auto & s : fb["services"]) {
      ServiceEntry e;
      e.name = s["name"].as<std::string>();
      e.type = s["type"].as<std::string>();
      cfg.services.push_back(std::move(e));
    }
  }

  if (fb["actions"]) {
    for (const auto & a : fb["actions"]) {
      ActionEntry e;
      e.name = a["name"].as<std::string>();
      e.type = a["type"].as<std::string>();
      cfg.actions.push_back(std::move(e));
    }
  }

  if (fb["remotes"]) {
    for (const auto & r : fb["remotes"]) {
      RemoteEntry e;
      e.host = r["host"].as<std::string>();
      if (r["port"]) {
        e.port = r["port"].as<uint16_t>();
      }
      cfg.remotes.push_back(std::move(e));
    }
  }

  return cfg;
}

}  // namespace franbro
