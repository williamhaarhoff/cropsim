#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace cropsim::expression {

enum class Variable { x, y, u, v };

class Program final {
public:
  explicit Program(std::string source);

  [[nodiscard]] const std::vector<std::string> &references() const noexcept;
  void bind(const std::function<std::size_t(std::string_view)> &resolver);
  [[nodiscard]] double evaluate(double x, double y, double u, double v,
                                const std::vector<double> &symbols) const;

public: // Internal bytecode representation used by the private parser.
  enum class Op {
    constant,
    x,
    y,
    u,
    v,
    symbol,
    add,
    sub,
    mul,
    div,
    power,
    neg,
    pos,
    function
  };
  enum class Function {
    sin,
    cos,
    tan,
    asin,
    acos,
    atan,
    atan2,
    exp,
    log,
    log10,
    sqrt,
    abs,
    floor,
    ceil,
    round,
    min,
    max,
    pow,
    clamp,
    smoothstep
  };
  struct Instruction {
    Op op{};
    double number{};
    std::size_t index{};
    Function function{};
    int arity{};
  };

private:
  std::vector<Instruction> code_;
  std::vector<std::string> references_;
};

} // namespace cropsim::expression
