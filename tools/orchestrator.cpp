#include "Orchestrator/Orchestrator.h"
#include "../lib/Compiler/CompilerServer/AsyncCompilation/AsyncCompilation.h"
#include "Compiler/CompilerServer/CompilationService/CompilationService.h"
#include "Orchestrator/OrchestratorTransport.h"
#include "VirtualMachine/VirtualMachine.Serialization.h"
#include "VirtualMachine/VirtualMachine.h"
#include <boost/program_options.hpp>
#include <cassert>
#include <string>
#include <vector>

#define TRACE_ORCHESTRATOR

using namespace orchestration;
using namespace std::string_literals;
namespace po = boost::program_options;

struct CLIOptions {
  sync_compilation::Strategy syncStrategy() const {
    if (VMap.contains("sync"))
      return sync_compilation::of(VMap["sync"].as<std::string>());
    return sync_compilation::JustInTime{};
  }

  std::optional<VirtualMachine::Options::Application> benchmark() const {
    if (VMap.contains("benchmark")) {
      const auto Benchmark = VMap["benchmark"].as<std::string>();
      if (std::ifstream IFS(fmt::format(
              "/tmp/truejit/benchmarks/{}/benchmark.json", Benchmark));
          IFS.is_open()) {
        json J;
        IFS >> J;
        IFS.close();
        VirtualMachine::Options::Application App;
        deserialize(std::move(J), App);
        return std::move(App);
      }
      Panic(fmt::format("Benchmark file '{}' does not exist.", Benchmark));
    }
    return std::nullopt; // No benchmark specified
  }

  std::optional<int> localCache() const {
    if (VMap.contains("local-cache")) {
      const auto CacheLimit = VMap["local-cache"].as<int>();
      assert(CacheLimit > 0);
      return CacheLimit;
    }
    return std::nullopt; // No local cache specified
  }
  std::optional<int> globalCache() const {
    if (VMap.contains("global-cache")) {
      const auto CacheLimit = VMap["global-cache"].as<int>();
      assert(CacheLimit > 0);
      return CacheLimit;
    }
    return std::nullopt; // No global cache specified
  }

  int clusterSize() const {
    if (VMap.contains("size")) {
      const auto Size = VMap["size"].as<int>();
      assert(Size > 0);
      return Size;
    }
    Panic("Cluster size is required. Use --cluster-size option.");
  }

  seconds wakeup() const {
    if (VMap.contains("wakeup")) {
      const auto Delay = VMap["wakeup"].as<int>();
      assert(Delay >= 0);
      return seconds{Delay};
    }
    return seconds{3}; // Default wakeup delay
  }

  std::optional<int> interval() const {
    if (VMap.contains("interval")) {
      const auto Interval = VMap["interval"].as<int>();
      assert(Interval > 0);
      return Interval;
    }
    return std::nullopt; // No interval specified
  }

  bool remoteCompilation() const {
    if (VMap.contains("remote-compilation"))
      return VMap["remote-compilation"].as<bool>();
    return true; // Default to using remote compilation
  }

  po::variables_map VMap;
  explicit CLIOptions(const int Argc, char **Argv) {
    // clang-format off
    po::options_description Desc{"Options"};
    Desc.add_options()("help,h", "Help message");
    Desc.add_options()("sync", po::value<std::string>(), "Synchronous compilation strategy");
    Desc.add_options()("benchmark", po::value<std::string>(),"Path to the benchmark file (optional, overrides command)");
    Desc.add_options()("local-cache", po::value<int>(), "Local code cache size limit");
    Desc.add_options()("global-cache", po::value<int>(), "Global code cache size limit");
    Desc.add_options()("size,s,n", po::value<int>(), "Cluster size");
    Desc.add_options()("wakeup,w", po::value<int>(), "Wakeup delay in seconds");
    Desc.add_options()("interval,i", po::value<int>(), "Interval in milliseconds for periodic tasks");
    Desc.add_options()("remote-compilation", po::value<bool>(), "Use remote compilation");
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

  CommandMsg Command;
  if (auto Benchmark = CLI.benchmark(); Benchmark.has_value())
    Command.VMO.App = std::move(*Benchmark);
  Command.CO.Sync = CLI.syncStrategy();
  Command.VMO.CacheLimit = CLI.localCache();
  Command.CO.CSO.CachingOpts.Limit = CLI.globalCache();
  Command.CO.RemoteCompilation = CLI.remoteCompilation();
  const auto ClusterSize = CLI.clusterSize();
  const auto Wakeup = CLI.wakeup();
  const auto Interval = CLI.interval();

  Orchestrator Orc{{std::move(Command), ClusterSize, Wakeup, Interval}};
  transport::OrchestratorTransport Trans{Orc};
  std::thread TransThread{[&] { Trans.start(); }};
  std::thread OrcThread{[&] {
    Orc.start();
    Trans.stop();
  }};
  TransThread.join();
  OrcThread.join();

  return 0;
}
