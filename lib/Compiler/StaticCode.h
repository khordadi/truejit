#pragma once

#include "Wasm/WasmService.h"
#include <DigitalSignature/DigitalSignatureService.h>
#include <Globals.h>
#include <filesystem>
#include <fmt/color.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <variant>

using json = nlohmann::json;

namespace static_code {

// todo : move it to a better place?
inline std::string GetHistoryPath(const std::string &Application) {
  auto Path = Application;
  if (Path.ends_with(".wasm"))
    Path.replace(Path.find(".wasm"), 5, ".json");
  if (Path.ends_with(".wat"))
    Path.replace(Path.find(".wat"), 4, ".json");
  if (Path.find("benchmarks") != std::string::npos)
    Path.replace(Path.find("benchmarks"), 10, "histories");
  if (not exists(std::filesystem::path(Path)))
    Panic(fmt::format("File not found: {}", Path));
  return Path;
}

// todo : move it to a better place?
inline std::vector<u32> GetHistory(const std::string &Application) {
  const auto HistoryPath = GetHistoryPath(Application);
  std::ifstream IFS(HistoryPath);
  json J;
  IFS >> J;
  std::vector<u32> History;
  for (const auto &Element : J.items())
    History.push_back(Element.value().get<u32>());
  return History;
}

// this is responsible for deciding who is static code cache candidate
struct All {
  std::set<u32> operator()(const std::shared_ptr<wabt::Module> &M) const {
    std::set<u32> Fs;
    for (u32 I = 0; I < M->funcs.size(); ++I)
      if (not M->funcs[I]->name.starts_with(
              "$__imported_wasi_snapshot_preview1"))
        Fs.insert(I);
    return Fs;
  }
};

struct Startup {
  int Percentage;
  explicit Startup(auto Percentage) : Percentage(Percentage) {}

  std::set<u32> operator()(const std::string &Application) const {
    auto Fs = GetHistory(Application);
    Fs.resize(Fs.size() * Percentage / 100);
    return {Fs.begin(), Fs.end()};
  }
};

struct SurplusMaximiser {
  std::set<u32> operator()() const {
    // hardcoded; TODO: make it configurable
    auto PlanPath = "/tmp/surplus.json";

    std::ifstream IFS(PlanPath);
    json J;
    IFS >> J;

    std::set<u32> Fs;
    for (const auto &Element : J.items())
      Fs.insert(Element.value().get<u32>());
    return Fs;
  }
};

using Strategy = std::variant<All, Startup, SurplusMaximiser>;

inline std::filesystem::path GetBinaryDirectory(std::string Application) {
  const std::string Prefix{"/benchmarks/"};
  Application.replace(Application.find(Prefix), Prefix.size(), "/binaries/");
  const auto Path = std::filesystem::path(Application);
  return Path.parent_path();
}

using Cache = std::unordered_map<u32, std::vector<char>>;

// todo: rename it to loader service; takes strategy and application and returns
// the functions this is responsible for loading the code and verifying the
// signatures
class Service {
public:
  struct Options {
    std::string Application;
  };

  // todo: add profiler
  struct Profiler {
    std::atomic<u64> Total{0};
  };

  explicit Service(Options Opts, std::shared_ptr<SignatureService> SigS)
      : SigS(std::move(SigS)), Application(std::move(Opts.Application)) {}

  Cache loadAndVerify(const std::set<u32> &Fs) const {
    auto BinaryDir = GetBinaryDirectory(Application);
    if (not exists(BinaryDir))
      Panic(
          fmt::format("Binary directory {} doesn't exist", BinaryDir.string()));

    std::ifstream IFS(BinaryDir / "signatures.json");
    json J;
    IFS >> J;
    IFS.close();
    auto Signatures = J.get<std::map<std::string, std::vector<u8>>>();

    auto Codes = Cache{};
    Codes.reserve(Fs.size());

    for (const auto &F : Fs) {
      const auto File = BinaryDir / fmt::format("{}.o", F);
      std::ifstream IFS(File, std::ios::binary);
      auto Code = std::vector((std::istreambuf_iterator(IFS)),
                              (std::istreambuf_iterator<char>()));
      if (not SigS->verify(Code, Signatures[File]))
        Panic(fmt::format("Invalid signature for {}", File.string()));
      Codes[F] = Code;
    }

    return Codes;
  }

  ~Service() = default;

private:
  std::shared_ptr<SignatureService> SigS;
  std::string Application;
};

inline std::optional<Strategy> of(std::string Str) {
  // clang-format off
  if (Str == "all") return All{};
  if (Str.starts_with("startup")) {
    if (const auto N = Str.substr(std::strlen("startup.")); N.ends_with("%")) {
      const auto Percentage = std::stoi(N.substr(0, N.size() - 1));
      assert(0 <= Percentage <= 100 and "Invalid percentage");
      return Startup{Percentage};
    }
    Panic(fmt::format("Invalid startup strategy '{}'; must be startup.<percentage>%", Str));
  }
  if (Str == "max-surplus") return SurplusMaximiser{};
  Panic(fmt::format("invalid static strategy '{}'", Str));
  // clang-format on
}

inline std::string str(Strategy S) {
  struct {
    // clang-format off
    std::string operator()(const All &) const { return "all"; }
    std::string operator()(const Startup &S) const { return fmt::format("startup.{}%", S.Percentage); }
    std::string operator()(const SurplusMaximiser &) const { return "max-surplus"; }
    // clang-format on
  } Visitor;
  return std::visit(Visitor, S);
}

// TODO: this is needed by TriageService; maybe we can move it to a better place
// make it a service for getting functions
struct GetFunctions {
private:
  std::string Application;
  std::shared_ptr<WasmService> WS;

public:
  GetFunctions(const std::string &Application, std::shared_ptr<WasmService> WS)
      : Application(Application), WS(WS) {}

  std::set<u32> operator()(const All &S) const {
    return S(WS->get(Application));
  }
  std::set<u32> operator()(const Startup &S) const { return S(Application); }
  std::set<u32> operator()(const SurplusMaximiser &S) const { return S(); }
};

} // namespace static_code