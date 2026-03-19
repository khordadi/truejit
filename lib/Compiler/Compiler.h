#pragma once

#include "Compiler/Specialization.h"
#include "Globals.h"

namespace wabt {
struct Module;
}

class Compiler {
public:
  virtual ~Compiler() = default;

  struct OptimizationOptions {
    int Level{3};
    bool Extensions{true};
  };

  // cross-module
  struct ModuleOptions {
    std::shared_ptr<wabt::Module> M;
    OptimizationOptions DefaultOptimizations{};
    std::optional<SpecializationPlan> SpecPlan;
  };

  // per function
  struct Request {
    u32 Fn{};
    std::optional<OptimizationOptions> Optimization;
    std::optional<Specialization> Spec;
    char *ActivationFrame{};
  };
  using Response = std::vector<char>;
  using CompileFn = std::function<Response(Request)>;

  explicit Compiler(ModuleOptions Opts) : Opts(std::move(Opts)) {}

  virtual Response compile(Request Req) = 0;

  OptimizationOptions optimizations(const Request &Req) const {
    if (Req.Optimization.has_value())
      return Req.Optimization.value();
    return Opts.DefaultOptimizations;
  }

  std::optional<Specialization> specialization(const Request &Req) const {
    if (Req.Spec.has_value())
      return Req.Spec.value();
    if (Opts.SpecPlan.has_value() and Opts.SpecPlan->Plan.contains(Req.Fn)) {
      return UnconditionalDynamic{Req.ActivationFrame};
    }
    return std::nullopt;
  }

protected:
  ModuleOptions Opts;
};