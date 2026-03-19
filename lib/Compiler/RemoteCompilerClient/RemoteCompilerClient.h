#pragma once

#include "Compiler/CompilerServer/CompilerServer.h"
#include "Networking/Compilation.h"
#include "Networking/protocol.grpc.pb.h"

namespace transport {
class RemoteCompilerClient final : public CompilerServer {
public:
  explicit RemoteCompilerClient(const std::string &CompilerIP = "localhost");
  ConnectResponse connect(ConnectRequest Request) override;
  DisconnectResponse disconnect(DisconnectRequest Request) override;
  Response compile(Request Req) override;

private:
  std::shared_ptr<Compiler::Stub> Stub;
};
} // namespace transport