#pragma once

#include "../CompilationService/CompilationService.h"
#include "../Predictor/PredictionService.h"
#include "Globals.h"

namespace sync_compilation {
struct Request {
  std::vector<u32> History;
  char *ActivationFrame{};
};

struct Response {
  u32 Fn;
  std::vector<char> Code;
};

using CompileFn = std::function<std::vector<Response>(Request)>;

struct JustInTime {};
struct AheadOfTime {
  std::set<u32> Functions;
};
struct Predictive {
  int Depth;
};
using Strategy = std::variant<JustInTime, AheadOfTime, Predictive>;

Strategy of(std::string Str);

std::string str(Strategy S);

class Service {
  Strategy S;
  std::shared_ptr<WasmService> WS;
  std::shared_ptr<CompilationService> CS;
  std::shared_ptr<PredictionService> PS;
  std::shared_ptr<ThreadPool> TP;

public:
  Service(Strategy S, std::shared_ptr<WasmService> WS,
          std::shared_ptr<CompilationService> CS,
          std::shared_ptr<PredictionService> PS);

  CompileFn operator()(const std::string &Application);
};

} // namespace sync_compilation