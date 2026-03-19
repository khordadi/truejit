#pragma once

#include "Config.h"
#include "Globals.h"
#include <atomic>
#include <map>
#include <nlohmann/json.hpp>
#include <wabt/ir.h>

using json = nlohmann::json;
using namespace std::chrono;

class WasmService {
public:
  WasmService();
  std::shared_ptr<wabt::Module> get(const std::string &Application);
  static std::set<u32> NonWasiFunctions(const std::shared_ptr<wabt::Module> &M);

  struct Profiler {
    using Time = const time_point<high_resolution_clock> &;
    struct Record {
      std::string Application;
      u64 Start;
      u64 End;
    };
    std::vector<Record> Times;
    std::atomic<u64> Total{};

    void onParsing(const std::string &Application, Time Start, Time End) {
      auto S = Start.time_since_epoch().count();
      auto E = End.time_since_epoch().count();
      Total += E - S;
      WriteLock WL(TimesLock);
      Times.emplace_back(Application, S, E);
    }

  private:
    Lock TimesLock;
  };
  std::shared_ptr<Profiler> Prof{std::make_shared<Profiler>()};

private:
  std::shared_ptr<wabt::Module>
  ReadWebAssemblyModule(const std::string &File) const;

  static std::unique_ptr<wabt::Module> ReadWat(const std::string &WatFile);

  static std::unique_ptr<wabt::Module> ReadWasm(const std::string &File);

  std::map<std::string, std::shared_ptr<wabt::Module>> Modules;
  std::mutex ModulesLock;
};