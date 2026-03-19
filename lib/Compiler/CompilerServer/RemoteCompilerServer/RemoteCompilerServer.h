#pragma once

#include "Compiler/CompilerServer/CompilerServer.h"
#include <Networking/Compilation.h>

using namespace grpc;
namespace transport {
class RemoteCompilerServer final : public Compiler::Service,
                                   public CompilerServer {
public:
  explicit RemoteCompilerServer(std::shared_ptr<CompilerServer> CS);

  Status RemoteConnect(ServerContext *SC, const NetConnectRequest *Request,
                       NetConnectResponse *Response) override;
  Status RemoteDisconnect(ServerContext *SC,
                          const NetDisconnectRequest *Request,
                          NetDisconnectResponse *Response) override;
  Status RemoteGet(ServerContext *SC, const NetCompilationRequest *Request,
                   NetCompilationResponse *Response) override;

  ConnectResponse connect(ConnectRequest Request) override;
  DisconnectResponse disconnect(DisconnectRequest Req) override;
  Response compile(Request Req) override;

  void start(const std::string &CompilerIP = "localhost");
  void shutdown() const;

private:
  std::shared_ptr<CompilerServer> CS;
  std::shared_ptr<Server> JITServer;
};

} // namespace transport