#include "Compiler/CompilerServer/CompilerServer.h"
#include "AsyncCompilation/AsyncCompilation.h"
#include "CompilationService/CompilationService.h"
#include "Compiler/CompilerServer/Predictor/PredictionService.h"
#include "Config.h"
#include "Globals.h"
#include "SyncCompilation/SyncCompilation.h"
#include <VirtualMachine/VirtualMachine.h>
#include <memory>
#include <numeric>
#include <utility>

struct Session {
  sync_compilation::CompileFn SyncCompile;
  std::shared_ptr<async_compilation::AsyncCompiler> AsyncCompiler;
};

CompilerServer::CompilerServer(
    std::shared_ptr<sync_compilation::Service> SyncService,
    std::optional<std::shared_ptr<async_compilation::Service>> AsyncService)
    : SyncService(std::move(SyncService)),
      AsyncService(std::move(AsyncService)) {}

CompilerServer::ConnectResponse
CompilerServer::connect(ConnectRequest Request) {
  const auto [Application] = std::move(Request);

  auto SyncCompile = (*SyncService)(Application);

  std::shared_ptr<async_compilation::AsyncCompiler> AsyncCompiler;
  if (AsyncService.has_value())
    AsyncCompiler = (**AsyncService)(Application);

  const auto S = std::make_shared<Session>(SyncCompile, AsyncCompiler);
  u32 SessionID;
  {
    WriteLock WL(SessionsMutex);
    SessionID = Sessions.size();
    Sessions.push_back(S);
  }
  return {SessionID};
}

CompilerServer::DisconnectResponse
CompilerServer::disconnect(DisconnectRequest Req) {
  const auto [SessionID] = Req;

  if (AsyncService.has_value()) {
    std::shared_ptr<Session> S;
    {
      ReadLock RL(SessionsMutex);
      S = Sessions[SessionID];
    }
    auto [_, AsyncCompiler] = *S;
    AsyncCompiler->stop();
  }

  return {};
}

CompilerServer::Response CompilerServer::compile(Request Request) {
  auto [SessionID, RequestID, History, ActivationFrame] = std::move(Request);

  auto [SyncCompile, AsyncCompiler] = *session(SessionID);

  if (AsyncService.has_value())
    AsyncCompiler->push(History.back());

  const auto Out = SyncCompile({History, ActivationFrame});

  return {Out};
}
