#include "Orchestrator/OrchestratorTransport.h"

// #define TRACE_ORC_TRANS

namespace transport {

void OrchestratorTransport::start(const string &IP) {
  const auto Address = IP + ":" + ORCHESTRATOR_PORT;
#ifdef TRACE_ORC_TRANS
  fmt::println("[orc-trans][start] {}", Address);
#endif

  ServerBuilder Builder;
  Builder.AddListeningPort(Address, InsecureServerCredentials());
  Builder.RegisterService(this);
  Builder.AddChannelArgument(GRPC_ARG_MAX_CONCURRENT_STREAMS, 2000);
  Builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS, 30 * 60 * 1000);
  Builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 10 * 60 * 1000);
  Builder.AddChannelArgument(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  Builder.AddChannelArgument(GRPC_ARG_HTTP2_MAX_PINGS_WITHOUT_DATA, 0);
  Builder.AddChannelArgument(
      GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 10 * 60 * 1000);
  S = Builder.BuildAndStart();
  S->Wait();
}

} // namespace transport