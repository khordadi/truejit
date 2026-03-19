#include "Orchestrator/Orchestrator.h"
#include "Compiler/CompilerServer/RemoteCompilerServer/RemoteCompilerServer.h"
#include "Networking/Orchestration.h"
#include "Orchestrator/Message.Serialization.h"
#include "OrchestratorTransport.h"
#include <nlohmann/json.hpp>
#include <random>
#include <ranges>
#include <thread>

std::string Now() {
  const auto Now = system_clock::to_time_t(system_clock::now());
  std::ostringstream Stream;
  Stream << std::put_time(std::localtime(&Now), "%H:%M:%S");
  return Stream.str();
}

#define ENABLE_LOG
#ifdef ENABLE_LOG
void log(const std::string &Msg) {
  fmt::println("[{}][orc] {}", Now(), Msg);
  std::cout.flush();
}
#define LOG(MSG) log(MSG)

#else
#define LOG(FMT, ...)
#endif

template <typename T> T average(const std::vector<T> &Vec) {
  if (Vec.empty())
    return 0.0;
  T Sum = std::accumulate(Vec.begin(), Vec.end(), T{});
  return Sum / Vec.size();
}

namespace orchestration {

class IntegratedCompiler {
  std::shared_ptr<transport::RemoteCompilerServer> Compiler;
  std::thread Thread;

public:
  explicit IntegratedCompiler(auto &&CO, std::string CompilerIP = "0.0.0.0") {
    auto [CSO, Sync, Async, RemoteCompiler] = std::move(CO);

    // WasmService
    auto WS = std::make_shared<WasmService>();

    // CompilerService
    auto CompilerService = std::make_shared<CompilationService>(WS, CSO);

    // PredictionService
    auto PS = std::make_shared<PredictionService>();

    // SyncService
    // fmt::println("[sync] {}", sync_compilation::str(Sync));
    auto SyncService = std::make_shared<sync_compilation::Service>(
        Sync, WS, CompilerService, PS);

    // AsyncService
    std::optional<std::shared_ptr<async_compilation::Service>> AsyncService;
    if (Async.has_value()) {
      // fmt::println("[async] {}", async_compilation::str(*Async));
      AsyncService = std::optional{std::make_shared<async_compilation::Service>(
          *Async, WS, CompilerService)};
    }

    // CompilerServer
    auto CS = std::make_shared<CompilerServer>(SyncService, AsyncService);
    auto Latency = std::optional<int>{};
    if (Latency.has_value())
      CS = std::make_shared<DelayedCompilerServer>(
          CS, DelayedCompilerServer::Options{*Latency});

    Compiler = std::make_shared<transport::RemoteCompilerServer>(CS);

    Thread = std::thread([this, CompilerIP] { Compiler->start(CompilerIP); });
    LOG(fmt::format("compiler started at {}", CompilerIP));
  }

  ~IntegratedCompiler() {
    Compiler->shutdown();
    Thread.join();
    LOG("compiler stopped");
  }
};

Orchestrator::Orchestrator(Options &&Opts) : Opts(std::move(Opts)) {}

void Orchestrator::start() {
  std::unique_lock ConnLock(AllConnectedMutex);
  AllConnectedCV.wait(
      ConnLock, [this] { return Connections.size() == Opts.ClusterSize; });
  LOG(fmt::format("all connected ({} workers)", Connections.size()));
  LOG(fmt::format("command {}", serialize(Opts.Command).dump()));

  auto Compiler = Opts.Command.CO.RemoteCompilation
                      ? std::optional<IntegratedCompiler>(Opts.Command.CO)
                      : std::nullopt;

  for (auto I = 0; I < Connections.size(); ++I) {
    const auto &Address = Connections[I];
    const auto Channel = CreateChannel(Address, InsecureChannelCredentials());
    const auto Stub = transport::Worker::NewStub(Channel);

    CommandMsg Msg{Opts.Command};

    // Wakeup time
    CommandMsg::TimePoint Wakeup{}; // now
    Wakeup.add(Opts.Wakeup);        // += wakeup delay
    if (Opts.Interval)              // += interval (if specified)
      Wakeup.add(I * milliseconds{*Opts.Interval});
    Msg.Wakeup = std::move(Wakeup);

    ClientContext C{};
    transport::CommandMsg Cmd;
    transport::EmptyMsg Res{};
    transport::write(std::move(Msg), &Cmd);
    Stub->Execute(&C, Cmd, &Res);
  }
  ConnLock.unlock();

  std::unique_lock DisconnLock(ResultsMutex);
  ResultsCV.wait(DisconnLock,
                 [this] { return Results.size() == Opts.ClusterSize; });
  LOG(fmt::format("all disconnected ({} results)", Results.size()));

  std::vector<unsigned long> WaitingTimes;
  std::vector<unsigned long> E2Es;
  std::vector<unsigned long> Execs;

  for (auto &[VM] : Results) {
    const auto E2E = VM.End - VM.Start;
    E2Es.push_back(E2E);
    WaitingTimes.push_back(VM.Waiting);
    Execs.push_back(E2E - VM.Waiting);
  }

  const auto Waiting = average(WaitingTimes);
  const auto E2E = average(E2Es);
  const auto Exec = average(Execs);

  LOG(fmt::format("waiting={:.2f} e2e={:.2f} exec={:.2f} (seconds)",
                  Waiting / 1e9, E2E / 1e9, Exec / 1e9));

  metric("waiting", std::to_string(Waiting));
  metric("e2e", std::to_string(E2E));
  metric("exec", std::to_string(Exec));

  DisconnLock.unlock();
}

AckMsg Orchestrator::Connect(SyncMsg &&In) {
  std::unique_lock Lock(AllConnectedMutex);
  if (Connections.size() == Opts.ClusterSize)
    return AckMsg{false}; // don't accept new connections

  auto [Address] = std::move(In);

  Connections.push_back(Address);
  LOG(fmt::format("worker#{} {}", Connections.size(), Address));

  if (Connections.size() == Opts.ClusterSize) {
    Lock.unlock();
    AllConnectedCV.notify_one();
  }

  return AckMsg{true};
}
void Orchestrator::Disconnect(ResultMsg &&In) {
  std::unique_lock Lock(ResultsMutex);
  // TODO: maybe add address to disconnect message too?!

  if (Results.size() >= Opts.ClusterSize)
    Panic(fmt::format("too many results received: {} >= {}", Results.size(),
                      Opts.ClusterSize));

  Results.push_back(std::move(In));
  LOG(fmt::format("result#{}", Results.size()));

  if (Results.size() == Opts.ClusterSize) {
    Lock.unlock();
    ResultsCV.notify_one();
  }
}
} // namespace orchestration
