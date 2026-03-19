#include "Compiler/RemoteCompilerClient/RemoteCompilerClient.h"
#include "Config.h"
#include "Globals.h"
#include "Networking/Orchestration.h"
#include <arpa/inet.h>
#include <boost/program_options.hpp>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <netdb.h>
#include <nlohmann/json.hpp>
#include <thread>

#define TRACE_WORKER

#define LOG(MSG)                                                               \
  do {                                                                         \
    std::cout << "[worker] " << MSG << std::endl;                              \
    std::cout.flush();                                                         \
  } while (0)

static std::string GetHostIP() {
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) != 0)
    Panic("Could not get hostname.");

  addrinfo hints = {};
  hints.ai_family = AF_INET;
  addrinfo *result;
  if (getaddrinfo(hostname, NULL, &hints, &result) != 0)
    Panic("Could not find host IP address.");

  char IP[INET_ADDRSTRLEN];
  const auto addr = (sockaddr_in *)result->ai_addr;
  inet_ntop(AF_INET, &addr->sin_addr, IP, sizeof(IP));
  freeaddrinfo(result);
  return std::string(IP);
}

using namespace std::chrono;
using namespace grpc;
namespace po = boost::program_options;

namespace transport {
class WorkerTransport final : public Worker::Service {
  std::unique_ptr<Server> S;
  using OnWorkAsyncFn = std::function<void(orchestration::CommandMsg)>;
  OnWorkAsyncFn OnWorkAsync;

public:
  explicit WorkerTransport(const std::string &Address) {
    ServerBuilder Builder;
    Builder.AddListeningPort(Address, InsecureServerCredentials());
    Builder.RegisterService(this);
    S = Builder.BuildAndStart();
  }
  ~WorkerTransport() override {}

  void start() const {
    LOG("ready to receive commands");
    S->Wait();
  }
  void stop() const {
    S->Shutdown();
    LOG("receiver stopped");
  }

  void onWorkAsync(OnWorkAsyncFn Callback) {
    OnWorkAsync = std::move(Callback);
  }

  Status Execute(ServerContext *C, const CommandMsg *Req,
                 EmptyMsg *Res) override {
    OnWorkAsync(read(Req));
    return Status::OK;
  }
};
} // namespace transport

class Worker {
public:
  struct Options {
    std::string IP;
    std::string Port;
    std::string OrcIP;
  };

private:
  transport::OrchestratorProxy OrcProxy;
  std::string Address;

public:
  explicit Worker(Options &&Opts)
      : OrcProxy(Opts.OrcIP),
        Address(fmt::format("{}:{}", Opts.IP, Opts.Port)) {
    LOG(fmt::format("started @ {}", Address));
  }

  void connect() const {
    while (true) {
      if (const auto [Connected] =
              OrcProxy.Connect(orchestration::SyncMsg{Address});
          Connected) {
        LOG(fmt::format("connected"));
        break;
      }
      LOG(fmt::format("waiting to connect, retrying in 10 second..."));
      std::this_thread::sleep_for(seconds(10));
    }
  }

  void disconnect(orchestration::ResultMsg &&Result) const {
    OrcProxy.Disconnect(std::move(Result));
    LOG(fmt::format("disconnected"));
  }

