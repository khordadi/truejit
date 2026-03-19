#pragma once
#include "../lib/Compiler/CompilerServer/AsyncCompilation/AsyncCompilation.h"
#include "../lib/Compiler/CompilerServer/SyncCompilation/SyncCompilation.h"
#include "Compiler/CompilerClient/CompilerClient.h"
#include "Compiler/CompilerServer/CompilationService/CompilationService.h"
#include "VirtualMachine/VirtualMachine.h"
#include <boost/program_options.hpp>
namespace po = boost::program_options;

struct CLIOptions {
  VirtualMachine::Options vmOptions() const {
    std::optional<VirtualMachine::Options::Interpretation> Interp;
    if (VMap.contains("interp")) {
      if (const auto Type = VMap["interp"].as<std::string>(); Type == "all")
        Interp = VirtualMachine::Options::Interpretation::All;
      else if (Type == "planned")
        Interp = VirtualMachine::Options::Interpretation::Planned;
      else
        Panic(fmt::format("Unknown interpretation: {}", Type));
    }

    std::optional<int> CacheLimit;
    if (VMap.contains("vm-cache"))
      CacheLimit = VMap["vm-cache"].as<int>();

    std::vector<std::string> PreOpens;
    if (VMap.contains("dir"))
      PreOpens = VMap["dir"].as<std::vector<std::string>>();

    auto Args = VMap["args"].as<std::vector<std::string>>();
    const VirtualMachine::Options::Application App{std::move(PreOpens),
                                                   std::move(Args)};

    return VirtualMachine::Options{App, CacheLimit, Interp};
  }

  CompilerClient::Options clientOptions() const {
    std::optional<int> CacheLimit;
    if (VMap.contains("client-cache"))
      CacheLimit = VMap["client-cache"].as<int>();
    return CompilerClient::Options{CacheLimit};
  }

  std::optional<std::set<unsigned>> staticStrategy() const {
    if (VMap.contains("static")) {
      if (const auto Strategy = VMap["static"].as<std::string>();
          Strategy == "planned") {
        auto Plan = VecToSet(ReadPlan("static"));
        fmt::println("[statically-compiled] {} functions", Plan.size());
        return std::move(Plan);
      } else if (Strategy == "all")
        Panic("not implemented yet!");
    }
    return std::nullopt;
  }

  CompilationService::Options compilerOptions() const {
    auto Tier = CompilationService::Options::CompilationOptions::Tier::LLVM;
    if (VMap.contains("compiler")) {
      if (const auto Str = VMap["compiler"].as<std::string>(); Str == "llvm")
        Tier = CompilationService::Options::CompilationOptions::Tier::LLVM;
      else if (Str == "clift")
        Tier = CompilationService::Options::CompilationOptions::Tier::Cranelift;
      else
        Panic(fmt::format("Unknown compiler tier: {}", Str));
    }

    int Optimization = 3;
    if (VMap.contains("optimization"))
      Optimization = VMap["optimization"].as<int>();

    bool Extensions = true;
    if (VMap.contains("hardware-extensions") and
        VMap["hardware-extensions"].as<bool>() == false)
      Extensions = false;

    std::optional<int> CacheLimit;
    if (VMap.contains("server-cache"))
      CacheLimit = VMap["server-cache"].as<int>();

    std::optional<SpecializationPlan> SP;
    if (VMap.contains("specialize")) {
      auto Mode = VMap["specialize"].as<std::string>();
      assert(Mode == "planned" and "Only planned specialization is supported");
      auto Plan = VecToSet(ReadPlan("specialize"));
      fmt::println("[specialization] {} functions", Plan.size());
      SP = std::make_optional<SpecializationPlan>(std::move(Plan));
    }

    return CompilationService::Options{
        CompilationService::Options::CompilationOptions{
            Tier, {Optimization, Extensions}, std::move(SP)},
        CompilationService::Options::CachingOptions{CacheLimit}};
  }

  sync_compilation::Strategy syncStrategy() const {
    if (VMap.contains("sync"))
      return sync_compilation::of(VMap["sync"].as<std::string>());
    return sync_compilation::JustInTime{};
  }

  std::optional<async_compilation::Strategy> asyncStrategy() const {
    if (VMap.contains("async"))
      return async_compilation::of(VMap["async"].as<std::string>());
    return std::nullopt;
  }

  std::optional<std::string> specializationPlan() const {}

  std::optional<int> networkLatency() const {
    if (VMap.contains("network-latency"))
      return VMap["network-latency"].as<int>();
    return std::nullopt;
  }

  po::variables_map VMap;
  explicit CLIOptions(const int Argc, char **Argv) {
    // clang-format off
    po::options_description Desc{"Options"};
    Desc.add_options()("help,h", "Help message");
    Desc.add_options()("interp", po::value<std::string>(),"Interpret the application instead of compiling it [all, planned]");
    Desc.add_options()("sync", po::value<std::string>(), "Synchronous compilation strategy");
    Desc.add_options()("async", po::value<std::string>(), "Asynchronous compilation strategy");
    Desc.add_options()("static", po::value<std::string>(), "Static compilation strategy");
    Desc.add_options()("compiler", po::value<std::string>(), "Compiler; LLVM or Cranelift");
    Desc.add_options()("optimization,O", po::value<int>(), "Compiler optimization level (0-3)");
    Desc.add_options()("hardware-extensions,ext", po::value<bool>(), "Compiler hardware extensions enabled (true/false)");
    Desc.add_options()("specialize,spec",po::value<std::string>(), "Enable specialization");
    Desc.add_options()("network-latency,nl", po::value<int>(), "Network latency");
    Desc.add_options()("vm-cache", po::value<int>(), "VM code cache limit");
    Desc.add_options()("client-cache", po::value<int>(), "Compiler Client code cache limit");
    Desc.add_options()("server-cache", po::value<int>(), "Compiler Server code cache limit");
    // Desc.add_options()("benchmark,bench", po::value<std::string>(), "Benchmark file");
    Desc.add_options()("dir", po::value<std::vector<std::string>>()->composing(), "input directories");
    Desc.add_options()("args", po::value<std::vector<std::string>>(), "Application arguments");
    // clang-format on

    po::positional_options_description Pod;
    Pod.add("args", -1);

    const auto Opts =
        po::command_line_parser(Argc, Argv).options(Desc).positional(Pod).run();
    po::store(Opts, VMap);
    po::notify(VMap);

    if (VMap.contains("help")) {
      std::cout << Desc << "\n";
      exit(0);
    }
  }
};