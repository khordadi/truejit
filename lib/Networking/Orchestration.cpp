#include "Orchestration.h"
#include "Orchestrator/Message.Serialization.h"

namespace transport {
orchestration::SyncMsg read(const SyncMsg *In) {
  orchestration::SyncMsg Out;
  Out.Address = In->address();
  return std::move(Out);
}
void write(const orchestration::SyncMsg &Src, SyncMsg *Dest) {
  Dest->set_address(Src.Address);
}

orchestration::AckMsg read(const AckMsg *In) {
  orchestration::AckMsg Out;
  Out.Connected = In->connected();
  return std::move(Out);
}
void write(const orchestration::AckMsg &Src, AckMsg *Dest) {
  Dest->set_connected(Src.Connected);
}

orchestration::ResultMsg read(const ResultMsg *In) {
  orchestration::ResultMsg O;
  deserialize(json::parse(In->payload()), O);
  return O;
}
void write(const orchestration::ResultMsg &Src, ResultMsg *Dest) {
  Dest->set_payload(serialize(Src).dump());
}

orchestration::CommandMsg read(const CommandMsg *In) {
  orchestration::CommandMsg O;
  deserialize(json::parse(In->payload()), O);
  return O;
}
void write(const orchestration::CommandMsg &Src, CommandMsg *Dest) {
  Dest->set_payload(serialize(Src).dump());
}
} // namespace transport