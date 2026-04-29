#pragma once

#include "franbro/config.hpp"

namespace franbro
{

/// Get the compile-time generated configuration.
/// This function is generated at compile-time from the YAML configuration file
/// and provides zero-overhead access to the configuration.
Config get_generated_config();

}  // namespace franbro

