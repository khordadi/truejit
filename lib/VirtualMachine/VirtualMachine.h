#pragma once

#include "Compiler/CompilerClient/CompilerClient.h"
#include "Globals.h"
#include <Compiler/StaticCode.h>
#include <DigitalSignature/DigitalSignatureService.h>
#include <fmt/format.h>
#include <iostream>
#include <list>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <thread>
#include <utility>
#include <wabt/cast.h>

// #define PROFILING_VIRTUAL_MACHINE

struct uvwasi_s;
typedef struct uvwasi_s uvwasi_t;
class Monitor;
namespace static_code {
class Service;
}
namespace wabt {
struct Module;
}
class StackSubscriber;
class InstructionSubscriber;
class ReturnSubscriber;
class CallSubscriber;
class BranchSubscriber;

using json = nlohmann::json;

namespace llvm::orc {
class LLJIT;
}

class VirtualMachine {
public:
  struct Options final {
    struct Application final {
      std::vector<std::string> PreOpens{};
      std::vector<std::string> Args{};
      Application() = default;
      explicit Application(std::vector<std::string> PreOpens,
                           std::vector<std::string> Args)
          : PreOpens(std::move(PreOpens)), Args(std::move(Args)) {}
      std::string application() const { return Args.front(); }
    };
    Application App{};
    std::optional<int> CacheLimit{};
    enum Interpretation { All, Planned };
    std::optional<Interpretation> Interpret;
    explicit Options(Application App,
                     std::optional<int> CacheLimit = std::nullopt,
                     std::optional<Interpretation> Interpret = std::nullopt)
        : App(std::move(App)), CacheLimit(std::move(CacheLimit)),
          Interpret(std::move(Interpret)) {}
    Options() = default;
  };

  VirtualMachine(Options Opts,
                 // TODO: make Cache a smart pointer
                 std::optional<static_code::Cache> StaticCodeCache,
                 const std::shared_ptr<wabt::Module> &M,
                 std::shared_ptr<CompilerClient> CC);
  ~VirtualMachine();

  // calling functions
  void start();
  void onJIT(u32 Fn);
  void onInterp(u32 Fn);

  // interpreter
  void interpret(u32 FunctionIndex);

  class LocalCodeCache {
    using Func = u32;
    using FuncsIt = std::list<Func>::iterator;

    const int Limit;
    std::list<Func> Funcs; // front = most recent, back = oldest
    std::unordered_map<u32, FuncsIt> FuncsIndex; // key -> list iterator

  public:
    explicit LocalCodeCache(const int Limit) : Limit(Limit) {
      if (Limit <= 0)
        Panic("Cache limit must be greater than 0");
    }
    ~LocalCodeCache() = default;

    std::optional<u32> get(const u32 Fn) {
      if (not Funcs.empty() and Funcs.front() == Fn) {
        return std::nullopt; // no eviction
      }

      // Fn exists, move it to front
      if (const auto It = FuncsIndex.find(Fn); It != FuncsIndex.end()) {
        Funcs.erase(It->second);
        Funcs.push_front(Fn);
        FuncsIndex[Fn] = Funcs.begin();
        return std::nullopt; // no eviction
      }

      ++Prof->Misses;

      // Fn doesn't exist; insert
      std::optional<u32> Evicted{};
      if (Funcs.size() >= Limit) {
        ++Prof->Evictions;
        auto Oldest = Funcs.back();
        FuncsIndex.erase(Oldest);
        Funcs.pop_back();
        Evicted = Oldest;
      }

      Funcs.push_front(Fn);
      FuncsIndex[Fn] = Funcs.begin();
      return Evicted;
    }

    auto str() const {
      std::ostringstream oss;
      oss << "{";
      for (const auto &Fn : Funcs)
        oss << fmt::format("{}, ", Fn);
      auto S = oss.str();
      if (S.size() > 1)
        S.resize(S.size() - 2); // trim last comma
      S += "}";
      return S;
    }

  private:
    struct Profiler {
      std::atomic<unsigned long> Misses{};
      std::atomic<unsigned long> Evictions{};

