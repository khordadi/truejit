#pragma once

inline json serialize(const CompilerClient::Options &O) {
  json J;
  if (O.CacheLimit.has_value())
    J["CacheLimit"] = O.CacheLimit.value();
  return J;
}
inline void deserialize(const json &J, CompilerClient::Options &O) {
  if (J.contains("CacheLimit"))
    O.CacheLimit = J.at("CacheLimit").get<int>();
}