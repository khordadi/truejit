#pragma once
#include <Compiler/Compiler.h>
#include <Config.h>
#include <Globals.h>
#include <ThreadPool.h>
#include <Wasm/WasmService.h>
#include <atomic>
#include <utility>
#include <variant>

class CompilerInstance;
using namespace std::chrono;

class CompilationService {
public:
  struct Options {
    struct CompilationOptions {
      enum class Tier { LLVM, Cranelift } Tier{Tier::LLVM};
      Compiler::OptimizationOptions DefaultOptimizations{};
      std::optional<SpecializationPlan> SpecPlan;
    } CompilationOpts;
    struct CachingOptions {
      std::optional<int> Limit{};
      CachingOptions() = default;
      explicit CachingOptions(const std::optional<int> Limit) : Limit(Limit) {}
    } CachingOpts{};

    Options() = default;
    explicit Options(CompilationOptions &&Compilation, CachingOptions &&Cache)
        : CompilationOpts(Compilation), CachingOpts(Cache) {}
  };

  struct Request {
    u32 Fn{};
    char *ActivationFrame{};
  };
  using Response = Compiler::Response;
  using CompileFn = std::function<Response(Request)>;

  explicit CompilationService(std::shared_ptr<WasmService> WS, Options Opts);

  ~CompilationService() = default;

  std::shared_ptr<CompilerInstance> compiler(const std::string &Application);

  std::function<Response(Request)>
  compileUsing(const std::shared_ptr<CompilerInstance> &Compile) const;

  struct Profiler {
    using Time = const time_point<high_resolution_clock>;
    struct Request {
      u32 Fn;
      u32 Size;
      u64 Start;
      u64 End;
    };
    std::vector<Request> Requests;
    std::atomic<u64> Total{};

    void onRequest(u32 Fn, u32 Size, Time Start, Time End) {
      auto S = Start.time_since_epoch().count();
      auto E = End.time_since_epoch().count();
      Total += E - S;
      WriteLock WL(RequestsLock);
      Requests.emplace_back(Fn, Size, S, E);
    }

    ~Profiler() {
      fmt::println(
          "[compilation-service] #{} requests | compilation={:.2f} (seconds)",
          Requests.size(), Total.load() / 1e9);
      metric("compilation", std::to_string(Total.load()));
    }

  private:
    Lock RequestsLock;
  };

  std::shared_ptr<Profiler> Prof{std::make_shared<Profiler>()};

private:
  std::map<std::string, std::shared_ptr<CompilerInstance>> Modules;
  Lock ModulesLock{};
  std::shared_ptr<WasmService> WS{};

  Options Opts;
};
