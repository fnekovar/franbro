#!/usr/bin/env python3
"""
YAML-to-C++ Code Generator for FranBro Configuration

This tool converts a FranBro YAML configuration file into C++ code that can be
compiled as part of the build process. This enables:
- Compile-time validation of services and actions
- Concrete template instantiations instead of generic clients
- Zero-overhead configuration at runtime
"""

import sys
import yaml
import os
from pathlib import Path
from typing import Dict, List, Any


def parse_config(yaml_path: str) -> Dict[str, Any]:
    """Parse YAML configuration file."""
    try:
        with open(yaml_path, 'r') as f:
            data = yaml.safe_load(f)
        return data.get('franbro', {})
    except Exception as e:
        print(f"Error parsing config file {yaml_path}: {e}", file=sys.stderr)
        sys.exit(1)


def sanitize_name(name: str) -> str:
    """Convert ROS topic/service/action names to valid C++ identifiers."""
    # Remove leading slashes and convert path separators to underscores
    name = name.lstrip('/')
    name = name.replace('/', '_')
    name = name.replace('-', '_')
    return name


def generate_config_header(config: Dict[str, Any]) -> str:
    """Generate the config header with compile-time constants."""
    port = config.get('port', 7890)

    topics = config.get('topics', [])
    services = config.get('services', [])
    actions = config.get('actions', [])
    remotes = config.get('remotes', [])

    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "#include <string>",
        "#include <vector>",
        "#include <array>",
        "",
        "#include \"franbro/config.hpp\"",
        "",
        "namespace franbro",
        "{",
        "namespace generated",
        "{",
        "",
        "// ── Compile-time configuration constants ────────────────────────────────────",
        "",
        f"constexpr uint16_t SERVER_PORT = {port};",
        "",
    ]

    # Topic entries
    lines.append("// ── Topic entries ────────────────────────────────────────────────────────────")
    lines.append("")
    lines.append("namespace topics")
    lines.append("{")

    # Generate topic constants
    for i, topic in enumerate(topics):
        name = topic.get('name', '')
        msg_type = topic.get('type', '')
        lines.append(f"  constexpr std::string_view topic_{i}_name = \"{name}\";")
        lines.append(f"  constexpr std::string_view topic_{i}_type = \"{msg_type}\";")

    lines.append("}")
    lines.append("")
    lines.append("// ── Service entries ────────────────────────────────────────────────────────────")
    lines.append("")
    lines.append("namespace services")
    lines.append("{")

    # Generate service constants
    for i, service in enumerate(services):
        name = service.get('name', '')
        srv_type = service.get('type', '')
        lines.append(f"  constexpr std::string_view service_{i}_name = \"{name}\";")
        lines.append(f"  constexpr std::string_view service_{i}_type = \"{srv_type}\";")

    lines.append("}")

    lines.append("")
    lines.append("// ── Action entries ────────────────────────────────────────────────────────────")
    lines.append("")
    lines.append("namespace actions")
    lines.append("{")

    # Generate action constants
    for i, action in enumerate(actions):
        name = action.get('name', '')
        action_type = action.get('type', '')
        lines.append(f"  constexpr std::string_view action_{i}_name = \"{name}\";")
        lines.append(f"  constexpr std::string_view action_{i}_type = \"{action_type}\";")

    lines.append("}")

    lines.append("")
    lines.append("// ── Remote entries ─────────────────────────────────────────────────────────────")
    lines.append("")
    lines.append("namespace remotes")
    lines.append("{")

    # Generate remote constants
    for i, remote in enumerate(remotes):
        host = remote.get('host', '')
        port_remote = remote.get('port', 7890)
        lines.append(f"  constexpr std::string_view remote_{i}_host = \"{host}\";")
        lines.append(f"  constexpr uint16_t remote_{i}_port = {port_remote};")

    lines.append("}")

    # Generate static config builder function
    lines.append("")
    lines.append("// ── Configuration builders ──────────────────────────────────────────────────")
    lines.append("// These functions construct the runtime Config structure from compile-time constants")
    lines.append("")
    lines.append("inline Config build_config()")
    lines.append("{")
    lines.append("  Config cfg;")
    lines.append("  cfg.port = SERVER_PORT;")
    lines.append("")

    # Add topics
    lines.append("  // Topics")
    for i in range(len(topics)):
        lines.append("  cfg.topics.push_back({")
        lines.append(f"    std::string(topics::topic_{i}_name),")
        lines.append(f"    std::string(topics::topic_{i}_type)")
        lines.append("  });")

    # Add services
    lines.append("")
    lines.append("  // Services")
    for i in range(len(services)):
        lines.append("  cfg.services.push_back({")
        lines.append(f"    std::string(services::service_{i}_name),")
        lines.append(f"    std::string(services::service_{i}_type)")
        lines.append("  });")

    # Add actions
    lines.append("")
    lines.append("  // Actions")
    for i in range(len(actions)):
        lines.append("  cfg.actions.push_back({")
        lines.append(f"    std::string(actions::action_{i}_name),")
        lines.append(f"    std::string(actions::action_{i}_type)")
        lines.append("  });")

    # Add remotes
    lines.append("")
    lines.append("  // Remotes")
    for i in range(len(remotes)):
        lines.append("  cfg.remotes.push_back({")
        lines.append(f"    std::string(remotes::remote_{i}_host),")
        lines.append(f"    remotes::remote_{i}_port")
        lines.append("  });")

    lines.append("")
    lines.append("  return cfg;")
    lines.append("}")
    lines.append("")
    lines.append("}  // namespace generated")
    lines.append("}  // namespace franbro")
    lines.append("")

    return "\n".join(lines)


def main():
    """Main entry point."""
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.yaml> <output_dir>", file=sys.stderr)
        sys.exit(1)

    yaml_path = sys.argv[1]
    output_dir = sys.argv[2]

    # Verify input file exists
    if not os.path.exists(yaml_path):
        print(f"Error: Config file not found: {yaml_path}", file=sys.stderr)
        sys.exit(1)

    # Create output directory if it doesn't exist
    Path(output_dir).mkdir(parents=True, exist_ok=True)

    # Parse configuration
    config = parse_config(yaml_path)

    # Generate header file
    config_header = generate_config_header(config)

    # Write output files
    header_path = os.path.join(output_dir, 'generated_config.hpp')
    with open(header_path, 'w') as f:
        f.write(config_header)

    print(f"Generated: {header_path}")


if __name__ == '__main__':
    main()







