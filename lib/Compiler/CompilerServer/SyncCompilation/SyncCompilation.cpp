#include "SyncCompilation.h"
#include <functional>
#include <optional>
#include <vector>
namespace sync_compilation {

Strategy of(std::string Str) {
  if (Str == "jit")
    return JustInTime{};
  if (Str == "aot")
    return AheadOfTime{};
  if (not Str.starts_with("predictive."))
    Panic(fmt::format("Invalid sync strategy '{}'", Str));
  auto Pred = Str.substr(std::strlen("predictive."));
  if (not Pred.starts_with("lstm"))
    Panic(fmt::format("Invalid predictor '{}'", Pred));
  const auto Depth = std::stoi(Pred.substr(std::strlen("lstm-")));
  return Predictive{Depth};
}
std::string str(Strategy S) {
  struct {
    // clang-format off
    std::string operator()(const JustInTime &) const { return "jit"; }
    std::string operator()(const AheadOfTime &) const { return "aot"; }
    std::string operator()(const Predictive &P) const { return fmt::format("predictive.lstm-{}", P.Depth); }
    // clang-format on
  } Visitor;
  return std::visit(Visitor, S);
}
Service::Service(Strategy S, std::shared_ptr<WasmService> WS,
                 std::shared_ptr<CompilationService> CS,
                 std::shared_ptr<PredictionService> PS)
    : S(std::move(S)), WS(std::move(WS)), CS(std::move(CS)), PS(std::move(PS)) {
  const auto Cores = std::thread::hardware_concurrency() - 4;
  TP = std::make_shared<ThreadPool>(Cores);
  // fmt::println("[thread-pool] using 1 thread");
  // TP = std::make_shared<ThreadPool>(1);
}

std::function<std::vector<Response>(Request)>
Service::operator()(const std::string &Application) {
  const auto CompileFn = [this, C = CS->compiler(Application)](auto Req) {
    return CS->compileUsing(C)(std::move(Req));
  };

  const auto CompileAsyncFn = [this, C = std::move(CompileFn)](auto Req) {
    return TP->enqueue([C, Req = std::move(Req)] { return C(std::move(Req)); });
  };

  if (std::holds_alternative<JustInTime>(S))
    return [CompileFn](auto R) {
      const auto F = R.History.back();
      const auto Code = CompileFn(CompilationService::Request{F, R.ActivationFrame});
      return std::vector{Response{F, Code}};
    };

  // TODO: remove 'this' from the lambda capture list, and calculate the Ins outside of the lambda
  if (std::holds_alternative<AheadOfTime>(S))
    return [this, CompileAsyncFn, Application](auto R) {
      auto Ins = std::vector{R.History.back()};

      for (const auto F : WasmService::NonWasiFunctions(WS->get(Application)))
        if (F != R.History.back())
          Ins.push_back(F);

      std::vector<std::future<std::vector<char>>> Futures;
      Futures.reserve(Ins.size());
      std::ranges::transform(
          Ins, std::back_inserter(Futures), [&](const auto &In) {
            return CompileAsyncFn(CompilationService::Request{In});
          });

      std::vector<Response> Outs;
      Outs.reserve(Futures.size());
      std::transform(
          Futures.begin(), Futures.end(), Ins.begin(), std::back_inserter(Outs),
          [](auto &Future, auto &In) { return Response{In, Future.get()}; });
      return Outs;
    };

  if (std::holds_alternative<Predictive>(S))
    return [this, CompileAsyncFn, Application](auto R) {
      const auto F = R.History.back();
      auto Ins = std::vector{F};

      auto Depth = std::get<Predictive>(S).Depth;
      const auto Predictor = [this, Predictor = PS->predictor(Application),
                              Depth](const u32 Fn) {
        return PS->predictUsing({Predictor, Depth})(Fn);
      };

      const auto Predictions = Predictor(F);
      Ins.insert(Ins.end(), Predictions.begin(), Predictions.end());

      std::vector<std::future<std::vector<char>>> Futures;
      Futures.reserve(Ins.size());
      std::ranges::transform(
          Ins, std::back_inserter(Futures), [&](const auto &In) {
            return CompileAsyncFn(CompilationService::Request{In});
          });

      std::vector<Response> Outs;
      Outs.reserve(Futures.size());
      std::transform(
          Futures.begin(), Futures.end(), Ins.begin(), std::back_inserter(Outs),
          [](auto &Future, auto &In) { return Response{In, Future.get()}; });

      return Outs;
    };

  Panic("Not implemented");
}
} // namespace sync_compilation