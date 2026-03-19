#pragma once

#include "Networking/Orchestration.h"
#include "Orchestrator.h"
#include <grpcpp/grpcpp.h>

using namespace grpc;

namespace transport {

class OrchestratorTransport final : public Orchestrator::Service {
  std::shared_ptr<Server> S;
  orchestration::Orchestrator &Orc;

public:
  explicit OrchestratorTransport(orchestration::Orchestrator &Orc) : Orc(Orc) {}

  Status Connect(ServerContext *SC, const SyncMsg *Req, AckMsg *Res) override {
    const auto Ack = Orc.Connect(read(Req));
    write(Ack, Res);
    return Status::OK;
  }

  Status Disconnect(ServerContext *SC, const ResultMsg *Req,
                    EmptyMsg *Res) override {
    Orc.Disconnect(read(Req));
    return Status::OK;
  }

  void start(const std::string &IP = "0.0.0.0");
  void stop() const { S->Shutdown(); }
};

} // namespace transport