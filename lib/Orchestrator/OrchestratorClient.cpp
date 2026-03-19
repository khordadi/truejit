/*
#include "Orchestrator/OrchestratorClient.h"
#include "Orchestrator/OrchestratorServer.h"

#ifdef TRACE_ORCHESTRATOR
#include "Logging.h"
static auto EventLogger = Logger{"orchestrator-client", fmt::color::gold};
auto VirtualMachineOptionsSS =
    SubScope{"virtual-machine-options", fmt::color::deep_sky_blue};
auto OrchestratorOptionsSS =
    SubScope{"orchestrator-options", fmt::color::deep_sky_blue};
auto CompilerOptionsSS =
    SubScope{"compiler-options", fmt::color::deep_sky_blue};
#endif

OrchestratorClient::OrchestratorClient(ClusterOptions CO) : CO(CO) {
  auto Benchmarks =
      json::parse(std::ifstream("/truejit/benchmarks/benchmarks.json"));
  for (auto &Benchmark : Benchmarks) {
    Application App;
    App.Name = Benchmark["name"];
    App.Suite = Benchmark["suite"];
    auto Binary = Benchmark["binary"].get<std::string>();
    auto Args = Benchmark["args"].get<std::vector<std::string>>();
    App.ApplicationAndArgs = {Binary};
    App.ApplicationAndArgs.insert(App.ApplicationAndArgs.end(), Args.begin(),
                                  Args.end());
    App.PreOpens = Benchmark["pre_opens"].get<std::vector<std::string>>();

    AllApplications.push_back(App);
  }
}

InteractiveOrchestratorClient::InteractiveOrchestratorClient(
    const ClusterOptions &CO)
    : OrchestratorClient(CO) {}

std::vector<Workload> InteractiveOrchestratorClient::getExecutionOptions() {
  // Application Options
  Ask(fmt::format("Benchmark?\n"));
  for (auto I = 0; I < AllApplications.size(); I++) {
    auto &Application = AllApplications[I];
    if (I == 0)
      Ask(fmt::format("({}) {}::{} [default]\n", I + 1, Application.Suite,
                      Application.Name));
    else
      Ask(fmt::format("({}) {}::{}\n", I + 1, Application.Suite,
                      Application.Name));
  }
  std::string Line;
  std::getline(std::cin, Line);
  if (Line.empty())
    Line = "1";
  auto Index = std::stoi(Line);
  if (Index < 1 or Index > AllApplications.size())
    ErrAndExit("Invalid option");
  auto &Application = AllApplications[Index - 1];
  Respond(
      fmt::format("Benchmark: {}::{}", Application.Suite, Application.Name));

  const auto VMO = VirtualMachine::Options(
      VirtualMachine::Options::Application{Application.PreOpens,
                                           Application.ApplicationAndArgs},
      std::nullopt);

#ifdef TRACE_ORCHESTRATOR
  EventLogger.log(VirtualMachineOptionsSS,
                  fmt::format("benchmark={}", Application.Name));
#endif

  // Orchestrator Options
  auto OrcO = std::make_shared<OrchestratorOptions>();

  Ask(fmt::format("Number of workers? (max: {})", CO.ClusterSize));
  std::getline(std::cin, Line);
  OrcO->NumberOfWorkers = Line.empty() ? CO.ClusterSize : std::stoi(Line);
  Respond(fmt::format("Number of workers: {}", OrcO->NumberOfWorkers));
  //  OrcO->NumberOfWorkers = CSO.ClusterSize;

  Ask(fmt::format("Wakeup delay (seconds)? [2s]"));
  std::getline(std::cin, Line);
  OrcO->WakeupDelay = Line.empty() ? 2 : std::stoi(Line);
  Respond(fmt::format("Wakeup delay: {}s", OrcO->WakeupDelay));
  //  OrcO->WakeupDelay = 2;

  Ask(fmt::format("Wakeup interval (milliseconds)? [0ms]"));
  std::getline(std::cin, Line);
  OrcO->WakeupInterval = Line.empty() ? 0 : std::stoi(Line);
  Respond(fmt::format("Wakeup interval: {}ms", OrcO->WakeupInterval));
  //  OrcO->WakeupInterval = 0;

  Ask(fmt::format("Remote compiler? ([yes]/no)"));
  std::getline(std::cin, Line);
  if (Line.empty())
    Line = "yes";
  OrcO->WithRemoteCompiler = Line == "yes";
  Respond(fmt::format("Remote compiler: {}", OrcO->WithRemoteCompiler));

#ifdef TRACE_ORCHESTRATOR
  EventLogger.log(OrchestratorOptionsSS,
                  fmt::format("workers={} wakeup-delay={}s "
                              "wakeup-interval={}ms remote-compiler={}",
                              OrcO->NumberOfWorkers, OrcO->WakeupDelay,
                              OrcO->WakeupInterval, OrcO->WithRemoteCompiler));
#endif

  // Compiler Options
  auto CSO = std::make_shared<CompilationService::Options>();

  //   if (OrcO->WithRemoteCompiler) {
  //     //      Ask(fmt::format("Number of threads? ([multi])"));
  //     //      std::getline(std::cin, Line);
  //     //      CSO->Threads = Line.empty() ? "multi" : Line;
  //     //      Respond(fmt::format("Number of threads: {}", CSO->Threads));
  //     CSO->Threads = "multi";
  //   } else {
  //     // if no remote compiler, then only 1 thread
  //     CSO->Threads = "1";
  //   }
  //
  //   auto CompilationStrategyOptions =
  //       OrcO->WithRemoteCompiler
  //           ? std::vector<std::string>{"aot", "jit", "truejit"}
  //           : std::vector<std::string>{"aot", "jit"};
  //   Ask(fmt::format("Compilation mode?\n"));
  //   for (auto I = 0; I < CompilationStrategyOptions.size(); I++) {
  //     auto &Option = CompilationStrategyOptions[I];
  //     if (I != CompilationStrategyOptions.size() - 1)
  //       Ask(fmt::format("({}) {}\n", I + 1, Option));
  //     else
  //       Ask(fmt::format("({}) {} [default]", I + 1, Option));
  //   }
  //   std::getline(std::cin, Line);
  //   if (Line.empty()) {
  //     if (OrcO->WithRemoteCompiler)
  //       CSO->SyncCompilationStrategy = "truejit";
  //     else
  //       CSO->SyncCompilationStrategy = "jit";
  //   } else if (std::all_of(Line.begin(), Line.end(), isdigit)) {
  //     auto Index = std::stoi(Line);
  //     if (Index < 1 or Index > CompilationStrategyOptions.size())
  //       ErrAndExit("Invalid option");
  //     CSO->SyncCompilationStrategy = CompilationStrategyOptions[Index - 1];
  //   } else
  //     ErrAndExit("Invalid option");
  //   Respond(fmt::format("Compilation mode: {}",
  //   CSO->SyncCompilationStrategy));
  //
  //   if (CSO->SyncCompilationStrategy == "truejit") {
  //     Ask(fmt::format("Depth of LSTM speculative predictions? [100]"));
  //     std::getline(std::cin, Line);
  //     if (Line.empty())
  //       CSO->Predictor = "100-gram-lstm";
  //     else if (std::all_of(Line.begin(), Line.end(), isdigit)) {
  //       CSO->Predictor = Line + "-gram-lstm";
  //     } else
  //       ErrAndExit("Invalid option");
  //     Respond(fmt::format("PredictionService: {}", CSO->Predictor));
  //     //    CSO->PredictionService = "100-gram-lstm";
  //   }
  //
  //   Ask(fmt::format("Warmup application? (yes/[no])"));
  //   std::getline(std::cin, Line);
  //   if (Line.empty())
  //     Line = "no";
  //   CSO->WarmupStrategy =
  //       Line == "yes" ? Application.ApplicationAndArgs.front() : "";
  //   Respond(fmt::format("Warmup application: {}", CSO->WarmupStrategy));
  //
  //   // network latency
  //   Ask(fmt::format("Network latency? [0ms]"));
  //   std::getline(std::cin, Line);
  //   CSO->NetworkLatency = Line.empty() ? 0 : std::stoi(Line);
  //   Respond(fmt::format("Network latency: {}ms", CSO->NetworkLatency));
  //
  // #ifdef TRACE_ORCHESTRATOR
  //   std::string Msg;
  //   Msg += fmt::format("compilation-strategy={} ",
  //   CSO->SyncCompilationStrategy); Msg += fmt::format("threads={} ",
  //   CSO->Threads); if (not CSO->Predictor.empty())
  //     Msg += fmt::format("predictor={} ", CSO->Predictor);
  //   if (not CSO->WarmupStrategy.empty())
  //     Msg += fmt::format("warmup ");
  //   if (CSO->NetworkLatency)
  //     Msg += fmt::format("network-latency={} ", CSO->NetworkLatency);
  //   EventLogger.log(CompilerOptionsSS, Msg);
  // #endif

  return {Workload{
      .VMO = VMO,
      .OrcO = {},
  }};
}
void InteractiveOrchestratorClient::Ask(const std::string &Question) {
  std::cout << fmt::format(fg(fmt::color::light_golden_rod_yellow), "{}",
                           Question)
            << std::flush;
}
void InteractiveOrchestratorClient::Respond(const std::string &Answer) {
  std::cout << fmt::format(fg(fmt::color::yellow), "{}\n", Answer)
            << std::flush;
}

AutomatedOrchestratorClient::AutomatedOrchestratorClient(ClusterOptions CO)
    : OrchestratorClient(CO) {
  //  TrueJITvsRemoteJITvsLocal();
  //  ImpactOfCacheAndPrediction();
  //  ImpactOfNetworkLatency();
  //  ImpactOfNumberOfWorkers();
  //  ImpactOfStaggeredReleaseDelay();
}
std::vector<Workload> AutomatedOrchestratorClient::getExecutionOptions() {
  // ask for the next set of ExecutionOptions
  std::cout << "[automated-orchestrator-client] which experiment?\n";
  std::cout << "1. TrueJITvsRemoteJITvsLocal\n";
  std::cout << "2. ImpactOfCacheAndPrediction\n";
  std::cout << "3. ImpactOfNetworkLatency\n";
  std::cout << "4. ImpactOfNumberOfWorkers\n";
  std::cout << "5. ImpactOfStaggeredReleaseDelay\n";
  std::cout << "6. ImpactOfPredictionDepth\n";
  std::cout << "7. DatasetGen (first run: rm /truejit/predictor/dataset.txt)\n";
  std::string Line;
  std::getline(std::cin, Line);
  if (Line.empty())
    Line = "1";
  switch (std::stoi(Line)) {
  // case 1:
  //   return TrueJITvsRemoteJITvsLocal();
  // case 2:
  //   return ImpactOfCacheAndPrediction();
  // case 3:
  //   return ImpactOfNetworkLatency();
  // case 4:
  //   return ImpactOfNumberOfWorkers();
  // case 5:
  //   return ImpactOfStaggeredReleaseDelay();
  // case 6:
  //   return ImpactOfPredictionDepth();
  // case 7:
  //   return DatasetGen();
  default:
    ErrAndExit("Invalid option");
  }
}

// std::vector<ExecutionOptions>
// AutomatedOrchestratorClient::TrueJITvsRemoteJITvsLocal() {
//   constexpr unsigned int Repetitions = 2;
//   auto Executions = std::vector<ExecutionOptions>{};
//   for (const auto &Application : AllApplications) {
//     auto VMO = VirtualMachineOptions{
//         .ApplicationName = Application.Name,
//         .SuiteName = Application.Suite,
//         .PreOpens = Application.PreOpens,
//         .Args = Application.ApplicationAndArgs,
//     };
//     auto WarmupApplication = "";      // cold start
//     unsigned int WakeupInterval = 10; // staggered wakeup (10ms)
//     unsigned int WakeupDelay = 0;
//     unsigned int NetworkLatency = 0; // no network latency
//
//     for (auto &[WithRemoteCompiler, CompilationStrategy] :
//          std::vector<std::tuple<bool, std::string>>{
//              // {false, "aot"},   // (1) Local AOT
//              {false, "jit"},   // (2) Local JIT
//              {true, "jit"},    // (3) Remote JIT
//              {true, "truejit"} // (4) TrueJIT
//          }) {
//       auto Threads = WithRemoteCompiler ? "multi" : "1";
//       auto Predictor = CompilationStrategy == "truejit" ? "100-gram-lstm" :
//       ""; auto NumberOfWorkers = WithRemoteCompiler ? CO.ClusterSize : 1;
//       auto OrcO = OrchestratorOptions{
//           .NumberOfWorkers = NumberOfWorkers,
//           .WakeupDelay = WakeupDelay,
//           .WakeupInterval = WakeupInterval,
//           .WithRemoteCompiler = WithRemoteCompiler,
//       };
//       auto CSO = CompilerOptions{.Threads = Threads,
//                                  .SyncCompilationStrategy =
//                                  CompilationStrategy, .Predictor = Predictor,
//                                  .WarmupStrategy = WarmupApplication,
//                                  .NetworkLatency = NetworkLatency};
//       auto ExeO =
//           ExecutionOptions{.VMO =
//           std::make_shared<VirtualMachineOptions>(VMO),
//                            .OrcO =
//                            std::make_shared<OrchestratorOptions>(OrcO), .CSO
//                            = std::make_shared<CompilerOptions>(CSO)};
//       for (auto I = 0; I < Repetitions; I++)
//         Executions.push_back(ExeO);
//     }
//   }
//   return Executions;
// }
//
// std::vector<ExecutionOptions> AutomatedOrchestratorClient::DatasetGen() {
// #ifdef PRINT_PREDICTOR_DATASET
//   std::remove(PREDICTOR_DATASET_FILE_PATH);
// #endif
//
//   // print the name of all applications
//   auto Str = std::string{"[dataset-gen]\napplications=["};
//   for (auto &Application : AllApplications)
//     Str += Application.Name + ",";
//   Str.pop_back();
//   Str += "]";
//   std::cout << Str << std::endl;
//
//   auto Executions = std::vector<ExecutionOptions>{};
//   for (auto &Application : AllApplications) {
//     auto VMO = VirtualMachineOptions{
//         .ApplicationName = Application.Name,
//         .SuiteName = Application.Suite,
//         .PreOpens = Application.PreOpens,
//         .Args = Application.ApplicationAndArgs,
//     };
//
//     auto Threads = "single";
//     auto CompilationStrategy = "jit";
//     unsigned int WakeupInterval = 0;
//     unsigned int WakeupDelay = 0;
//     unsigned int NetworkLatency = 0;
//     auto WithRemoteCompiler = true;
//     unsigned int NumberOfWorkers = 1;
//
//     auto WarmupApplication = "";
//     auto Predictor = "";
//
//     auto OrcO = OrchestratorOptions{
//         .NumberOfWorkers = NumberOfWorkers,
//         .WakeupDelay = WakeupDelay,
//         .WakeupInterval = WakeupInterval,
//         .WithRemoteCompiler = WithRemoteCompiler,
//     };
//     auto CSO = CompilerOptions{.Threads = Threads,
//                                .SyncCompilationStrategy =
//                                CompilationStrategy, .Predictor = Predictor,
//                                .WarmupStrategy = WarmupApplication,
//                                .NetworkLatency = NetworkLatency};
//     auto ExeO =
//         ExecutionOptions{.VMO = std::make_shared<VirtualMachineOptions>(VMO),
//                          .OrcO = std::make_shared<OrchestratorOptions>(OrcO),
//                          .CSO = std::make_shared<CompilerOptions>(CSO)};
//
//     Executions.push_back(ExeO);
//   }
//
//   return Executions;
// }
//
// std::vector<ExecutionOptions>
// AutomatedOrchestratorClient::ImpactOfCacheAndPrediction() {
//   constexpr unsigned int Repetitions = 3;
//   auto Executions = std::vector<ExecutionOptions>{};
//   for (auto &Application : AllApplications) {
//     auto VMO = VirtualMachineOptions{
//         .ApplicationName = Application.Name,
//         .SuiteName = Application.Suite,
//         .PreOpens = Application.PreOpens,
//         .Args = Application.ApplicationAndArgs,
//     };
//
//     auto Threads = "multi";
//     unsigned int WakeupInterval = 0; // simultaneous wakeup
//     unsigned int WakeupDelay = 0;
//     unsigned int NetworkLatency = 0; // no network latency
//     auto WithRemoteCompiler = true;
//     unsigned int NumberOfWorkers = CO.ClusterSize;
//
//     for (auto &[CompilationStrategy, Warm] :
//          std::vector<std::tuple<std::string, bool>>{
//              {"jit", false},   // (1) no predictions, no cache;
//              {"jit", true},    // (2) no predictions, cache;
//              {"truejit", true} // (3) predictions, cache
//          }) {
//       auto WarmupApplication =
//           Warm ? Application.ApplicationAndArgs.front() : "";
//       auto Predictor = CompilationStrategy == "truejit" ? "100-gram-lstm" :
//       ""; if (CompilationStrategy == "jit" and not Warm)
//         NumberOfWorkers = 1;
//
//       auto OrcO = OrchestratorOptions{
//           .NumberOfWorkers = NumberOfWorkers,
//           .WakeupDelay = WakeupDelay,
//           .WakeupInterval = WakeupInterval,
//           .WithRemoteCompiler = WithRemoteCompiler,
//       };
//       auto CSO = CompilerOptions{.Threads = Threads,
//                                  .SyncCompilationStrategy =
//                                  CompilationStrategy, .Predictor = Predictor,
//                                  .WarmupStrategy = WarmupApplication,
//                                  .NetworkLatency = NetworkLatency};
//       auto ExeO =
//           ExecutionOptions{.VMO =
//           std::make_shared<VirtualMachineOptions>(VMO),
//                            .OrcO =
//                            std::make_shared<OrchestratorOptions>(OrcO), .CSO
//                            = std::make_shared<CompilerOptions>(CSO)};
//       for (auto I = 0; I < Repetitions; I++)
//         Executions.push_back(ExeO);
//     }
//   }
//   return Executions;
// }
// std::vector<ExecutionOptions>
// AutomatedOrchestratorClient::ImpactOfNetworkLatency() {
//   constexpr unsigned int Repetitions = 1;
//   auto Executions = std::vector<ExecutionOptions>{};
//   for (auto &Application : AllApplications) {
//     auto VMO = VirtualMachineOptions{
//         .ApplicationName = Application.Name,
//         .SuiteName = Application.Suite,
//         .PreOpens = Application.PreOpens,
//         .Args = Application.ApplicationAndArgs,
//     };
//
//     auto Threads = "multi";
//     unsigned int WakeupInterval = 0;
//     unsigned int WakeupDelay = 0;
//     auto WithRemoteCompiler = true;
//     unsigned int NumberOfWorkers = 1;
//     auto WarmupApplication = VMO.Args.front();
//
//     for (auto &[CompilationStrategy] : std::vector<std::tuple<std::string>>{
//              {"jit"},    // (1) RemoteJIT
//              {"truejit"} // (2) TrueJIT
//          }) {
//       auto Predictor = CompilationStrategy == "truejit" ? "100-gram-lstm" :
//       ""; auto OrcO = OrchestratorOptions{
//           .NumberOfWorkers = NumberOfWorkers,
//           .WakeupDelay = WakeupDelay,
//           .WakeupInterval = WakeupInterval,
//           .WithRemoteCompiler = WithRemoteCompiler,
//       };
//
//       for (auto NetworkLatency : std::vector<unsigned int>{0, 1, 2, 5, 10}) {
//         auto CSO =
//             CompilerOptions{.Threads = Threads,
//                             .SyncCompilationStrategy = CompilationStrategy,
//                             .Predictor = Predictor,
//                             .WarmupStrategy = WarmupApplication,
//                             .NetworkLatency = NetworkLatency};
//         auto ExeO = ExecutionOptions{
//             .VMO = std::make_shared<VirtualMachineOptions>(VMO),
//             .OrcO = std::make_shared<OrchestratorOptions>(OrcO),
//             .CSO = std::make_shared<CompilerOptions>(CSO)};
//         for (auto I = 0; I < Repetitions; I++)
//           Executions.push_back(ExeO);
//       }
//     }
//   }
//   return Executions;
// }
// std::vector<ExecutionOptions>
// AutomatedOrchestratorClient::ImpactOfNumberOfWorkers() {
//   constexpr unsigned int Repetitions = 1;
//   auto Executions = std::vector<ExecutionOptions>{};
//   for (auto &Application : AllApplications) {
//     auto VMO = VirtualMachineOptions{
//         .ApplicationName = Application.Name,
//         .SuiteName = Application.Suite,
//         .PreOpens = Application.PreOpens,
//         .Args = Application.ApplicationAndArgs,
//     };
//
//     auto Threads = "multi";
//     unsigned int WakeupInterval = 0;
//     unsigned int WakeupDelay = 2;
//     auto WithRemoteCompiler = true;
//     auto CompilationStrategy = "truejit";
//     auto Predictor = CompilationStrategy == "truejit" ? "100-gram-lstm" : "";
//     auto WarmupApplication = VMO.Args.front();
//     auto NetworkLatency = 0u;
//
//     for (auto NumberOfWorkers : std::vector<unsigned int>{1, 10, 100, 1000})
//     {
//       if (NumberOfWorkers > CO.ClusterSize) {
//         std::cout << "**** Number of workers exceeds the cluster size
//         ****\n"; break;
//       }
//       auto OrcO = OrchestratorOptions{
//           .NumberOfWorkers = NumberOfWorkers,
//           .WakeupDelay = WakeupDelay,
//           .WakeupInterval = WakeupInterval,
//           .WithRemoteCompiler = WithRemoteCompiler,
//       };
//       auto CSO = CompilerOptions{.Threads = Threads,
//                                  .SyncCompilationStrategy =
//                                  CompilationStrategy, .Predictor = Predictor,
//                                  .WarmupStrategy = WarmupApplication,
//                                  .NetworkLatency = NetworkLatency};
//       auto ExeO =
//           ExecutionOptions{.VMO =
//           std::make_shared<VirtualMachineOptions>(VMO),
//                            .OrcO =
//                            std::make_shared<OrchestratorOptions>(OrcO), .CSO
//                            = std::make_shared<CompilerOptions>(CSO)};
//       for (auto I = 0; I < Repetitions; I++)
//         Executions.push_back(ExeO);
//     }
//   }
//   return Executions;
// }
// std::vector<ExecutionOptions>
// AutomatedOrchestratorClient::ImpactOfStaggeredReleaseDelay() {
//   constexpr unsigned int Repetitions = 1;
//   auto Executions = std::vector<ExecutionOptions>{};
//   for (auto &Application : AllApplications) {
//     auto VMO = VirtualMachineOptions{
//         .ApplicationName = Application.Name,
//         .SuiteName = Application.Suite,
//         .PreOpens = Application.PreOpens,
//         .Args = Application.ApplicationAndArgs,
//     };
//
//     auto Threads = "multi";
//     unsigned int WakeupDelay = 1;
//     auto WithRemoteCompiler = true;
//     auto CompilationStrategy = "truejit";
//     auto Predictor = "100-gram-lstm";
//     auto WarmupApplication = "";
//     auto NetworkLatency = 0u;
//     auto NumberOfWorkers = CO.ClusterSize;
//
//     for (auto WakeupInterval : std::vector<unsigned int>{0, 1, 10, 100}) {
//       auto OrcO = OrchestratorOptions{
//           .NumberOfWorkers = NumberOfWorkers,
//           .WakeupDelay = WakeupDelay,
//           .WakeupInterval = WakeupInterval,
//           .WithRemoteCompiler = WithRemoteCompiler,
//       };
//       auto CSO = CompilerOptions{.Threads = Threads,
//                                  .SyncCompilationStrategy =
//                                  CompilationStrategy, .Predictor = Predictor,
//                                  .WarmupStrategy = WarmupApplication,
//                                  .NetworkLatency = NetworkLatency};
//       auto ExeO =
//           ExecutionOptions{.VMO =
//           std::make_shared<VirtualMachineOptions>(VMO),
//                            .OrcO =
//                            std::make_shared<OrchestratorOptions>(OrcO), .CSO
//                            = std::make_shared<CompilerOptions>(CSO)};
//
//       for (auto I = 0; I < Repetitions; I++)
//         Executions.push_back(ExeO);
//     }
//   }
//   return Executions;
// }
//
// std::vector<ExecutionOptions>
// AutomatedOrchestratorClient::ImpactOfPredictionDepth() {
//   constexpr unsigned int Repetitions = 1;
//   auto Executions = std::vector<ExecutionOptions>{};
//   for (auto &Application : AllApplications) {
//     auto VMO = VirtualMachineOptions{
//         .ApplicationName = Application.Name,
//         .SuiteName = Application.Suite,
//         .PreOpens = Application.PreOpens,
//         .Args = Application.ApplicationAndArgs,
//     };
//
//     auto Threads = "multi";
//     unsigned int WakeupInterval = 0;
//     unsigned int WakeupDelay = 0;
//     auto WithRemoteCompiler = true;
//     auto CompilationStrategy = "truejit";
//     auto WarmupApplication = "";
//     auto NetworkLatency = 0u;
//     auto NumberOfWorkers = 1u;
//
//     for (auto &Predictor : std::vector<std::string>{
//              "", "1-gram-lstm", "2-gram-lstm", "5-gram-lstm", "10-gram-lstm",
//              "20-gram-lstm", "50-gram-lstm", "100-gram-lstm",
//              "200-gram-lstm"}) {
//       auto OrcO = OrchestratorOptions{
//           .NumberOfWorkers = NumberOfWorkers,
//           .WakeupDelay = WakeupDelay,
//           .WakeupInterval = WakeupInterval,
//           .WithRemoteCompiler = WithRemoteCompiler,
//       };
//       auto CSO =
//           CompilerOptions{.Threads = Threads,
//                           .SyncCompilationStrategy =
//                               Predictor.empty() ? "jit" :
//                               CompilationStrategy,
//                           .Predictor = Predictor,
//                           .WarmupStrategy = WarmupApplication,
//                           .NetworkLatency = NetworkLatency};
//       auto ExeO =
//           ExecutionOptions{.VMO =
//           std::make_shared<VirtualMachineOptions>(VMO),
//                            .OrcO =
//                            std::make_shared<OrchestratorOptions>(OrcO), .CSO
//                            = std::make_shared<CompilerOptions>(CSO)};
//
//       for (auto I = 0; I < Repetitions; I++)
//         Executions.push_back(ExeO);
//     }
//   }
//   return Executions;
// }




*/