#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialized_message.hpp>
#include <rcl/service.h>
#include <rmw/rmw.h>

namespace franbro
{

/// Wrapper for a generic service server that operates on serialized messages.
/// This allows type-agnostic service handling at the wire protocol level.
class GenericService
{
public:
  using SharedPtr = std::shared_ptr<GenericService>;
  using CallbackType = std::function<void(
    std::shared_ptr<rmw_request_id_t>,
    std::shared_ptr<rclcpp::SerializedMessage>,
    std::shared_ptr<rclcpp::SerializedMessage>)>;

  GenericService(
    rclcpp::Node::SharedPtr node,
    const std::string & service_name,
    const std::string & service_type,
    CallbackType callback,
    const rclcpp::QoS & qos = rclcpp::ServicesQoS());

  ~GenericService();

private:
  rclcpp::Node::SharedPtr node_;
  CallbackType callback_;
  rcl_service_t service_handle_;
  bool service_initialized_{false};
};

}  // namespace franbro

