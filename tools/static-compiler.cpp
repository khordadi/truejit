#include "Globals.h"

#include "Compiler/StaticCode.h"
#include <Compiler/CompilerServer/CompilationService/CompilationService.h>
#include <Config.h>
#include <DigitalSignature/DigitalSignatureService.h>
#include <Wasm/WasmService.h>
#include <boost/program_options.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
namespace po = boost::program_options;

int main(const int Argc, char **Argv) {

  ///////////////////////////////////
  po::options_description Desc{"Options"};
  Desc.add_options()("help,h", "Help message");
  Desc.add_options()("compiler", po::value<std::string>(),
                     "Compiler; LLVM or Cranelift");
  Desc.add_options()("optimization,O", po::value<int>(),
                     "Compiler optimization level (0-3)");
  Desc.add_options()("hardware-extensions,ext", po::value<bool>(),
                     "Compiler hardware extensions enabled (true/false)");
  Desc.add_options()("args", po::value<std::vector<std::string>>(),
                     "Application");
  // clang-format on

  po::positional_options_description Pod;
  Pod.add("args", -1);

  po::variables_map VMap;
  const auto Opts =
      po::command_line_parser(Argc, Argv).options(Desc).positional(Pod).run();
  po::store(Opts, VMap);
  po::notify(VMap);

  if (VMap.contains("help")) {
    std::cout << Desc << "\n";
    exit(0);
  }
  ///////////////////////////////////

  const auto WS = std::make_shared<WasmService>();

  CompilationService::Options CSOpts{};
  if (VMap.contains("compiler")) {
    if (const auto Str = VMap["compiler"].as<std::string>(); Str == "llvm")
      CSOpts.CompilationOpts.Tier =
          CompilationService::Options::CompilationOptions::Tier::LLVM;
    else if (Str == "clift")
      CSOpts.CompilationOpts.Tier =
          CompilationService::Options::CompilationOptions::Tier::Cranelift;
    else
      Panic(fmt::format("Unknown compiler tier: {}", Str));
  }

  if (VMap.contains("optimization"))
    CSOpts.CompilationOpts.DefaultOptimizations.Level =
        VMap["optimization"].as<int>();

  if (VMap.contains("hardware-extensions"))
    CSOpts.CompilationOpts.DefaultOptimizations.Extensions =
        VMap["hardware-extensions"].as<bool>();

  auto CompilerService = std::make_shared<CompilationService>(WS, CSOpts);

  const auto Application = VMap["args"].as<std::vector<std::string>>().front();

  const auto BinDir = static_code::GetBinaryDirectory(Application);
  create_directories(BinDir);
#ifdef TRACE_STATIC_COMPILER
  fmt::print("Binary directory: {}\n", BinDir.string());
#endif

  auto M = WS->get(Application);

  auto C = CompilerService->compiler(Application);

  auto StaticFunctions = std::visit(static_code::GetFunctions{Application, WS},
                                    static_code::Strategy{static_code::All{}});

  std::vector<std::pair<u32, std::shared_future<std::vector<char>>>> Futures{};
  for (auto I : StaticFunctions) {
    Futures.emplace_back(I, std::async([I, CompilerService, C] {
                           return CompilerService->compileUsing(C)({I});
                         }));
#ifdef TRACE_STATIC_COMPILER
    auto Percentage = static_cast<int>((I + 1) * 100.0 / M->funcs.size());
    std::cout << '\r' << "Compiling... " << std::setw(3) << std::setfill(' ')
              << Percentage << "%" << std::flush;
#endif
  }
#ifdef TRACE_STATIC_COMPILER
  std::cout << '\r' << std::string(100, ' ') << '\r' << std::flush;
#endif

  std::map<std::string, std::vector<unsigned char>> Signatures{};
  auto DSS = std::make_shared<SignatureService>();
  for (const auto &[I, F] : Futures) {
    // store in a file {ObjDir}/{I}.o
    auto FileName = BinDir / fmt::format("{}.o", I);
    std::ofstream OFS(FileName, std::ios::binary);
    auto Code = F.get();
    auto Signature = DSS->sign(Code);
    Signatures[FileName] = Signature;
    OFS.write(Code.data(), Code.size());
    OFS.close();

    auto Percentage = static_cast<int>((I + 1) * 100.0 / M->funcs.size());
#ifdef TRACE_STATIC_COMPILER
    std::cout << '\r' << "Writing... " << std::setw(3) << std::setfill(' ')
              << Percentage << "%" << std::flush;
#endif
  }
#ifdef TRACE_STATIC_COMPILER
  std::cout << '\r' << std::string(100, ' ') << '\r' << std::flush;
#endif

  json J;
  for (const auto &[FileName, Signature] : Signatures) {
    J[FileName] = Signature;
  }

  // verify the signature
  for (const auto &[FileName, Signature] : Signatures) {
    std::ifstream IFS(FileName, std::ios::binary);
    std::vector Code((std::istreambuf_iterator(IFS)),
                     (std::istreambuf_iterator<char>()));
    auto Result = DSS->verify(Code, Signature);
    // std::cout << "The signature of " << FileName << " is "
    // << (Result ? "valid" : "invalid") << std::endl;
  }

  // write to file {ObjDir}/signatures.json
  std::ofstream SignatureFile(BinDir / "signatures.json");
  SignatureFile << J.dump(2);
  SignatureFile.close();

  std::string ResultStr;
  json ResultJ;

#ifdef TRACE_STATIC_COMPILER
  // log results
  if (not ResultStr.empty()) {
    ResultStr += "(seconds)";
    Logger.log(ResultSS, ResultStr);
  }
#endif

  // write to PROFILE
  if (auto ProfilePath = std::getenv("PROFILE")) {
    std::ofstream Profile(ProfilePath);

    Profile << ResultJ.dump();
    Profile.close();
  }

  return 0;
}
