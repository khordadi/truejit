#include "CraneliftCompiler.h"

#include <ATen/core/interned_strings.h>
#include <wabt/ir.h>

struct Vector {
  u8 *data;
  size_t size;
};

extern "C" {
void *compiler_constructor(const char *application);

Vector *compile(void *compiler, u32 func, u32 opt_level,
                bool disable_hardware_features);

void compiler_destructor(void *compiler);
}

CraneliftCompiler::CraneliftCompiler(ModuleOptions Sess)
    : ::Compiler(std::move(Sess)) {
  Compiler = compiler_constructor(this->Opts.M->name.c_str());
}
std::vector<char> CraneliftCompiler::compile(const Request Req) {
  const auto Spec = specialization(Req);
  assert(not Spec.has_value() and
         "Specialization is not supported for Cranelift");

  auto [OptLevel, EnableHardwareExtensions] = optimizations(Req);
  assert(OptLevel == 0 or
         OptLevel == 1 and "Invalid optimization level for Cranelift");

  const auto disable_hardware_features = not EnableHardwareExtensions;
  const auto Result =
      ::compile(Compiler, Req.Fn, OptLevel, disable_hardware_features);
  return {Result->data, Result->data + Result->size};
}
CraneliftCompiler::~CraneliftCompiler() { compiler_destructor(Compiler); }