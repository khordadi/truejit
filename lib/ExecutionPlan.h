#pragma once
#include "Compiler/Specialization.h"
#include "DigitalSignature/DigitalSignatureService.h"
#include "Wasm/WasmService.h"
#include <Compiler/CompilerServer/Compilation/CompilationService.h>
#include <Compiler/CompilerServer/CompilerServer.h>
#include <filesystem>
#include <fmt/color.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>
#include <variant>

using json = nlohmann::json;

// AsyncCompilationStrategy
struct DynamicPlan { // formerly Oracle
  static std::string GetPlanPath(const std::string &Application) {
    auto OracleHistoryPath = Application;
    if (OracleHistoryPath.ends_with(".wasm"))
      OracleHistoryPath.replace(OracleHistoryPath.find(".wasm"), 5, ".json");
    if (OracleHistoryPath.ends_with(".wat"))
      OracleHistoryPath.replace(OracleHistoryPath.find(".wat"), 4, ".json");
    if (OracleHistoryPath.find("benchmarks") != std::string::npos)
      OracleHistoryPath.replace(OracleHistoryPath.find("benchmarks"), 10,
                                "histories");
    if (not exists(std::filesystem::path(OracleHistoryPath)))
      ErrAndExit(fmt::format("Oracle history file {} does not exist",
                             OracleHistoryPath));
    return OracleHistoryPath;
  }
};
struct StaticPlan {
  static std::string GetPlanPath(const std::string &Application) {
    auto PlanPath = Application;
    if (PlanPath.ends_with(".wasm"))
      PlanPath.replace(PlanPath.find(".wasm"), 5, ".json");
    if (PlanPath.ends_with(".wat"))
      PlanPath.replace(PlanPath.find(".wat"), 4, ".json");
    if (PlanPath.find("benchmarks") != std::string::npos)
      PlanPath.replace(PlanPath.find("benchmarks"), 10, "plans");
    if (not std::filesystem::exists(PlanPath))
      ErrAndExit(fmt::format("Plan file {} does not exist", PlanPath));
    return PlanPath;
  }
};