  static orchestration::ResultMsg work(const orchestration::CommandMsg &Cmd,
                                       const std::string &CompilerIP) {
    auto &[VMO, CO, Wakeup] = Cmd;

    auto &[App, VMCacheLimit, Interp] = VMO;

    // FIXME: hack! send cwd in the control message
    if (auto &[PreOpens, Args] = App; not PreOpens.empty())
      std::filesystem::current_path(PreOpens.back());
    else
      current_path(std::filesystem::path{App.application()}.parent_path());

    // WasmService
    auto WS = std::make_shared<WasmService>();

    // VirtualMachine
    // fmt::println("[vm] {}", VMO.serialize().dump());
    auto Application = App.application();
    auto M = WS->get(Application);

    // TODO: phase2
    auto SigS = std::make_shared<SignatureService>();
    auto StaticService = static_code::Service{{Application}, SigS};
    static_code::Cache StaticCodeCache;

    // VirtualMachine
    std::shared_ptr<VirtualMachine> VM;
    if (CO.RemoteCompilation) { // with remote compiler
      std::shared_ptr<CompilerServer> CS;
      // todo: hardcoded
      LOG(fmt::format("using remote compiler"));

      CS = std::make_shared<transport::RemoteCompilerClient>(CompilerIP);

      // CompilerClient
      auto ClO = CompilerClient::Options{50}; // same as in the orc
      auto CC = std::make_shared<CompilerClient>(CS, ClO, Application);

      VM = std::make_shared<VirtualMachine>(VMO, StaticCodeCache, M, CC);
    } else { // with local compiler
      LOG(fmt::format("using local compiler"));
      // CompilerService
      auto CSO = CO.CSO;
      auto CompilerService = std::make_shared<CompilationService>(WS, CSO);

      // PredictionService
      auto PS = std::make_shared<PredictionService>();

      // SyncService
      auto Sync = CO.Sync;
      // fmt::println("[sync] {}", sync_compilation::str(Sync));
      auto SyncService = std::make_shared<sync_compilation::Service>(
          Sync, WS, CompilerService, PS);

      // AsyncService
      std::optional<std::shared_ptr<async_compilation::Service>> AsyncService;
      auto Async = CO.Async;
      if (Async.has_value()) {
        // fmt::println("[async] {}", async_compilation::str(*Async));
        AsyncService =
            std::optional{std::make_shared<async_compilation::Service>(
                *Async, WS, CompilerService)};
      }

      // CompilerServer
      auto CS = std::make_shared<CompilerServer>(SyncService, AsyncService);

      // CompilerClient
      auto ClO = CompilerClient::Options{50}; // same as in the orc
      auto CC = std::make_shared<CompilerClient>(CS, ClO, Application);

      // VirtualMachine
      VM = std::make_shared<VirtualMachine>(VMO, StaticCodeCache, M, CC);
    }

    if (not Wakeup.past()) {
#ifdef TRACE_WORKER
      const auto Duration = Wakeup.nanos();
      fmt::println("[worker] sleep for {:.2f} s", Duration.count() / 1e9);
#endif
      std::this_thread::sleep_until(Wakeup.Time);
    }

    LOG("starting");
    VM->start();
    LOG("done");

    orchestration::ResultMsg Res{};
    Res.VM = VM->Prof;

    return std::move(Res);
  }
};

struct CLIOptions {
  Worker::Options workerOptions() const {
    std::string IP = GetHostIP();
    if (VMap.contains("worker-ip"))
      IP = VMap["worker-ip"].as<std::string>();

    std::string Port = "60000";
    if (VMap.contains("worker-port"))
      Port = VMap["worker-port"].as<std::string>();

    std::string OrchestratorIP = "127.0.0.1";
    if (VMap.contains("orchestrator-ip"))
      OrchestratorIP = VMap["orchestrator-ip"].as<std::string>();

    return Worker::Options{
        .IP = IP,
        .Port = Port,
        .OrcIP = OrchestratorIP,
    };
  }

  po::variables_map VMap;
  explicit CLIOptions(const int Argc, char **Argv) {
    // clang-format off
    po::options_description Desc{"Options"};
    Desc.add_options()("help,h", "Help message");
    Desc.add_options()("worker-ip,ip", po::value<std::string>(), "Worker IP");
    Desc.add_options()("worker-port,port", po::value<std::string>(), "Worker port");
    Desc.add_options()("orchestrator-ip,orc-ip", po::value<std::string>(), "Orchestrator IP address");
    // clang-format on

    const auto Opts = po::command_line_parser(Argc, Argv).options(Desc).run();
    po::store(Opts, VMap);
    po::notify(VMap);

    if (VMap.contains("help")) {
      std::cout << Desc << "\n";
      exit(0);
    }
  }
};

int main(const int Argc, char **Argv) {
  const auto CLI = CLIOptions{Argc, Argv};

  auto [IP, Port, OrcIP] = CLI.workerOptions();

  auto Address = IP + ":" + Port;

  transport::WorkerTransport Trans{Address};
  std::thread TransThread([&] { Trans.start(); });
  std::this_thread::sleep_for(seconds(2));
  // from now the worker is ready to accept commands

  Worker W{CLI.workerOptions()};
  std::thread WorkerThread;
  auto OnWorkAsync = [&, OrcIP](orchestration::CommandMsg &&Cmd) {
    WorkerThread =
        std::thread{[&, OrcIP, CompilerIP = OrcIP, Cmd = std::move(Cmd)] {
          // LOG(fmt::format("command {}", Cmd.serialize().dump()));
          LOG("command");
          auto Result = W.work(Cmd, CompilerIP);
          W.disconnect(std::move(Result));
          Trans.stop();
        }};
  };
  Trans.onWorkAsync(std::move(OnWorkAsync));

  W.connect();

  TransThread.join();
  WorkerThread.join();

  return 0;
}