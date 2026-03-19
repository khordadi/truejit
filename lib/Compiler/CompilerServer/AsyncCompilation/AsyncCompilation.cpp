#include "AsyncCompilation.h"

namespace async_compilation {
Strategy of(std::string Str) {
  if (Str == "all")
    return All{};
  if (Str == "planned")
    return Planned{};
  if (Str == "dynamic")
    return Dynamic{};
  if (Str == "static")
    return Static{};
  Panic(fmt::format("Invalid async strategy '{}'", Str));
}
std::string str(Strategy S) {
  struct {
    std::string operator()(const All &) const { return "all"; }
    std::string operator()(const Planned &) const { return "planned"; }
    std::string operator()(const Dynamic &) const { return "dynamic"; }
    std::string operator()(const Static &) const { return "static"; }
  } Visitor;
  return std::visit(Visitor, S);
}
}; // namespace async_compilation