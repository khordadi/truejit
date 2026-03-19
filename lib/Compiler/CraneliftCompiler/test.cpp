#include "Compiler/CraneliftCompiler/CraneliftCompiler.h"
#include <Wasm/WasmService.h>
#include <iostream>

int main() {
  const auto WS = std::make_shared<WasmService>();
  const auto M = WS->get("/truejit/lib/Compiler/CraneliftCompiler/in.wat");
  auto Compiler = CraneliftCompiler(Compiler::Options{M});
  auto Result = Compiler.compile(Compiler::Request{0, Specialization{}});
  // print the result as hex
  for (const auto &C : Result)
    std::cout << std::hex << static_cast<int>(C) << " ";
  return 0;
}