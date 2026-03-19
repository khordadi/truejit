#include "Wasm/WasmService.h"
#include <fmt/format.h>
#include <wabt/binary-reader-ir.h>
#include <wabt/error-formatter.h>
#include <wabt/validator.h>
#include <wabt/wast-lexer.h>
#include <wabt/wast-parser.h>

using namespace wabt;

// #define TRACE_WASM_MODULE_SERVICE

std::unique_ptr<Module> WasmService::ReadWat(const std::string &WatFile) {
  std::vector<uint8_t> Buf;
  Result Result = ReadFile(WatFile, &Buf);
  if (Failed(Result)) {
    std::cerr << fmt::format("Error reading file: '{}'\n", WatFile);
    std::exit(EXIT_FAILURE);
  }

  // Lexing the input file.
  Errors Errs;
  std::unique_ptr<WastLexer> Lexer =
      WastLexer::CreateBufferLexer(WatFile, Buf.data(), Buf.size(), &Errs);
  if (Failed(Result)) {
    auto LineFinder = Lexer->MakeLineFinder();
    FormatErrorsToFile(Errs, Location::Type::Text, LineFinder.get());
    std::exit(EXIT_FAILURE);
  }

  std::unique_ptr<Module> WasmModule;

  // ParseWorkerOptions the input file.
  Features Features;
  WastParseOptions ParseWastOptions(Features);
  Result = ParseWatModule(Lexer.get(), &WasmModule, &Errs, &ParseWastOptions);
  if (Failed(Result)) {
    auto LineFinder = Lexer->MakeLineFinder();
    FormatErrorsToFile(Errs, Location::Type::Text, LineFinder.get());
    std::exit(EXIT_FAILURE);
  }

  // Validate the Model.
  const ValidateOptions Options(Features);
  Result = ValidateModule(WasmModule.get(), &Errs, Options);
  if (Failed(Result)) {
    auto LineFinder = Lexer->MakeLineFinder();
    FormatErrorsToFile(Errs, Location::Type::Text, LineFinder.get());
    std::exit(EXIT_FAILURE);
  }

  return WasmModule;
}

std::unique_ptr<Module> WasmService::ReadWasm(const std::string &File) {
  std::vector<uint8_t> Buf;
  if (auto R = ReadFile(File, &Buf); Succeeded(R)) {
    constexpr Features Features;
    Errors Errs;
    Module M;
    const ReadBinaryOptions RBO(Features, nullptr, true, true, true);
    R = ReadBinaryIr(File.c_str(), Buf.data(), Buf.size(), RBO, &Errs, &M);
    if (Succeeded(R)) {
      const ValidateOptions VO(Features);
      R = ValidateModule(&M, &Errs, VO);
      if (Succeeded(R))
        return std::make_unique<Module>(std::move(M));
    }
    FormatErrorsToFile(Errs, Location::Type::Binary);
  }
  Panic(fmt::format("[wasm-parser] file '{}' is invalid.", File));
}

WasmService::WasmService() = default;
std::shared_ptr<Module> WasmService::get(const std::string &Application) {
  std::lock_guard Lock(ModulesLock);
  if (not Modules.contains(Application))
    Modules[Application] = ReadWebAssemblyModule(Application);
  return Modules[Application];
}
std::set<u32> WasmService::NonWasiFunctions(const std::shared_ptr<Module> &M) {
  std::set<u32> Result{};
  for (u32 I = 0; I < M->funcs.size(); ++I)
    if (not M->funcs[I]->name.starts_with("$__imported_wasi_snapshot_preview1"))
      Result.insert(I);
  return Result;
}

std::shared_ptr<Module>
WasmService::ReadWebAssemblyModule(const std::string &Application) const {
#ifdef PROFILE_WASM_SERVICE
  const auto Start = high_resolution_clock::now();
#endif
  std::unique_ptr<Module> M;
  if (Application.ends_with(".wat")) {
    M = ReadWat(Application);
  } else if (Application.ends_with(".wasm")) {
    M = ReadWasm(Application);
  } else
    Panic(fmt::format("[WasmService] file '{}' is invalid.", Application));
  M->name = Application;
#ifdef PROFILE_WASM_SERVICE
  const auto End = high_resolution_clock::now();
  Prof->onParsing(Application, Start, End);
#ifdef TRACE_WASM_MODULE_SERVICE
  fmt::print(fmt::fg(fmt::color::blue_violet),
             "[wasm-module-service][read] {} {}ms\n", Application, Duration);
#endif
#endif

  return M;
}