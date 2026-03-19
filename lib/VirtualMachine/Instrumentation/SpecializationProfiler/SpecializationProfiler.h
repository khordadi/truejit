#pragma once
#include "Globals.h"
#include "VirtualMachine/Instrumentation/Subscribers.h"
#include <Compiler/Specialization.h>
#include <fmt/color.h>
#include <map>
#include <variant>
#include <wabt/ir.h>

class SpecializationProfiler final : public CallSubscriber,
                                     public ReturnSubscriber {
public:
  explicit SpecializationProfiler(std::shared_ptr<wabt::Module> M,
                                  std::vector<char> &ActivationFrame);
  void update(const CallSubscriber::Event &Event) override;
  void update(const ReturnSubscriber::Event &Event) override;
  ~SpecializationProfiler() override;

private:
  // per functions
  std::vector<u64> Counter;
  std::vector<u64> Durations;
  std::vector<bool> TrackUniqueParams;

  using UniqueParams = std::map<std::vector<WasmValue>, u64>;
  std::vector<UniqueParams> UniqueParamsList;

  struct FunctionRecord {
    u32 Idx;
    std::string Name;
    u64 Count;
    double Duration;
    UniqueParams Uniques;
  };

  std::shared_ptr<wabt::Module> M;
  std::vector<char> &ActivationFrame;

  std::vector<u64> CallStackStartTimes;

  void ReportAll();
};
