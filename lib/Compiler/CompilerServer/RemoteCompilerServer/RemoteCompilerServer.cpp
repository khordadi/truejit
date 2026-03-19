#include "RemoteCompilerServer.h"
#include "Compiler/CompilerServer/CompilerServer.h"
#include "Config.h"

#include <Networking/Compilation.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>

#ifdef TRACE_NETWORKING
#include "Logging.h"
#include <fmt/color.h>
static Logger Logger{"networking", fmt::color::gold};
auto ServerStartSS = SubScope{"server-start", fmt::color::light_blue};
auto ServerShutdownSS = SubScope{"server-shutdown", fmt::color::light_blue};
auto ConnectSS = SubScope{"connect", fmt::color::pink};
auto DisconnectSS = SubScope{"disconnect", fmt::color::pink};
auto GetSS = SubScope{"get", fmt::color::cyan};
auto HitSS = SubScope{"hit", fmt::color::dark_cyan};
auto ErrorSS = SubScope{"error", fmt::color::red};
#endif

namespace transport {
RemoteCompilerServer::RemoteCompilerServer(std::shared_ptr<CompilerServer> CS)
    : CS(std::move(CS)) {}

void RemoteCompilerServer::start(const std::string &CompilerIP) {
  auto CompilerAddress = CompilerIP + ":" + COMPILER_PORT;
#ifdef TRACE_NETWORKING
  Logger.log(ServerStartSS, fmt::format("{}", CompilerAddress));
#endif
  JITServer = std::shared_ptr(
      ServerBuilder{}
          .AddListeningPort(CompilerAddress, InsecureServerCredentials())
          .RegisterService(this)
          .BuildAndStart());
  JITServer->Wait();
}

void RemoteCompilerServer::shutdown() const {
  JITServer->Shutdown();
#ifdef TRACE_NETWORKING
  Logger.log(ServerShutdownSS, "");
#endif
}

Status RemoteCompilerServer::RemoteGet(ServerContext *SC,
                                       const NetCompilationRequest *Request,
                                       NetCompilationResponse *Response) {
  auto Req = New(*Request);
#ifdef TRACE_NETWORKING
  Logger.log(GetSS, fmt::format("#{}:{}", Req.SessionID, Req.RequestID));
#endif
  const auto Res = compile(Req);
  WriteTo(Res, Response);
  return Status::OK;
}

Status RemoteCompilerServer::RemoteConnect(ServerContext *SC,
                                           const NetConnectRequest *Request,
                                           NetConnectResponse *Response) {
  auto Req = New(*Request);
#ifdef TRACE_NETWORKING
  Logger.log(ConnectSS, "");
#endif
  const auto Res = connect(std::move(Req));
  WriteTo(Res, Response);
  return Status::OK;
}

Status
RemoteCompilerServer::RemoteDisconnect(ServerContext *SC,
                                       const NetDisconnectRequest *Request,
                                       NetDisconnectResponse *Response) {
  auto SessionID = Request->session_id();
#ifdef TRACE_NETWORKING
  Logger.log(DisconnectSS, fmt::format("#{}", SessionID));
#endif
  disconnect({.SessionID = SessionID});
  return Status::OK;
}

CompilerServer::ConnectResponse
RemoteCompilerServer::connect(ConnectRequest Request) {
  return CS->connect(std::move(Request));
}

CompilerServer::DisconnectResponse
RemoteCompilerServer::disconnect(DisconnectRequest Req) {
  return CS->disconnect(std::move(Req));
}

CompilerServer::Response RemoteCompilerServer::compile(Request Req) {
  return CS->compile(std::move(Req));
}
} // namespace transport