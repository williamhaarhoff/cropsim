#include "expression.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace cropsim::expression {
namespace {
constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double e = 2.718281828459045235360287471352662498;
constexpr std::size_t max_source_size = 4096U;
constexpr std::size_t max_instructions = 1024U;
constexpr std::size_t max_nesting = 64U;

class Parser final {
public:
  explicit Parser(std::string source) : source_(std::move(source)) {
    if (source_.size() > max_source_size) {
      throw std::invalid_argument(
          "modifier expression exceeds 4096 characters");
    }
  }

  Program parse() = delete;
  std::vector<Program::Instruction> parse_code() {
    expression();
    skip();
    if (position_ != source_.size())
      fail("unexpected token");
    return code_;
  }
  const std::vector<std::string> &references() const noexcept {
    return references_;
  }

private:
  using Op = Program::Op;
  using Function = Program::Function;
  using Instruction = Program::Instruction;

  [[noreturn]] void fail(const char *message) const {
    throw std::invalid_argument(std::string("modifier expression: ") + message);
  }
  void skip() {
    while (position_ < source_.size() &&
           std::isspace(static_cast<unsigned char>(source_[position_]))) {
      ++position_;
    }
  }
  bool take(char value) {
    skip();
    if (position_ < source_.size() && source_[position_] == value) {
      ++position_;
      return true;
    }
    return false;
  }
  void enter() {
    if (++depth_ > max_nesting)
      fail("nesting exceeds 64");
  }
  void leave() { --depth_; }
  void emit(Instruction instruction) {
    code_.push_back(instruction);
    if (code_.size() > max_instructions)
      fail("expression exceeds 1024 instructions");
  }
  void expression() {
    enter();
    term();
    while (true) {
      if (take('+')) {
        term();
        emit({Op::add});
      } else if (take('-')) {
        term();
        emit({Op::sub});
      } else
        break;
    }
    leave();
  }
  void term() {
    unary();
    while (true) {
      if (take('*')) {
        unary();
        emit({Op::mul});
      } else if (take('/')) {
        unary();
        emit({Op::div});
      } else
        break;
    }
  }
  void unary() {
    if (take('+')) {
      unary();
      emit({Op::pos});
    } else if (take('-')) {
      unary();
      emit({Op::neg});
    } else
      power();
  }
  void power() {
    primary();
    if (take('^')) {
      unary();
      emit({Op::power});
    }
  }
  std::string identifier() {
    skip();
    const auto start = position_;
    if (position_ >= source_.size() ||
        !(std::isalpha(static_cast<unsigned char>(source_[position_])) ||
          source_[position_] == '_')) {
      fail("expected identifier");
    }
    while (position_ < source_.size() &&
           (std::isalnum(static_cast<unsigned char>(source_[position_])) ||
            source_[position_] == '_'))
      ++position_;
    return source_.substr(start, position_ - start);
  }
  void primary();

  std::string source_;
  std::size_t position_{};
  std::size_t depth_{};
  std::vector<Instruction> code_;
  std::vector<std::string> references_;
};

void Parser::primary() {
  skip();
  if (take('(')) {
    expression();
    if (!take(')'))
      fail("missing closing parenthesis");
    return;
  }
  if (position_ < source_.size() &&
      (std::isdigit(static_cast<unsigned char>(source_[position_])) ||
       source_[position_] == '.')) {
    std::size_t used{};
    const auto value = std::stod(source_.substr(position_), &used);
    if (!std::isfinite(value))
      fail("number must be finite");
    position_ += used;
    emit({Op::constant, value});
    return;
  }
  const auto name = identifier();
  if (take('(')) {
    int arity = 0;
    if (!take(')')) {
      do {
        expression();
        ++arity;
      } while (take(','));
      if (!take(')'))
        fail("missing function parenthesis");
    }
    const std::array<std::pair<const char *, Function>, 19> functions{
        {{"sin", Function::sin},
         {"cos", Function::cos},
         {"tan", Function::tan},
         {"asin", Function::asin},
         {"acos", Function::acos},
         {"atan", Function::atan},
         {"atan2", Function::atan2},
         {"exp", Function::exp},
         {"log", Function::log},
         {"log10", Function::log10},
         {"sqrt", Function::sqrt},
         {"abs", Function::abs},
         {"floor", Function::floor},
         {"ceil", Function::ceil},
         {"round", Function::round},
         {"min", Function::min},
         {"max", Function::max},
         {"pow", Function::pow},
         {"clamp", Function::clamp}}};
    const auto found =
        std::find_if(functions.begin(), functions.end(),
                     [&](const auto &item) { return name == item.first; });
    Function function{};
    if (found != functions.end())
      function = found->second;
    else if (name == "smoothstep")
      function = Function::smoothstep;
    else
      fail("unknown function");
    const auto expected =
        function == Function::atan2 || function == Function::min ||
                function == Function::max || function == Function::pow
            ? 2
        : function == Function::clamp || function == Function::smoothstep ? 3
                                                                          : 1;
    if (arity != expected)
      fail("wrong function argument count");
    emit({Op::function, 0.0, 0U, function, arity});
    return;
  }
  if (name == "x")
    emit({Op::x});
  else if (name == "y")
    emit({Op::y});
  else if (name == "u")
    emit({Op::u});
  else if (name == "v")
    emit({Op::v});
  else if (name == "pi")
    emit({Op::constant, pi});
  else if (name == "e")
    emit({Op::constant, e});
  else {
    auto found = std::find(references_.begin(), references_.end(), name);
    if (found == references_.end()) {
      references_.push_back(name);
      found = references_.end() - 1;
    }
    emit({Op::symbol, 0.0,
          static_cast<std::size_t>(found - references_.begin())});
  }
}
} // namespace

