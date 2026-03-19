#pragma once
#include "Compiler/CompilerServer/CompilationService/CompilationService.Serialization.h"
#include "Message.h"
#include "VirtualMachine/VirtualMachine.Serialization.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace orchestration {
inline json serialize(const SyncMsg &O) {
  json J;
  J["Address"] = O.Address;
  return J;
}
inline void deserialize(const json &J, SyncMsg &O) { O.Address = J["Address"]; }

inline json serialize(const AckMsg &O) {
  json J;
  J["Connected"] = O.Connected;
  return J;
}
inline void deserialize(const json &J, AckMsg &O) {
  O.Connected = J.at("Connected").get<bool>();
}

inline json serialize(const ResultMsg &O) {
  json J;
  J["VMProf"] = serialize(O.VM);
  return J;
}
inline void deserialize(const json &J, ResultMsg &O) {
  deserialize(J.at("VMProf"), O.VM);
}

inline json serialize(const CommandMsg::CompilerOptions &O) {
  json J;
  J["CompilationService"] = serialize(O.CSO);
  J["Sync"] = sync_compilation::str(O.Sync);
  if (O.Async.has_value())
    J["Async"] = async_compilation::str(*(O.Async));
  J["RemoteCompilation"] = O.RemoteCompilation;
  return J;
}
inline void deserialize(const json &J, CommandMsg::CompilerOptions &O) {
  if (J.contains("CompilationService"))
    deserialize(J.at("CompilationService").get<json>(), O.CSO);
  if (J.contains("Sync"))
    O.Sync = sync_compilation::of(J.at("Sync").get<std::string>());
  if (J.contains("Async"))
    O.Async = async_compilation::of(J.at("Async").get<std::string>());
  O.RemoteCompilation = J.at("RemoteCompilation").get<bool>();
}
inline uint64_t serialize(const CommandMsg::TimePoint &O) {
  return O.time_since_epoch();
}
inline void deserialize(const json &J, CommandMsg::TimePoint &O) {
  const auto Nanos = J.at("TimePoint").get<unsigned long>();
  O.Time = system_clock::time_point(nanoseconds(Nanos));
}
inline json serialize(const CommandMsg &O) {
  json J;
  J["VirtualMachine"] = serialize(O.VMO);
  J["Wakeup"] = serialize(O.Wakeup);
  J["Compiler"] = serialize(O.CO);
  return J;
}
inline void deserialize(const json &J, CommandMsg &O) {
  if (J.contains("VirtualMachine"))
    deserialize(J.at("VirtualMachine"), O.VMO);
  if (J.contains("Wakeup"))
    deserialize(J.at("Wakeup"), O.Wakeup);
  if (J.contains("Compiler"))
    deserialize(J.at("Compiler"), O.CO);
}

} // namespace orchestration