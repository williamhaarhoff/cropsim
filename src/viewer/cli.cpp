#include "cli.hpp"

#include "cropsim/snapshot.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cropsim::viewer {

Options parse_options(const int argc, const char* const* argv) {
    Options options;
    bool has_input = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--check") {
            options.check_only = true;
            continue;
        }
        if (argument == "--smoke") {
            options.smoke_test = true;
            continue;
        }
        if (has_input) {
            throw std::invalid_argument("exactly one world input must be specified");
        }
        if (argument == "--yaml" || argument == "--snapshot") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("exactly one world input must be specified");
            }
            options.input_kind = argument == "--yaml" ? InputKind::yaml : InputKind::snapshot;
            options.input_path = argv[++index];
        } else if (!argument.empty() && argument.front() != '-') {
            options.input_kind = InputKind::automatic;
            options.input_path = argument;
        } else {
            throw std::invalid_argument(
                "usage: cropsim_viewer <path> [--check|--smoke] (or --yaml/--snapshot <path>)");
        }
        has_input = true;
    }
    if (!has_input) {
        throw std::invalid_argument("a world input path is required");
    }
    if (options.check_only && options.smoke_test) {
        throw std::invalid_argument("--check and --smoke are mutually exclusive");
    }
    return options;
}

World load_world(const Options& options) {
    if (options.input_kind == InputKind::snapshot) {
        return load_snapshot(options.input_path);
    }
    std::ifstream stream(options.input_path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("failed to open world input: " + options.input_path.string());
    }
    const std::string description{std::istreambuf_iterator<char>(stream),
                                  std::istreambuf_iterator<char>()};
    constexpr std::string_view snapshot_magic{"CROPSIM\0", 8U};
    if (options.input_kind == InputKind::automatic &&
        description.size() >= snapshot_magic.size() &&
        std::string_view(description).substr(0U, snapshot_magic.size()) == snapshot_magic) {
        return load_snapshot(options.input_path);
    }
    try {
        return world_from_yaml(description);
    } catch (const std::exception& error) {
        if (options.input_kind == InputKind::automatic) {
            throw std::runtime_error("world input is neither a valid snapshot nor valid YAML: " +
                                     std::string(error.what()));
        }
        throw;
    }
}

}  // namespace cropsim::viewer
