#include <memory>

#include <rclcpp/rclcpp.hpp>

#include "franbro/franbro_node.hpp"

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);

  rclcpp::NodeOptions options;
  auto node = std::make_shared<franbro::FranBroNode>(options);

  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
