#pragma once

#include "Compiler/CompilerServer/CompilerServer.h"
#include "Globals.h"
#include "Orchestrator/Applications.h"
#include "OrchestratorServer.h"
#include "fmt/color.h"
#include "fmt/core.h"
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// TODO: move it to a separate file;

class OrchestratorClient {
public:
  explicit OrchestratorClient(ClusterOptions CO);
  virtual std::vector<Workload> getExecutionOptions() = 0;
  virtual ~OrchestratorClient() = default;

protected:
  std::vector<Application> AllApplications;
  ClusterOptions CO;
};

class InteractiveOrchestratorClient final : public OrchestratorClient {
public:
  explicit InteractiveOrchestratorClient(const ClusterOptions &CO);
  std::vector<Workload> getExecutionOptions() override;

private:
  static void Ask(const std::string &Question);
  static void Respond(const std::string &Answer);
};

class AutomatedOrchestratorClient final : public OrchestratorClient {
public:
  explicit AutomatedOrchestratorClient(ClusterOptions CO);
  std::vector<Workload> getExecutionOptions() override;

private:
  // std::vector<ExecutionOptions> TrueJITvsRemoteJITvsLocal();
  // std::vector<ExecutionOptions> ImpactOfCacheAndPrediction();
  // std::vector<ExecutionOptions> ImpactOfNetworkLatency();
  // std::vector<ExecutionOptions> ImpactOfNumberOfWorkers();
  // std::vector<ExecutionOptions> ImpactOfStaggeredReleaseDelay();
  // std::vector<ExecutionOptions> ImpactOfPredictionDepth();
  // std::vector<ExecutionOptions> DatasetGen();
};