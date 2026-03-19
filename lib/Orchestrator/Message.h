#pragma once
#include "../Compiler/CompilerServer/AsyncCompilation/AsyncCompilation.h"
#include "../Compiler/CompilerServer/SyncCompilation/SyncCompilation.h"
#include "Compiler/CompilerServer/CompilationService/CompilationService.h"
#include "VirtualMachine/VirtualMachine.h"
#include <grpcpp/support/time.h>
#include <utility>

namespace orchestration {
struct SyncMsg {
  std::string Address;
  explicit SyncMsg() = default;
  explicit SyncMsg(std::string Address) : Address(std::move(Address)) {}
};

struct AckMsg {
  bool Connected{};
  explicit AckMsg() = default;
  explicit AckMsg(const bool Connected) : Connected(Connected) {}
};
struct CommandMsg {
  struct CompilerOptions {
    CompilationService::Options CSO{};
    sync_compilation::Strategy Sync{sync_compilation::JustInTime{}};
    std::optional<async_compilation::Strategy> Async{};
    bool RemoteCompilation = true;

    CompilerOptions() = default;
    CompilerOptions(
        const CompilationService::Options &CSO, sync_compilation::Strategy Sync,
        const std::optional<async_compilation::Strategy> &Async = std::nullopt)
        : CSO(CSO), Sync(std::move(Sync)), Async(Async) {}
  };
  struct TimePoint {
    time_point<system_clock> Time;
    TimePoint() : Time(system_clock::now()) {}
    explicit TimePoint(const time_point<system_clock> &Time) : Time(Time) {}
    void add(const milliseconds &M) { Time += M; }
    void add(const seconds &S) { Time += S; }
    void sub(const milliseconds &M) { Time -= M; }
    void sub(const seconds &S) { Time -= S; }

    // return nanoseconds from now
    nanoseconds nanos() const {
      return duration_cast<nanoseconds>(Time - system_clock::now());
    }

    bool past() const { return Time < system_clock::now(); }

    uint64_t time_since_epoch() const {
      return duration_cast<nanoseconds>(Time.time_since_epoch()).count();
    }
  };

  VirtualMachine::Options VMO{};
  CompilerOptions CO{};
  TimePoint Wakeup{};

  CommandMsg() = default;
  explicit CommandMsg(VirtualMachine::Options &&VMO, TimePoint &&Wakeup,
                      CompilerOptions &&LCO)
      : VMO(std::move(VMO)), CO(std::move(LCO)), Wakeup(std::move(Wakeup)) {}
  CommandMsg(const CommandMsg &Other)
      : VMO(Other.VMO), CO(Other.CO), Wakeup(Other.Wakeup) {}
};
struct ResultMsg {
  VirtualMachine::Profiler VM;
};

} // namespace orchestration