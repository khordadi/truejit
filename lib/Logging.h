#pragma once

#include <fmt/color.h>
#include <utility>

struct SubScope {
  std::string SubScopePrefix;
  fmt::color SubScopeColor;
};

class Logger final {
public:
  Logger(std::string ScopePrefix, fmt::color ScopeColor)
      : ScopePrefix(std::move(ScopePrefix)), ScopeColor(ScopeColor) {}

  void log(const SubScope &SS, const std::string &Message = "") const {
    auto Str = format(fg(ScopeColor), "[{}]", ScopePrefix);
    Str +=
        format(fg(SS.SubScopeColor), "[{}] {}\n", SS.SubScopePrefix, Message);
    std::cout << Str << std::flush;
  }

private:
  const std::string ScopePrefix;
  const fmt::color ScopeColor;
};