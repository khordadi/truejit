#pragma once
#include <nlohmann/json.hpp>
using json = nlohmann::json;

inline std::string
serialize(const enum CompilationService::Options::CompilationOptions::Tier &O) {
  return O == CompilationService::Options::CompilationOptions::Tier::LLVM
             ? "llvm"
             : "clift";
}

inline void
deserialize(const std::string &Str,
            enum CompilationService::Options::CompilationOptions::Tier &O) {
  if (Str == "llvm")
    O = CompilationService::Options::CompilationOptions::Tier::LLVM;
  else if (Str == "clift")
    O = CompilationService::Options::CompilationOptions::Tier::Cranelift;
  else
    Panic("Unknown compiler tier");
}

inline json
serialize(const CompilationService::Options::CompilationOptions &O) {
  json J;
  J["Compiler"] = serialize(O.Tier);
  J["Optimization"] = O.DefaultOptimizations.Level;
  J["Extensions"] = O.DefaultOptimizations.Extensions;
  return J;
}

inline void deserialize(const json &J,
                        CompilationService::Options::CompilationOptions &O) {
  O.Tier = CompilationService::Options::CompilationOptions::Tier::LLVM;
  O.DefaultOptimizations.Level = 3;
  O.DefaultOptimizations.Extensions = true;
  if (J.contains("Compiler"))
    deserialize(J.at("Compiler").get<std::string>(), O.Tier);
  if (J.contains("Optimization"))
    O.DefaultOptimizations.Level = J.at("Optimization").get<int>();
  if (J.contains("Extensions"))
    O.DefaultOptimizations.Extensions = J.at("Extensions").get<bool>();
}

inline json serialize(const CompilationService::Options::CachingOptions &O) {
  json J;
  if (O.Limit.has_value())
    J["Limit"] = O.Limit.value();
  return J;
}

inline void deserialize(const json &J,
                        CompilationService::Options::CachingOptions &O) {
  if (J.contains("Limit"))
    O.Limit = J.at("Limit").get<int>();
}

inline json serialize(const CompilationService::Options &O) {
  json J;
  J["CompilationOptions"] = serialize(O.CompilationOpts);
  J["CachingOptions"] = serialize(O.CachingOpts);
  return J;
}

inline void deserialize(const json &J, CompilationService::Options &O) {
  if (J.contains("CompilationOptions"))
    deserialize(J.at("CompilationOptions").get<json>(), O.CompilationOpts);
  if (J.contains("CachingOptions"))
    deserialize(J.at("CachingOptions").get<json>(), O.CachingOpts);
}