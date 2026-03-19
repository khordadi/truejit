#include "RemoteCompilerClient.h"
#include "Config.h"
#include "Networking/Compilation.h"
#include <grpcpp/create_channel.h>

// #define TRACE_NETWORKING
#ifdef TRACE_NETWORKING
#include "Logging.h"
static Logger Logger{"networking", fmt::color::gold};
auto ServerStartSS = SubScope{"channel-creation", fmt::color::light_blue};
auto ConnectSS = SubScope{"connect", fmt::color::pink};
auto DisconnectSS = SubScope{"disconnect", fmt::color::pink};
auto GetSS = SubScope{"get", fmt::color::cyan};
auto HitSS = SubScope{"hit", fmt::color::dark_cyan};
auto ErrorSS = SubScope{"error", fmt::color::red};
#endif

namespace transport {
RemoteCompilerClient::RemoteCompilerClient(const std::string &CompilerIP) {
  auto CompilerAddress = CompilerIP + ":" + COMPILER_PORT;
  Stub = Compiler::NewStub(
      CreateChannel(CompilerAddress, InsecureChannelCredentials()));
#ifdef TRACE_NETWORKING
  Logger.log(ServerStartSS, fmt::format("{}", CompilerAddress));
#endif
}

CompilerServer::ConnectResponse
RemoteCompilerClient::connect(ConnectRequest Request) {
  ClientContext C;
  NetConnectResponse Response;
  if (const auto Status = Stub->RemoteConnect(&C, New(Request), &Response);
      not Status.ok()) {
#ifdef TRACE_NETWORKING
    Logger.log(ErrorSS, fmt::format("{}", Status.error_message()));
#endif
  }
#ifdef TRACE_NETWORKING
  Logger.log(ConnectSS, fmt::format("#{}", Response.session_id()));
#endif
  return New(Response);
}
CompilerServer::DisconnectResponse
RemoteCompilerClient::disconnect(DisconnectRequest Request) {
#ifdef TRACE_NETWORKING
  Logger.log(DisconnectSS, fmt::format("#{}", Request.SessionID));
#endif
  ClientContext C;
  NetDisconnectResponse Response;
  if (const auto Status = Stub->RemoteDisconnect(&C, New(Request), &Response);
      not Status.ok()) {
#ifdef TRACE_NETWORKING
    Logger.log(ErrorSS, fmt::format("{}", Status.error_message()));
#endif
  }
  return New(Response);
}

CompilerServer::Response RemoteCompilerClient::compile(Request Req) {
#ifdef TRACE_NETWORKING
  Logger.log(GetSS, fmt::format("#{}", Req.RequestID));
#endif
  ClientContext C;

  NetCompilationResponse Response;
  if (const auto Status = Stub->RemoteGet(&C, New(Req), &Response);
      not Status.ok()) {
#ifdef TRACE_NETWORKING
    Logger.log(ErrorSS, fmt::format("{}", Status.error_message()));
#endif
    Panic(fmt::format("Network Error: {}", Status.error_message()));
  }
  return New(Response);
}
} // namespace transport