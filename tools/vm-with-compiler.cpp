#include "CLIOptions.h"
#include "Compiler/CompilerServer/AsyncCompilation/AsyncCompilation.h"
#include "Compiler/CompilerServer/CompilationService/CompilationService.h"
#include "Compiler/CompilerServer/CompilerServer.h"
#include "Compiler/CompilerServer/Predictor/PredictionService.h"
#include "Compiler/CompilerServer/SyncCompilation/SyncCompilation.h"
#include "VirtualMachine/VirtualMachine.h"

using namespace std::chrono;

int main(int Argc, char **Argv) {
  auto CLI = CLIOptions{Argc, Argv};

  // WasmService
  auto WS = std::make_shared<WasmService>();

  // VirtualMachine
  auto VMOpts = CLI.vmOptions();

  auto Application = VMOpts.App.application();
  auto M = WS->get(Application);

  std::optional<static_code::Cache> StaticCodeCache;
  if (auto StaticFunctions = CLI.staticStrategy();
      StaticFunctions.has_value()) {
    auto SigS = std::make_shared<SignatureService>();
    auto StaticService = static_code::Service{{Application}, SigS};
    StaticCodeCache = StaticService.loadAndVerify(*StaticFunctions);
    {
      auto Num = StaticCodeCache->size();
      double Size;
      for (auto &Code : *StaticCodeCache | std::views::values)
        Size += static_cast<double>(Code.size());
      fmt::print("[static-code-cache] count={} size={:.2f} KB\n", Num,
                 Size / 1024.0);
    }
  }

  // CompilerService
  auto CO = CLI.compilerOptions();
  auto CompilerService = std::make_shared<CompilationService>(WS, CO);

  // PredictionService
  auto PS = std::make_shared<PredictionService>();

  // SyncService
  auto Sync = CLI.syncStrategy();
  // fmt::println("[sync] {}", sync_compilation::str(Sync));
  auto SyncService = std::make_shared<sync_compilation::Service>(
      Sync, WS, CompilerService, PS);

  // AsyncService
  std::optional<std::shared_ptr<async_compilation::Service>> AsyncService;
  auto Async = CLI.asyncStrategy();
  if (Async.has_value()) {
    // fmt::println("[async] {}", async_compilation::str(*Async));
    AsyncService = std::optional{std::make_shared<async_compilation::Service>(
        *Async, WS, CompilerService)};
  }

  // CompilerServer
  auto CS = std::make_shared<CompilerServer>(SyncService, AsyncService);
  auto Latency = CLI.networkLatency();
  if (Latency.has_value())
    CS = std::make_shared<DelayedCompilerServer>(
        CS, DelayedCompilerServer::Options{*Latency});

  // CompilerClient
  auto ClientOpts = CLI.clientOptions();
  auto CC = std::make_shared<CompilerClient>(CS, ClientOpts, Application);

  // VirtualMachine
  auto VM = std::make_shared<VirtualMachine>(VMOpts, StaticCodeCache, M, CC);

  VM->start();

  auto ClientCache = CC->cache();
  // fmt::println("[client-cache] writes={} hits={} misses={} evictions={}",
  //              ClientCache->Writes, ClientCache->Hits, ClientCache->Misses,
  //              ClientCache->Evictions);

  // if (const auto &VMCache = VM->cache(); VMCache.has_value())
  //   fmt::println("[vm-cache] misses={} evictions={}", (*VMCache)->Misses,
  //                (*VMCache)->Evictions);

  json ResultJ;

  // auto Report = Reporter{ResultJ, ResultStr};

#ifdef PROFILE_COMPILATION_SERVICE
  // Report(CompilerService->Prof);
#endif

#ifdef PROFILE_VIRTUAL_MACHINE
  // Report(VM->Prof);
#endif

  if (Latency.has_value()) {
#if defined(PROFILE_COMPILER_SERVER) and defined(PROFILE_COMPILER_CLIENT)
    long double NetLatency = CC->Prof->Total.load() - CS->Prof->Total.load();
    ResultJ["network"] = NetLatency;
#endif
  }

#ifdef PROFILE_WASM_SERVICE
  // Report(WS->Prof);
#endif

  if (std::holds_alternative<sync_compilation::Predictive>(Sync)) {
#ifdef PROFILE_PREDICTION_SERVICE
    // Report(CS->Prof);
#endif
  }

  // write to PROFILE
  if (auto ProfilePath = std::getenv("PROFILE")) {
    std::ofstream Profile(ProfilePath);

    std::string Title;

    ResultJ["title"] = Title;

    Profile << ResultJ.dump();
    Profile.close();
  }

  return 0;
}
