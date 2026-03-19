#include "CallProfiler.h"
#include <fmt/color.h>
#include <fmt/printf.h>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void CallProfiler::update(const CallSubscriber::Event &Event) {
  // Version 1
  // const auto Now = Clock::now().time_since_epoch().count();
  // CallsStack.push_back(Now);

  // Version 2
  // Events.emplace_back(Clock::now().time_since_epoch().count(), Event.FIdx,
  // true);

  const auto Time = Clock::now().time_since_epoch().count();
  if (not Durations.contains(CurrentFunction))
    Durations[CurrentFunction] = 0;
  Durations[CurrentFunction] += Time - PreviousTime;
  PreviousTime = Time;

  const auto Func = Event.FIdx;
  Stack.push_back(Func);
  CurrentFunction = Func;
}
void CallProfiler::update(const ReturnSubscriber::Event &Event) {
  const auto Time = Clock::now().time_since_epoch().count();
  if (not Durations.contains(CurrentFunction))
    Durations[CurrentFunction] = 0;
  Durations[CurrentFunction] += Time - PreviousTime;
  PreviousTime = Time;

  Stack.pop_back();
  CurrentFunction = Stack.back();
}

CallProfiler::~CallProfiler() {
  if (CurrentFunction != -1)
    Panic("[call-profiler] current function is not -1");

  if (Stack.size() != 1)
    Panic("[call-profiler] stack size is not 1");

  Durations.erase(-1); // remove -1 from the Durations

  auto Path = std::filesystem::path("/tmp/calls.json");
  if (not exists(Path.parent_path()))
    create_directories(Path.parent_path());

  json J;
  for (const auto &[FIdx, Duration] : Durations)
    J[std::to_string(FIdx)] = Duration;

  std::ofstream O(Path);
  O << J.dump(2);

  // sort by duration and print the results
  // std::vector<std::pair<u32, u64>> Sorted;
  // Sorted.reserve(Durations.size());
  // for (const auto &[FIdx, Duration] : Durations) {
  //   Sorted.emplace_back(FIdx, Duration);
  // }
  // std::ranges::sort(
  //     Sorted, [](const auto &L, const auto &R) { return L.second > R.second; });
  //
  // for (const auto &[FIdx, Duration] : Sorted) {
  //   auto Name = M->funcs[FIdx]->name;
  //   if (Name.size() > 70)
  //     Name = Name.substr(0, 67) + "...";
  //   fmt::print("{:.6f} {}\n", Duration / 1'000'000'000.0, Name);
  // }

  // print sum of durations
  const auto Sum = std::accumulate(
      Durations.begin(), Durations.end(), 0.0l,
      [](const auto &Acc, const auto &P) { return Acc + P.second; });

  fmt::print("[exec+profiling] {:.1f}\n", Sum / 1'000'000'000.0);

  // Version 2
  // std::ofstream Times("/tmp/call-profiler.txt");
  // for (const auto &E : Events) {
  //   auto Time = E.Time - Events.front().Time;
  //   auto FIdx = E.FIdx;
  //   auto Entering = E.Entering;
  //   Times << fmt::format("{} {} {}\n", Time, FIdx, Entering);
  // }
  //
  // std::ofstream FuncNames("/tmp/call-profiler.funcs.json");
  // json J;
  // for (auto I = 0; I < M->funcs.size(); ++I) {
  //   auto Name = M->funcs[I]->name;
  //   J[I] = Name;
  // }
  // FuncNames << J.dump(2);
}