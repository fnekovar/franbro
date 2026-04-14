cmake_minimum_required(VERSION 3.14)
project(franbro VERSION 0.1.0 LANGUAGES CXX)

# ── C++ standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── ROS 2 build tool ────────────────────────────────────────────────────────
find_package(ament_cmake REQUIRED)

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

# ── Source files ─────────────────────────────────────────────────────────────
set(FRANBRO_SOURCES
  src/main.cpp
  src/franbro_node.cpp
  src/config_parser.cpp
  src/topic_bridge.cpp
  src/service_bridge.cpp
  src/action_bridge.cpp
  src/transport/protocol.cpp
  src/transport/transport_server.cpp
  src/transport/transport_client.cpp
)

add_executable(franbro_node ${FRANBRO_SOURCES})

target_include_directories(franbro_node
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
