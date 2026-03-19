#include "CLIOptions.h"
#include <Compiler/CompilerServer/CompilationService/CompilationService.h>
#include <Compiler/LLVMCompiler/LLVMCompiler.h>
#include <Compiler/RemoteCompilerClient/RemoteCompilerClient.h>
#include <VirtualMachine/VirtualMachine.h>

#define PRINT_RESULTS
#ifdef PRINT_RESULTS
#include "Logging.h"
static Logger Logger{"virtual-machine", fmt::color::gold};
static auto ResultSS = SubScope{"result", fmt::color::lime_green};
#endif

int main(int Argc, char **Argv) {
  auto CLI = CLIOptions{Argc, Argv};

  auto MainStart = NOW();

  // WasmService
  auto WS = std::make_shared<WasmService>();

  // VirtualMachine
  auto VMO = CLI.vmOptions();

  auto Application = VMO.App.application();
  auto M = WS->get(Application);

  // TODO: phase2
  auto StaticFunctions = std::set<u32>{};
  auto StaticStrategy = CLI.staticStrategy();
  if (StaticStrategy.has_value())
    StaticFunctions =
        std::visit(static_code::GetFunctions{Application, WS}, *StaticStrategy);

  // TODO: phase2
  // TODO: add specializations functions to here
  auto Exceptions = std::set<u32>{};
  for (auto E : Exceptions)
    StaticFunctions.erase(E);

  // TODO: phase2
  auto SigS = std::make_shared<SignatureService>();
  auto StaticService = static_code::Service{{Application}, SigS};
  static_code::Cache StaticCodeCache;
  if (StaticStrategy.has_value())
    StaticCodeCache = StaticService.loadAndVerify(StaticFunctions);

  // CompilerServer
  auto CS = std::make_shared<transport::RemoteCompilerClient>();

  // CompilerClient
  auto ClientOpts = CLI.clientOptions();
  auto CC = std::make_shared<CompilerClient>(CS, ClientOpts, Application);

  // VirtualMachine
  auto VM = std::make_shared<VirtualMachine>(VMO, StaticCodeCache, M, CC);

  VM->start();

  auto MainEnd = NOW();

  auto ClientCache = CC->cache();
  fmt::println("[client-cache] writes={} hits={} misses={} evictions={}",
               ClientCache->Writes, ClientCache->Hits, ClientCache->Misses,
               ClientCache->Evictions);

  // if (const auto &VMCache = VM->cache(); VMCache.has_value())
  //   fmt::println("[vm-cache] misses={} evictions={}", (*VMCache)->Misses,
  //                (*VMCache)->Evictions);

  std::string ResultStr;
  json ResultJ;

  /*
  auto Report = Reporter{ResultJ, ResultStr};

#ifdef PROFILE_VIRTUAL_MACHINE
  Report(VM->Prof);
#endif

#ifdef PROFILE_WASM_SERVICE
  Report(WS->Prof);
#endif

  if (StaticStrategy.has_value()) {
#ifdef PROFILE_SIGNATURE_SERVICE
    Report(SigS->Prof);
#endif
  }
  */

  // log results
  if (ResultStr.empty()) {
    auto Main = static_cast<double>(MainEnd - MainStart);
    Main /= 1e9;
    ResultStr += fmt::format("main={:.1f} ", Main);
  }

  ResultStr += "(seconds)";

  if (ClientOpts.CacheLimit.has_value()) {
    ResultStr += " | ";
    ResultStr += fmt::format("local-cache-size={}", *ClientOpts.CacheLimit);
  }

  Logger.log(ResultSS, ResultStr);

  // write to PROFILE
  if (auto ProfilePath = std::getenv("PROFILE")) {
    std::ofstream Profile(ProfilePath);

    std::string Title;

    ResultJ["title"] = Title;
    ResultJ["start"] = MainStart;
    ResultJ["end"] = MainEnd;

    Profile << ResultJ.dump();
    Profile.close();
  }

  // write the HISTORY
  if (auto Path = std::getenv("HISTORY")) {
    auto HistoryPath = std::filesystem::path(Path);
    if (not exists(HistoryPath))
      create_directories(HistoryPath.parent_path());
    std::ofstream History(HistoryPath);
    json J = CC->History;
    History << J.dump();
    History.close();
  }

  return 0;
}
