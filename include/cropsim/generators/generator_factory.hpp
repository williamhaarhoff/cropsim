#pragma once

#include "cropsim/generators/generator.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cropsim::generators {

class GeneratorFactory final {
public:
    using Creator = std::function<std::unique_ptr<Generator>()>;

    void register_generator(std::string name, Creator creator);
    [[nodiscard]] std::unique_ptr<Generator> create(std::string_view name) const;

private:
    std::unordered_map<std::string, Creator> creators_;
};

[[nodiscard]] GeneratorFactory make_builtin_generator_factory();

}  // namespace cropsim::generators
