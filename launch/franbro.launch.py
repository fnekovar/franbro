from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_file_arg = DeclareLaunchArgument(
        "config_file",
        default_value=PathJoinSubstitution(
            [FindPackageShare("franbro"), "config", "example_config.yaml"]
        ),
        description="Absolute path to the FranBro YAML configuration file.",
    )

    franbro_node = Node(
        package="franbro",
        executable="franbro_node",
        name="franbro",
        output="screen",
        parameters=[
            {"config_file": LaunchConfiguration("config_file")}
        ],
    )

    return LaunchDescription([
        config_file_arg,
        franbro_node,
    ])