      ~Profiler() {
        fmt::println("[local-cache] misses={} evictions={}", Misses.load(),
                     Evictions.load());

        metric("local-cache.misses", std::to_string(Misses.load()));
        metric("local-cache.evictions", std::to_string(Evictions.load()));
      }
    };
    std::shared_ptr<Profiler> Prof{std::make_shared<Profiler>()};
  };

  std::optional<LocalCodeCache> Cache;
  void onFn(u32 Fn);

  using LinkedFunction = void (*)(void **);
  LinkedFunction link(u32 F, std::vector<char> &&Code);

  // startup-related
  // std::map<u32, ExecMode> FunctionsExecMode;
  std::map<u32, unsigned long> FunctionsStartup;
  void onStartUp(u32 Fn);

  std::vector<unsigned long> StaticSizes;

  // linear memory management
  void memoryFill(u32 Dest, u32 Value, u32 Size) const;
  void memoryCopy(u32 DstOffset, u32 SrcOffset, u32 Size) const;
  u32 memoryGrow(u32 Pages);
  u32 memorySize() const;

  // event handlers
  void onCall(u32 FIdx);
  void onReturn(u32 FIdx);
  void onStackOp(u32 Op, u32 Ty, u64 Val);
  void onInstruction(u32 Line);
  void onMemoryOp(u32 Op, u32 Ty, u64 Base, u64 Offset, u64 Addr);
  void onOutOfBoundMemory(u32 Line, u32 Ty, u64 Base, u64 Offset, u64 Addr);
  void onBranch(u32 Line, u32 Cond);

  // tracing
  void onString(const char *Str);

  // wasi functions
#define WASI_FUNCTION(NAME) void NAME();
#include "WASI.api.def"

#undef WASI_FUNCTION

  void VirtualCall(const u32 Fn) {
    ((void (*)(void **, u32))(JumpTable[Fn]))(Runtime.data(), Fn);
  }

private:
  // static data
  std::vector<char> ActivationFrame;
  std::vector<u32> Globals;
  std::vector<u32> IndirectJumpTable;
  std::vector<void *> JumpTable;
  std::vector<void *> FunctionPointers;
  unsigned NumOfWasiFunctions{};
  bool NotWasi(const u32 Fn) const { return Fn >= NumOfWasiFunctions; }
  char *LinearMemory{};
  u64 LinearMemorySize{};
  std::optional<u32> MaxPages;
  uvwasi_t *Wasi; // TODO: make it shared_ptr
  std::optional<FILE *> WasiIn;
  std::optional<FILE *> WasiOut;
  std::optional<FILE *> WasiErr;
  int StartFIdx;

  std::vector<void *> Runtime;
  std::shared_ptr<CompilerClient> CC;
  std::shared_ptr<llvm::orc::LLJIT> Linker;

  class Monitor {
    std::thread Thread;
    std::atomic<bool> Running{true};
    std::vector<std::pair<u32, uint64_t>> Samples;
    std::map<u32, uint64_t> SamplesCounter;
    std::map<u32, uint64_t> Frequencies;
    uint32_t SamplingInterval; // in microseconds
    std::shared_ptr<wabt::Module> M;

    template <typename T> class MonitoredValue {
      std::atomic<std::optional<T>> V{};

    public:
      MonitoredValue() = default;
      std::optional<T> read() const { return V.load(); }
      void write(const std::optional<T> New) { V = New; }
      T unset() {
        auto Current = V.load();
        V = std::nullopt;
        return Current.value();
      }
    };

    MonitoredValue<u32> Fn{};

  public:
    explicit Monitor(std::shared_ptr<wabt::Module> M)
        : SamplingInterval(10000), // 10 microseconds
          M(std::move(M)) {
      for (const auto &Export : this->M->exports)
        if (Export->kind == wabt::ExternalKind::Func and
            Export->name == "_start") {
          Fn.write(Export->var.index());
          break;
        }

      Thread = std::thread([this] {
        while (Running) {
          std::this_thread::sleep_for(microseconds(SamplingInterval));
          auto V = Fn.read();
          if (not V.has_value())
            continue;

          Samples.emplace_back(V.value(), NOW());

          if (auto Current = V.value(); SamplesCounter.contains(Current))
            SamplesCounter[Current]++;
          else
            SamplesCounter[Current] = 1;
        }
      });
    }

