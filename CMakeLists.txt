cmake_minimum_required(VERSION 3.14)
project(franbro VERSION 0.1.0 LANGUAGES CXX)

# ── C++ standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Configuration options ───────────────────────────────────────────────────
# Path to the YAML configuration file for compile-time code generation
set(FRANBRO_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config/pc_config.yaml" CACHE STRING
  "Path to YAML configuration file for compile-time code generation")

# ── ROS 2 build tool ────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)

# ── Code generation ─────────────────────────────────────────────────────────
# Run the YAML-to-C++ code generator before compilation
set(FRANBRO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${FRANBRO_GENERATED_DIR}")

# Find Python3 for running the generator
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Custom command to generate C++ from YAML config
add_custom_command(
  OUTPUT "${FRANBRO_GENERATED_DIR}/generated_config.hpp"
  COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/yaml_to_cpp_generator.py"
    "${FRANBRO_CONFIG_FILE}"
    "${FRANBRO_GENERATED_DIR}"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/yaml_to_cpp_generator.py"
    "${FRANBRO_CONFIG_FILE}"
  COMMENT "Generating C++ configuration from YAML..."
)

# Create a target for generated files
add_custom_target(franbro_generate_config ALL
  DEPENDS "${FRANBRO_GENERATED_DIR}/generated_config.hpp"
)

# ── ROS 2 runtime dependencies ──────────────────────────────────────────────
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(rosidl_typesupport_cpp REQUIRED)
find_package(rosidl_typesupport_introspection_cpp REQUIRED)
find_package(rosidl_runtime_cpp REQUIRED)
find_package(rmw REQUIRED)

# ── Third-party dependencies ─────────────────────────────────────────────────
find_package(yaml-cpp REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

# nlohmann/json: prefer a cmake target; fall back to header-only search
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    PATHS /usr/include /usr/local/include)
  if(NLOHMANN_JSON_INCLUDE_DIR)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    target_include_directories(nlohmann_json::nlohmann_json
      INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR})
  else()
    message(FATAL_ERROR "nlohmann/json not found. Install nlohmann-json-dev.")
  endif()
endif()

# ── Source files ─────────────────────────────────────────────────────────
set(FRANBRO_SOURCES
  src/main.cpp
  src/franbro_node.cpp
  src/config_parser.cpp
  src/generated_config_helper.cpp
  src/topic_bridge.cpp
  src/service_bridge.cpp
  src/action_bridge.cpp
  src/transport/protocol.cpp
  src/transport/transport_server.cpp
  src/transport/transport_client.cpp
)

add_executable(franbro_node ${FRANBRO_SOURCES})

# Make sure generated files are created before compilation
add_dependencies(franbro_node franbro_generate_config)

target_include_directories(franbro_node
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${FRANBRO_GENERATED_DIR}>
    $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(franbro_node
  rclcpp
  rclcpp_action
  rcl_interfaces
  rosidl_typesupport_cpp
  rosidl_typesupport_introspection_cpp
  rosidl_runtime_cpp
  rmw
)

target_link_libraries(franbro_node
  yaml-cpp
  Boost::system
  nlohmann_json::nlohmann_json
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS franbro_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

# ── Testing ───────────────────────────────────────────────────────────────────
#if(BUILD_TESTING)
#  find_package(ament_cmake_gtest REQUIRED)
#  find_package(ament_lint_auto REQUIRED)
#
#  # Protocol layer tests
#  ament_add_gtest(test_protocol
#    test/test_protocol.cpp
#  )
#  target_include_directories(test_protocol
#    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
#  )
#
#  # Unit tests for bridges
#  ament_add_gtest(test_service_bridge
#    test/test_service_bridge.cpp
#    src/service_bridge.cpp
#    src/transport/protocol.cpp
#  )
#  target_include_directories(test_service_bridge
#    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
#  )
#  ament_target_dependencies(test_service_bridge
#    rclcpp
#    rclcpp_action
#    rcl_interfaces
#    rosidl_typesupport_cpp
#    rosidl_typesupport_introspection_cpp
#    rosidl_runtime_cpp
#    rmw
#  )
#  target_link_libraries(test_service_bridge
#    yaml-cpp
#    Boost::system
#    nlohmann_json::nlohmann_json
#  )
#
#  ament_add_gtest(test_action_bridge
#    test/test_action_bridge.cpp
#    src/action_bridge.cpp
#    src/transport/protocol.cpp
#  )
#  target_include_directories(test_action_bridge
#    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
#  )
#  ament_target_dependencies(test_action_bridge
#    rclcpp
#    rclcpp_action
#    rcl_interfaces
#    rosidl_typesupport_cpp
#    rosidl_typesupport_introspection_cpp
#    rosidl_runtime_cpp
#    rmw
#  )
#  target_link_libraries(test_action_bridge
#    yaml-cpp
#    Boost::system
#    nlohmann_json::nlohmann_json
#  )
#
#  ament_add_gtest(test_topic_bridge
#    test/test_topic_bridge.cpp
#    src/topic_bridge.cpp
#    src/transport/protocol.cpp
#  )
#  target_include_directories(test_topic_bridge
#    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
#  )
#  ament_target_dependencies(test_topic_bridge
#    rclcpp
#    rclcpp_action
#    rcl_interfaces
#    rosidl_typesupport_cpp
#    rosidl_typesupport_introspection_cpp
#    rosidl_runtime_cpp
#    rmw
#  )
#  target_link_libraries(test_topic_bridge
#    yaml-cpp
#    Boost::system
#    nlohmann_json::nlohmann_json
#  )
#
#  ament_lint_auto_find_test_dependencies()
#endif()

ament_package()
