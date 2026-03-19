#pragma once

#include "../CompilationService/CompilationService.h"
#include <Globals.h>
#include <boost/thread/synchronized_value.hpp>
#include <fmt/core.h>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace async_compilation {
struct All {};
struct Planned {};
struct Dynamic {};
struct Static {};
using Strategy = std::variant<All, Planned, Dynamic, Static>;

Strategy of(std::string Str);
std::string str(Strategy S);

class AsyncCompiler {
public:
  explicit AsyncCompiler(CompilationService::CompileFn Compile,
                         std::vector<u32> Init, std::set<u32> Exceptions)
      : Ins(std::move(Init)), Exceptions(std::move(Exceptions)),
        Compile(std::move(Compile)) {}

  virtual ~AsyncCompiler() {
    Thread.join();
    // for (auto &F : Futures)
    // F.get();
  }
  virtual void push(const u32 F) {
    auto &&Vec = Ins.synchronize();
    Vec->push_back(F);
  }
  virtual void stop() { Stop = true; }

protected:
  std::optional<u32> pop() {
    auto &&Vec = Ins.synchronize();
    if (Vec->empty())
      return std::nullopt;
    const auto F = Vec->back();
    Vec->pop_back();
    return std::make_optional(F);
  }

  virtual void start() = 0;

  std::thread Thread;
  std::atomic<bool> Stop{false};
  boost::synchronized_value<std::vector<u32>> Ins;
  const std::set<u32> Exceptions;
  CompilationService::CompileFn Compile;
};
class AllAsyncCompiler final : public AsyncCompiler {
public:
  explicit AllAsyncCompiler(CompilationService::CompileFn Compile,
                            std::vector<u32> Init, std::set<u32> Exceptions)
      : AsyncCompiler(std::move(Compile), std::move(Init),
                      std::move(Exceptions)) {
    start();
  }
  void push(u32 F) override {} // do nothing

private:
  void start() override {
    Thread = std::thread([this] {
      while (not Stop) {
        const auto Back = pop();
        if (not Back or Exceptions.contains(*Back))
          continue;
        Compile(CompilationService::Request{*Back});
      }
    });
  }
};
class StaticAsyncCompiler final : public AsyncCompiler {
public:
  explicit StaticAsyncCompiler(CompilationService::CompileFn Compile,
                               std::vector<u32> Init, std::set<u32> Exceptions,
                               std::map<u32, std::vector<u32>> Plan)
      : AsyncCompiler(std::move(Compile), std::move(Init),
                      std::move(Exceptions)),
        Plan(std::move(Plan)) {
    start();
  }

private:
  std::map<u32, std::vector<u32>> Plan;
  void start() override {
    Thread = std::thread([this] {
      auto NoneBlacklist = [this](auto F) {
        return not Exceptions.contains(F);
      };

      while (not Stop) {
        const auto Back = pop();
        if (not Back.has_value())
          continue;

        std::vector<std::future<void>> Futures;
        for (const auto &F :
             Plan[Back.value()] | std::views::filter(NoneBlacklist)) {
          if (Stop)
            break;

          Futures.push_back(std::async(std::launch::async, [this, F] {
            Compile(CompilationService::Request{F});
          }));
        }
        for (auto &Future : Futures)
          Future.get();
      }
    });
  }
};

namespace static_plan {
static std::map<std::string, std::map<u32, std::vector<u32>>> Plans;
static Lock PlansLock;
static auto Path(std::string Application) {
  const std::string From{"/benchmarks/"};
  Application.replace(Application.find(From), From.size(), "/plans/");
  auto Path = std::filesystem::path(Application);
  Path.replace_extension(".json");
  if (not exists(Path))
    Panic(fmt::format("static async plan not found {}", Path.string()));
  return Path;
}

static std::map<u32, std::vector<u32>> Get(const std::string &Application) {
  {
    ReadLock RL(PlansLock);
    if (Plans.contains(Application))
      return Plans[Application];
  }

  const auto PlanPath = Path(Application);
  std::ifstream IFS(PlanPath);
  json J;
  IFS >> J;
  std::map<u32, std::vector<u32>> Plan;
  for (const auto &E : J.items())
    for (const auto &Value : E.value())
      Plan[std::stoi(E.key())].push_back(Value.get<u32>());

  {
    WriteLock WL(PlansLock);
    Plans[Application] = Plan;
  }
  return Plan;
}
} // namespace static_plan

class Service {
  Strategy S;
  std::shared_ptr<WasmService> WS;
  std::shared_ptr<CompilationService> CS;

public:
  Service(Strategy S, std::shared_ptr<WasmService> WS,
          std::shared_ptr<CompilationService> CS)
      : S(std::move(S)), WS(std::move(WS)), CS(std::move(CS)) {}

  std::shared_ptr<AsyncCompiler> operator()(const std::string &Application) {
    auto Compile = [this, Compiler = CS->compiler(Application)](auto Req) {
      return CS->compileUsing(Compiler)(std::move(Req));
    };

    // add specializations functions to here;
    // add static functions to here
    std::set<unsigned int> Exceptions{};

    if (std::holds_alternative<All>(S)) {
      std::vector<unsigned int> Init{};
      for (const auto F : WasmService::NonWasiFunctions(WS->get(Application)))
        Init.push_back(F);
      return std::make_shared<AllAsyncCompiler>(Compile, Init, Exceptions);
    }
    if (std::holds_alternative<Planned>(S)) {
      auto Plan = ReadPlan("async");
      fmt::println("[async::planned] {} functions", Plan.size());
      // reverse the plan
      std::ranges::reverse(Plan);
      return std::make_shared<AllAsyncCompiler>(Compile, Plan, Exceptions);
    }
    if (std::holds_alternative<Dynamic>(S))
      Panic("Dynamic async compilation is not implemented yet");
    if (std::holds_alternative<Static>(S)) {
      auto Plan = static_plan::Get(Application);
      std::vector<unsigned int> Init{}; // TODO: add static code
      return std::make_shared<StaticAsyncCompiler>(Compile, Init, Exceptions,
                                                   Plan);
    }
    Panic(fmt::format("Invalid async strategy '{}'", str(S)));
  }
};

} // namespace async_compilation