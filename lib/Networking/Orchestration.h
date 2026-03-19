#pragma once

#include "Networking/protocol.grpc.pb.h"
#include "Orchestrator/Message.h"
#include <grpcpp/create_channel.h>

using namespace grpc;

#define ORCHESTRATOR_PORT "50052"

namespace transport {

// Sync
using SyncMsg = networking::Sync;
orchestration::SyncMsg read(const SyncMsg *In);
void write(const orchestration::SyncMsg &Src, SyncMsg *Dest);

// Ack
using AckMsg = networking::Ack;
orchestration::AckMsg read(const AckMsg *In);
void write(const orchestration::AckMsg &Src, AckMsg *Dest);

// Command
using CommandMsg = networking::Command;
orchestration::CommandMsg read(const CommandMsg *In);
void write(const orchestration::CommandMsg &Src, CommandMsg *Dest);

// Result
using ResultMsg = networking::Result;
orchestration::ResultMsg read(const ResultMsg *In);
void write(const orchestration::ResultMsg &Src, ResultMsg *Dest);

// Empty
using EmptyMsg = google::protobuf::Empty;

using Orchestrator = networking::Orchestrator;
class OrchestratorProxy {
  std::unique_ptr<Orchestrator::Stub> Stub;

public:
  explicit OrchestratorProxy(const std::string &IP,
                             const std::string &Port = ORCHESTRATOR_PORT) {
    const auto Address = IP + ":" + Port;
    const auto Channel = CreateChannel(Address, InsecureChannelCredentials());
    Stub = Orchestrator::NewStub(Channel);
  }

  orchestration::AckMsg Connect(const orchestration::SyncMsg &Sync) const {
    ClientContext C{};
    SyncMsg Req;
    transport::write(Sync, &Req);
    AckMsg Res;
    if (const auto Status = Stub->Connect(&C, Req, &Res); not Status.ok()) {
      std::cerr << Status.error_message() << std::endl;
      Panic("Connect failed!");
    }
    return read(&Res);
  }

  void Disconnect(const orchestration::ResultMsg &Result) const {
    constexpr int MaxRetries = 5;
    auto BackoffDuration = seconds{1};
    for (int Attempt = 1; Attempt <= MaxRetries; ++Attempt) {
      ClientContext C{};
      ResultMsg Req;
      transport::write(Result, &Req);
      EmptyMsg Res;

      Status Status = Stub->Disconnect(&C, Req, &Res);
      if (Status.ok())
        return; // Success!

      std::cerr << "Attempt " << Attempt
                << " failed: " << Status.error_message() << std::endl;

      if (Attempt < MaxRetries) {
        std::cerr << "Retrying in " << BackoffDuration.count()
                  << " second(s)..." << std::endl;
        std::this_thread::sleep_for(BackoffDuration);
        BackoffDuration *= 2; // Double the wait time for the next attempt
      }
    }

    // If the loop finishes, all retries have failed.
    Panic("Disconnect failed after all retries!");
  }
};

using Worker = networking::Worker;

// Command

} // namespace transport