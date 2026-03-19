#include "CLIOptions.h"
#include <../lib/Compiler/CompilerServer/RemoteCompilerServer/RemoteCompilerServer.h>
#include <vector>

#define PRINT_RESULTS
#ifdef PRINT_RESULTS
#include "Logging.h"
static Logger Logger{"remote-compiler", fmt::color::gold};
static auto ResultSS = SubScope{"result", fmt::color::lime_green};
#endif

int main(const int Argc, char **Argv) {
  auto MainStart = NOW();

  auto CLI = CLIOptions{Argc, Argv};

  // WasmService
  auto WS = std::make_shared<WasmService>();

  // CompilerService
  auto CO = CLI.compilerOptions();
  auto CompilerService = std::make_shared<CompilationService>(WS, CO);

  // PredictionService
  auto PS = std::make_shared<PredictionService>();

  // SyncService
  auto Sync = CLI.syncStrategy();
  fmt::println("[sync] {}", sync_compilation::str(Sync));
  auto SyncService = std::make_shared<sync_compilation::Service>(
      Sync, WS, CompilerService, PS);

  // AsyncService
  std::optional<std::shared_ptr<async_compilation::Service>> AsyncService;
  auto Async = CLI.asyncStrategy();
  if (Async.has_value()) {
    fmt::println("[async] {}", async_compilation::str(*Async));
    AsyncService = std::optional{std::make_shared<async_compilation::Service>(
        *Async, WS, CompilerService)};
  }

  // CompilerServer
  auto CS = std::make_shared<CompilerServer>(SyncService, AsyncService);
  auto Latency = CLI.networkLatency();
  if (Latency.has_value())
    CS = std::make_shared<DelayedCompilerServer>(
        CS, DelayedCompilerServer::Options{*Latency});

  const auto RCS = std::make_shared<transport::RemoteCompilerServer>(CS);
  auto ServerThread = std::thread([&]() { RCS->start(); });
  std::cout << "Compiler Server is running; press 'q' to quit...";
  while (true) {
    char c;
    std::cin >> c;
    if (c == 'q') {
      RCS->shutdown();
      ServerThread.join();
      break;
    }
  }

  auto MainEnd = NOW();

  return 0;
}