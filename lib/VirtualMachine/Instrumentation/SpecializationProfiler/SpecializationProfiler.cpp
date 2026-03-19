#include "SpecializationProfiler.h"
#include "Compiler/Specialization.h"
#include "ThreadPool.h"
#include <Compiler/CompilerServer/CompilationService/CompilationService.h>
#include <Compiler/LLVMCompiler/LLVMCompiler.h>
#include <fmt/color.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <variant>

#define THRESHOLD 100

using json = nlohmann::json;
using Clock = std::chrono::high_resolution_clock;

SpecializationProfiler::SpecializationProfiler(
    std::shared_ptr<wabt::Module> M, std::vector<char> &ActivationFrame)
    : Counter(M->funcs.size(), 0), Durations(M->funcs.size(), 0),
      TrackUniqueParams(M->funcs.size(), true),
      UniqueParamsList(M->funcs.size()), M(std::move(M)),
      ActivationFrame(ActivationFrame) {}

void SpecializationProfiler::update(const CallSubscriber::Event &Event) {
  const auto F = Event.FIdx;
  // count the call
  Counter[F]++;

  // unique params
  if (TrackUniqueParams[F]) {
    if (const auto Params = to_constants(M->funcs[F]->decl.sig.param_types,
                                         ActivationFrame.data());
        not Params.empty()) {
      if (not UniqueParamsList[F].contains(Params))
        UniqueParamsList[F][Params] = 0;
      UniqueParamsList[F][Params]++;
    }

    if (UniqueParamsList[F].size() > THRESHOLD)
      TrackUniqueParams[F] = false;
  }

  // start the timer
  const auto Now = Clock::now().time_since_epoch().count();
  CallStackStartTimes.push_back(Now);
}
void SpecializationProfiler::update(const ReturnSubscriber::Event &Event) {
  const auto F = Event.FIdx;

  // stop the timer
  const auto Now = Clock::now().time_since_epoch().count();
  const auto Start = CallStackStartTimes.back();
  CallStackStartTimes.pop_back();
  Durations[F] += Now - Start;
}

std::string GetParamsString(const wabt::TypeVector &Params) {
  std::string Name = "(";
  for (auto i = 0; i < Params.size(); ++i) {
    Name += Params[i].GetName();
    if (i + 1 < Params.size())
      Name += ", ";
  }
  Name += ")";
  return Name;
}

void SpecializationProfiler::ReportAll() {
  std::vector<FunctionRecord> Records;
  Records.reserve(Counter.size());
  for (u32 I = 0; I < Counter.size(); ++I)
    Records.emplace_back(I, M->funcs[I]->name, Counter[I],
                         static_cast<double>(Durations[I]) / 1'000'000'000,
                         UniqueParamsList[I]);

  // sort by duration, count, and size of unique params
  std::ranges::sort(Records, [](const auto &L, const auto &R) {
    return L.Duration > R.Duration or
           (L.Duration == R.Duration and L.Count > R.Count) or
           (L.Duration == R.Duration and L.Count == R.Count and
            L.Uniques.size() < R.Uniques.size());
  });

  const auto TotalDuration = Records.front().Duration;

  auto HasParameter = [](const auto &R) { return not R.Uniques.empty(); };
  auto CalledMoreThan = [](const auto &N) {
    return [N](const auto &R) { return R.Count > N; };
  };
  auto NumOfUniques = [](const auto &N) {
    return [N](const auto &R) { return R.Uniques.size() == N; };
  };
  auto NonWasi = [](const auto &R) {
    return not R.Name.starts_with("$__imported_wasi_snapshot_preview1_");
  };

  auto FilteredRecords = Records | std::views::filter(NonWasi) |
                         std::views::filter(HasParameter) |
                         std::views::filter(CalledMoreThan(1000)) |
                         // std::views::filter(CalledMoreThan(0)) |
                         std::views::filter(NumOfUniques(1));

  auto constexpr ColumnFormat = "{:>5} {:>15} {:>10} {:>5} {:>20}   {:<20}  {}";
  fmt::println(ColumnFormat, "ID", "Duration", "Calls", "Uniq",
               "Size Improvement", "Function", "Params");

  auto Compiler = LLVMCompiler{Compiler::ModuleOptions{M, {}, std::nullopt}};

  json J;
  for (const auto &[I, Name, Counts, Duration, Uniqs] : FilteredRecords) {
    auto DStr = fmt::format("{:>6.2f} {:>6.2f}%", Duration,
                            Duration / TotalDuration * 100);

    auto NameStr = Name;
    if (Name.size() > 20)
      NameStr = Name.substr(0, 17) + "...";

    assert(Uniqs.size() == 1 and "Only one unique param is expected");
    auto Top = Uniqs.begin()->first;

    auto Before = Compiler.compile({I});
    auto After = Compiler.compile({I, {}, UnconditionalStatic{Top}});
    auto AfterSize = static_cast<double>(After.size());
    auto BeforeSize = static_cast<double>(Before.size());
    auto SizeImprove = static_cast<int>(100.0 - AfterSize / BeforeSize * 100);
    auto Size =
        fmt::format("{} -> {} {:>3}%", BeforeSize, AfterSize, SizeImprove);

    auto ParamsStr = GetParamsString(M->funcs[I]->decl.sig.param_types);
    auto UniquesStr = TrackUniqueParams[I] ? fmt::format("{}", Uniqs.size())
                                           : fmt::format("+{}", THRESHOLD);
    fmt::println(ColumnFormat, I, DStr, Counts, UniquesStr, Size, NameStr,
                 ParamsStr);

    fmt::println("{}", serialize(Top));

    auto JJ = json::object();
    JJ["idx"] = I;
    JJ["name"] = Name;
    JJ["duration"] = Duration;
    JJ["calls"] = Counts;
    JJ["size_before"] = BeforeSize;
    JJ["size_after"] = AfterSize;
    JJ["params"] = ParamsStr;
    J.push_back(JJ);
  }
  std::cout.flush();

  {
    auto ReportPath = "/tmp/report.json";
    std::ofstream O(ReportPath);
    O << J.dump(1);
    O.close();
  }

  auto Selected = [](const auto &R) {
    return R.Uniques.size() == 1 and R.Count > 1000;
  };

  // print(fg(fmt::color::gold), "Printing unique params for functions\n");
  json Funcs = json::array();
  for (auto &[I, Name, C, D, P] : Records | std::views::filter(Selected))
    // Funcs.push_back({I, Name, serialize(P.begin()->first)});
    Funcs.push_back(I);
  json OutJ;
  OutJ["specialize"] = Funcs;

  auto SpecializationPlanPath = "/tmp/plan.json";
  std::ofstream O(SpecializationPlanPath);
  O << OutJ.dump();
  O.close();
}

SpecializationProfiler::~SpecializationProfiler() { ReportAll(); }