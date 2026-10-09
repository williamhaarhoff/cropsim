#include "cropsim/generators/generator_factory.hpp"

#include "cropsim/generators/grid_generator.hpp"

#include <stdexcept>
#include <utility>

namespace cropsim::generators {

void GeneratorFactory::register_generator(std::string name, Creator creator) {
    if (name.empty() || !creator) {
        throw std::invalid_argument("generator registration requires a name and creator");
    }
    const auto [unused, inserted] = creators_.emplace(std::move(name), std::move(creator));
    static_cast<void>(unused);
    if (!inserted) {
        throw std::invalid_argument("generator name is already registered");
    }
}

std::unique_ptr<Generator> GeneratorFactory::create(const std::string_view name) const {
    const auto found = creators_.find(std::string(name));
    if (found == creators_.end()) {
        throw std::invalid_argument("unknown generator type: " + std::string(name));
    }
    auto generator = found->second();
    if (!generator) {
        throw std::runtime_error("generator creator returned null");
    }
    return generator;
}

GeneratorFactory make_builtin_generator_factory() {
    GeneratorFactory factory;
    factory.register_generator("grid", [] { return std::make_unique<GridGenerator>(); });
    return factory;
}

}  // namespace cropsim::generators
