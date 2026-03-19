#pragma once

#include "Globals.h"
#include "VirtualMachine/Instrumentation/Subscribers.h"

class BranchProfiler final : public BranchSubscriber {
public:
  void update(const Event &Event) override;
  ~BranchProfiler() override;

private:
  struct BranchProfile {
    u32 True = 0;
    u32 False = 0;
  };
  std::unordered_map<u32, BranchProfile> Branches;
};
