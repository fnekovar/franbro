#include "franbro/generated_config_helper.hpp"

// Include the generated configuration header
#include "generated_config.hpp"

namespace franbro
{

Config get_generated_config()
{
  return generated::build_config();
}

}  // namespace franbro

