#pragma once

#include <string>

#include "franbro/config.hpp"

namespace franbro
{

/// Parse a YAML configuration file and return a populated Config struct.
/// Throws std::runtime_error on parse or validation errors.
Config parse_config(const std::string & yaml_path);

}  // namespace franbro