    void set(const u32 NewFn) {
      if (Frequencies.contains(NewFn))
        Frequencies[NewFn]++;
      else
        Frequencies[NewFn] = 1;
      Fn.write(NewFn);
    }

    // unset/reset is necessary for when we don't want to monitor while
    // compiling
    u32 unset() { return Fn.unset(); }
    void reset(const u32 Current) { Fn.write(Current); }

    ~Monitor() {
      Running = false;
      if (Thread.joinable())
        Thread.join();

      // json SamplesJ = json::array();
      // for (const auto &[Fn, Time] : Samples)
      //   SamplesJ.push_back({Fn, Time});
      // metric("samples", SamplesJ.dump());

      uint64_t TotalSamples = 0;
      for (const auto Sample : SamplesCounter | std::views::values)
        TotalSamples += Sample;
      std::vector<std::pair<u32, uint64_t>> SortedSamples(
          SamplesCounter.begin(), SamplesCounter.end());
      std::ranges::sort(SortedSamples, [](const auto &A, const auto &B) {
        return A.second > B.second;
      });

      // (id, freq, duration)
      nlohmann::json J;
      for (const auto Fn : Frequencies | std::views::keys) {
        auto Freq = Frequencies[Fn];
        auto Duration = SamplesCounter[Fn] * SamplingInterval * 1000; // in ns
        J.push_back({Fn, Freq, Duration});
      }
      metric("functions-frequency-and-duration", J.dump());

      // use the samples and for each function choose the last sample;
      std::map<u32, u64> FunctionLastSample;
      for (const auto &[Fn, Time] : Samples)
        FunctionLastSample[Fn] = Time;

      nlohmann::json LastSamplesJ;
      for (const auto &[Fn, Time] : FunctionLastSample)
        LastSamplesJ.push_back({Fn, Time});
      metric("ends", LastSamplesJ.dump());
    }
  };
  std::unique_ptr<Monitor> Mon;

  std::vector<std::shared_ptr<BranchSubscriber>> BrSubscribers;
  std::vector<std::shared_ptr<CallSubscriber>> CallSubscribers;
  std::vector<std::shared_ptr<ReturnSubscriber>> ReturnSubscribers;
  std::vector<std::shared_ptr<InstructionSubscriber>> InstructionSubscribers;
  std::vector<std::shared_ptr<StackSubscriber>> StackSubscribers;

  u32 RequestIDCounter = 0;

  // new API
  std::shared_ptr<wabt::Module> M;
  std::vector<std::string> MLines;

public:
  struct Profiler final {
    u64 InitStart{};
    u64 InitEnd{};
    u64 StaticStart{};
    u64 StaticEnd{};
    u64 Start{};
    u64 End{};

    using Time = const time_point<high_resolution_clock> &;
    struct Record {
      u32 Fn;
      u64 Start;
      u64 End;
      u64 Size;
    };
    std::vector<Record> Records;
    std::atomic<u64> Waiting{};

    void onFn(u32 FunctionIndex, Time Start, Time End, u64 Size) {
      auto S = Start.time_since_epoch().count();
      auto E = End.time_since_epoch().count();
      Waiting += E - S;
      Records.emplace_back(FunctionIndex, S, E, Size);
    }

    // copy-constructors
    Profiler() = default;
    Profiler(const Profiler &Other)
        : InitStart(Other.InitStart), InitEnd(Other.InitEnd),
          StaticStart(Other.StaticStart), StaticEnd(Other.StaticEnd),
          Start(Other.Start), End(Other.End), Records(Other.Records),
          Waiting(Other.Waiting.load()) {}

    ~Profiler() {
      json J;
      for (const auto &[Fn, Start, End, Size] : Records)
        J.push_back({Fn, Start, End - Start, Size});
      metric("waiting-and-sizes", J.dump());

      metric("start", std::to_string(Start));

      {
        std::vector<unsigned long> DynamicSize;
        for (const auto &[Fn, Start, End, Size] : Records)
          DynamicSize.push_back(Size);
        metric("dynamic-sizes", json(DynamicSize).dump());
      }
    }
  };
  std::shared_ptr<Profiler> Prof{std::make_shared<Profiler>()};

  std::string Application;
};
