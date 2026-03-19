#pragma once

#include "SyncCompilation/SyncCompilation.h"
#include <Globals.h>
#include <atomic>
#include <cassert>
#include <memory>
#include <utility>

namespace async_compilation {
class Service;
}
struct Session;
namespace sync_compilation {
class Service;
}
class AsyncCompiler;
class CompilationService;

#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace std::chrono;

class PredictionService;
class WasmService;
class CompilerServer {
public:
  struct Function {
    u32 Fn{};
    std::vector<char> Code;
  };
  struct Request {
    u32 SessionID{};
    u32 RequestID{};
    std::vector<u32> History;
    char *ActivationFrame{};
  };

  struct Response {
    std::vector<sync_compilation::Response> Responses;
  };

  struct ConnectRequest {
    std::string Application;
  };
  struct ConnectResponse {
    u32 SessionID{};
  };
  struct DisconnectRequest {
    u32 SessionID{};
  };
  struct DisconnectResponse {};

  explicit CompilerServer(
      std::shared_ptr<sync_compilation::Service> SyncService,
      std::optional<std::shared_ptr<async_compilation::Service>> AsyncService);
  CompilerServer() = default;
  virtual ~CompilerServer() = default;

  virtual ConnectResponse connect(ConnectRequest Request);
  virtual DisconnectResponse disconnect(DisconnectRequest Req);
  virtual Response compile(Request Request);

private:
  std::shared_ptr<sync_compilation::Service> SyncService;
  std::optional<std::shared_ptr<async_compilation::Service>> AsyncService;

  std::vector<std::shared_ptr<Session>> Sessions;
  Lock SessionsMutex;

  std::shared_ptr<Session> session(const u32 SessionID) {
    ReadLock RL(SessionsMutex);
    assert(SessionID < Sessions.size() and "Invalid session ID");
    return Sessions[SessionID];
  }
};

class DelayedCompilerServer final : public CompilerServer {
public:
  struct Options {
    int NetworkLatency;
  };

  explicit DelayedCompilerServer(std::shared_ptr<CompilerServer> CS,
                                 const Options Opts)
      : CS(std::move(CS)), NetworkLatency(Opts.NetworkLatency) {}

  ConnectResponse connect(ConnectRequest Req) override {
    std::this_thread::sleep_for(milliseconds(NetworkLatency));
    const auto Response = CS->connect(std::move(Req));
    std::this_thread::sleep_for(milliseconds(NetworkLatency));
    return Response;
  }
  DisconnectResponse disconnect(DisconnectRequest Req) override {
    std::this_thread::sleep_for(milliseconds(NetworkLatency));
    const auto Response = CS->disconnect(std::move(Req));
    std::this_thread::sleep_for(milliseconds(NetworkLatency));
    return Response;
  }
  Response compile(Request Req) override {
    std::this_thread::sleep_for(milliseconds(NetworkLatency));
    auto Response = CS->compile(std::move(Req));
    std::this_thread::sleep_for(milliseconds(NetworkLatency));
    return Response;
  }

private:
  std::shared_ptr<CompilerServer> CS;
  unsigned int NetworkLatency;
};