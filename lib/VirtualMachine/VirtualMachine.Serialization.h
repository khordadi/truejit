#pragma once
#include "VirtualMachine.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

inline json serialize(const VirtualMachine::Options::Application &O) {
  json J;
  J["PreOpens"] = O.PreOpens;
  J["Args"] = O.Args;
  return J;
}

inline void deserialize(const json &J,
                        VirtualMachine::Options::Application &O) {
  O.Args = J.at("Args").get<std::vector<std::string>>();
  if (J.contains("PreOpens"))
    O.PreOpens = J.at("PreOpens").get<std::vector<std::string>>();
}

inline json serialize(const VirtualMachine::Options &O) {
  json J;
  J["Application"] = serialize(O.App);
  if (O.CacheLimit.has_value())
    J["CacheLimit"] = O.CacheLimit.value();
  return J;
}

inline void deserialize(const json &J, VirtualMachine::Options &O) {
  if (J.contains("Application"))
    deserialize(J.at("Application").get<json>(), O.App);
  if (J.contains("CacheLimit"))
    O.CacheLimit = J.at("CacheLimit").get<int>();
}

inline json serialize(const VirtualMachine::Profiler &O) {
  json J;
  J["init-start"] = O.InitStart;
  J["init-end"] = O.InitEnd;
  J["static-start"] = O.StaticStart;
  J["static-end"] = O.StaticEnd;
  J["start"] = O.Start;
  J["end"] = O.End;
  J["times"] = json::array();
  for (const auto &[Fn, Start, End, _] : O.Records)
    J["times"].push_back({Fn, Start, End});
  J["waiting"] = O.Waiting.load();
  auto E2E = O.End - O.Start;
  J["e2e"] = E2E;
  auto Exec = E2E - O.Waiting;
  J["exec"] = Exec;
  return J;
}

inline void deserialize(const json &J, VirtualMachine::Profiler &O) {
  O.InitStart = J.at("init-start").get<u64>();
  O.InitEnd = J.at("init-end").get<u64>();
  O.StaticStart = J.at("static-start").get<u64>();
  O.StaticEnd = J.at("static-end").get<u64>();
  O.Start = J.at("start").get<u64>();
  O.End = J.at("end").get<u64>();
  for (const auto &JJ : J.at("times")) {
    auto Fn = JJ.at(0).get<u32>();
    auto Start = JJ.at(1).get<u64>();
    auto End = JJ.at(2).get<u64>();
    O.Records.emplace_back(Fn, Start, End);
  }
  O.Waiting = J.at("waiting").get<u64>();
}