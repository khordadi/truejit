#pragma once

#include "Globals.h"
#include "VirtualMachine/Instrumentation/Subscribers.h"

#include <map>
#include <optional>
#include <wabt/ir.h>

class CallProfiler final : public CallSubscriber, public ReturnSubscriber {
  using Clock = std::chrono::high_resolution_clock;

public:
  explicit CallProfiler(std::shared_ptr<wabt::Module> M) : M(std::move(M)) {
    // Durations = std::vector<u64>(this->M->funcs.size(), 0);
    // Start = Clock::now().time_since_epoch().count();
    // // reserve space for 10_000_000_000 events
    // Events.reserve(10'000'000'000);
    PreviousTime = Clock::now().time_since_epoch().count();
    CurrentFunction = -1;
    Stack.push_back(CurrentFunction);
  }
  void update(const CallSubscriber::Event &Event) override;
  void update(const ReturnSubscriber::Event &Event) override;
  ~CallProfiler() override;

private:
  std::shared_ptr<wabt::Module> M;
  // version 1
  // std::vector<u64> CallsStack;
  // std::vector<u64> Durations;
  // u64 Start;
  // version 2
  struct Event {
    u64 Time;
    u32 FIdx;
    bool Entering;

    Event(const u64 Time, const u32 FIdx, const bool Entering)
        : Time(Time), FIdx(FIdx), Entering(Entering) {}
  };
  // std::vector<Event> Events;

  std::map<u32, u64> Durations;
  std::vector<u32> Stack;
  u32 CurrentFunction;
  u64 PreviousTime;
};
