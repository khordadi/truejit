#pragma once
#include "Message.h"
#include "VirtualMachine/VirtualMachine.h" // TODO: remove this dependency
#include <memory>

namespace orchestration {
using Profiler = std::shared_ptr<VirtualMachine::Profiler>;
class IntegratedCompiler;
class Orchestrator {
public:
  struct Options {
    CommandMsg Command;
    int ClusterSize;
    seconds Wakeup;
    std::optional<int> Interval;
  };
  explicit Orchestrator(Options &&Opts);

  void start();

  AckMsg Connect(SyncMsg &&In);
  void Disconnect(ResultMsg &&In);
  ~Orchestrator() = default;

private:
  Options Opts;

  std::vector<std::string> Connections;
  std::mutex AllConnectedMutex;
  std::condition_variable AllConnectedCV;

  std::vector<ResultMsg> Results;
  std::mutex ResultsMutex;
  std::condition_variable ResultsCV;
};

} // namespace orchestration