Program::Program(std::string source) {
  Parser parser(std::move(source));
  code_ = parser.parse_code();
  references_ = parser.references();
}

const std::vector<std::string> &Program::references() const noexcept {
  return references_;
}

void Program::bind(
    const std::function<std::size_t(std::string_view)> &resolver) {
  for (auto &instruction : code_) {
    if (instruction.op == Op::symbol) {
      instruction.index = resolver(references_.at(instruction.index));
    }
  }
}

double Program::evaluate(double x, double y, double u, double v,
                         const std::vector<double> &symbols) const {
  std::array<double, max_instructions + 1U> stack{};
  std::size_t size = 0;
  const auto push = [&](double value) { stack[size++] = value; };
  const auto pop = [&]() { return stack[--size]; };
  for (const auto &instruction : code_) {
    switch (instruction.op) {
    case Op::constant:
      push(instruction.number);
      break;
    case Op::x:
      push(x);
      break;
    case Op::y:
      push(y);
      break;
    case Op::u:
      push(u);
      break;
    case Op::v:
      push(v);
      break;
    case Op::symbol:
      push(symbols[instruction.index]);
      break;
    case Op::add: {
      const auto b = pop();
      const auto a = pop();
      push(a + b);
      break;
    }
    case Op::sub: {
      const auto b = pop();
      const auto a = pop();
      push(a - b);
      break;
    }
    case Op::mul: {
      const auto b = pop();
      const auto a = pop();
      push(a * b);
      break;
    }
    case Op::div: {
      const auto b = pop();
      const auto a = pop();
      push(a / b);
      break;
    }
    case Op::power: {
      const auto b = pop();
      const auto a = pop();
      push(std::pow(a, b));
      break;
    }
    case Op::neg:
      stack[size - 1U] = -stack[size - 1U];
      break;
    case Op::pos:
      break;
    case Op::function: {
      const auto c = pop();
      double result{};
      if (instruction.arity == 1) {
        switch (instruction.function) {
        case Function::sin:
          result = std::sin(c);
          break;
        case Function::cos:
          result = std::cos(c);
          break;
        case Function::tan:
          result = std::tan(c);
          break;
        case Function::asin:
          result = std::asin(c);
          break;
        case Function::acos:
          result = std::acos(c);
          break;
        case Function::atan:
          result = std::atan(c);
          break;
        case Function::exp:
          result = std::exp(c);
          break;
        case Function::log:
          result = std::log(c);
          break;
        case Function::log10:
          result = std::log10(c);
          break;
        case Function::sqrt:
          result = std::sqrt(c);
          break;
        case Function::abs:
          result = std::abs(c);
          break;
        case Function::floor:
          result = std::floor(c);
          break;
        case Function::ceil:
          result = std::ceil(c);
          break;
        case Function::round:
          result = std::round(c);
          break;
        default:
          break;
        }
      } else {
        const auto b = pop();
        if (instruction.arity == 2) {
          switch (instruction.function) {
          case Function::atan2:
            result = std::atan2(b, c);
            break;
          case Function::min:
            result = std::min(b, c);
            break;
          case Function::max:
            result = std::max(b, c);
            break;
          case Function::pow:
            result = std::pow(b, c);
            break;
          default:
            break;
          }
        } else {
          const auto a = pop();
          if (instruction.function == Function::clamp)
            result = std::clamp(a, b, c);
          else {
            const auto t = std::clamp((c - a) / (b - a), 0.0, 1.0);
            result = t * t * (3.0 - 2.0 * t);
          }
        }
      }
      push(result);
      break;
    }
    }
  }
  if (size != 1U || !std::isfinite(stack[0])) {
    throw std::runtime_error(
        "modifier expression produced a non-finite result");
  }
  return stack[0];
}

} // namespace cropsim::expression
