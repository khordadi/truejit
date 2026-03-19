#pragma once

#include "Globals.h"
#include <Compiler/Compiler.h>
#include <vector>

class CraneliftCompiler final : public Compiler {
public:
  explicit CraneliftCompiler(ModuleOptions Sess);

  std::vector<char> compile(Request) override;

  ~CraneliftCompiler() override;

private:
  void *Compiler;
};
