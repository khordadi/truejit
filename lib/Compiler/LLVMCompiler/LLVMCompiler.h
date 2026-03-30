#pragma once

#include "Compiler/Compiler.h"
#include "Config.h"
#include "Globals.h"
#include <future>
#include <nlohmann/json.hpp>
#include <vector>
#include <wabt/ir.h>

class Optimizer;
class LLVMCompiler final : public Compiler {
public:
  explicit LLVMCompiler(ModuleOptions Opts);
  std::vector<char> compile(Request) override;

private:
  std::optional<SpecializationPlan> SP;
};