#include "CompilationService.h"
#include "GlobalCodeCache.h"
#include <Compiler/CompilerServer/CompilerServer.h>
#include <Compiler/CraneliftCompiler/CraneliftCompiler.h>
#include <Compiler/LLVMCompiler/LLVMCompiler.h>
#include <ThreadPool.h>
#include <functional>
#include <memory>
#include <utility>

class CompilerInstance {
  std::shared_ptr<GlobalCodeCache> Cache;
  std::function<Compiler::Response(Compiler::Request)> Compile;

public:
  explicit CompilerInstance(const CompilationService::Options &Opts,
                            std::shared_ptr<wabt::Module> M) {
    auto [CompilationOpts, CachingOpts] = Opts;
    auto [Tier, DefaultOptimizations, SpecPlan] = std::move(CompilationOpts);
    const Compiler::ModuleOptions ModOpts{std::move(M), DefaultOptimizations,
                                          SpecPlan};
    std::shared_ptr<Compiler> Compiler;
#if defined(PROFILE_CALLS) or defined(PROFILE_FOR_SPECIALIZATION)
    // when profiling for call or specialization, we can only use LLVM as
    // the instrumentation for these profilers is only implemented in LLVM
    Compiler = std::make_shared<LLVMCompiler>(ModOpts);
#else
    switch (Tier) {
    case CompilationService::Options::CompilationOptions::Tier::LLVM:
      Compiler = std::make_shared<LLVMCompiler>(ModOpts);
      break;
    case CompilationService::Options::CompilationOptions::Tier::Cranelift:
      Compiler = std::make_shared<CraneliftCompiler>(ModOpts);
      break;
    default:
      Panic("Unknown compiler tier");
    }
#endif

    Compile = [Compiler = std::move(Compiler)](auto Req) mutable {
      return Compiler->compile(std::move(Req));
    };

    auto &[Limit] = CachingOpts;
    Cache = std::make_shared<GlobalCodeCache>(Limit);
  }

  Compiler::Response operator()(Compiler::Request Req) const {
    return Cache->get(Req.Fn,
                      [Req = std::move(Req), this] { return Compile(Req); });
  }
};

CompilationService::CompilationService(std::shared_ptr<WasmService> WS,
                                       Options Opts)
    : WS(std::move(WS)), Opts(std::move(Opts)) {}

std::shared_ptr<CompilerInstance>
CompilationService::compiler(const std::string &Application) {
  WriteLock WL(ModulesLock);
  if (not Modules.contains(Application))
    Modules[Application] =
        std::make_shared<CompilerInstance>(Opts, WS->get(Application));
  return Modules[Application];
}

std::function<CompilationService::Response(CompilationService::Request)>
CompilationService::compileUsing(
    const std::shared_ptr<CompilerInstance> &Compile) const {
  return [this, Compile](const Request Req) {
#ifdef PROFILE_COMPILATION_SERVICE
    const auto Start = high_resolution_clock::now();
#endif

    // TODO: here use VM context for specialization
    auto [Fn, ActivationFrame] = Req;
    auto Code = (*Compile)({.Fn = Fn, .ActivationFrame = ActivationFrame});

#ifdef PROFILE_COMPILATION_SERVICE
    const auto End = high_resolution_clock::now();
    Prof->onRequest(Fn, Code.size(), Start, End);
#endif
    return Code;
  };
}