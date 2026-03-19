#pragma once

#include "Globals.h"
#include <atomic>
#include <filesystem>
#include <fmt/format.h>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>

using json = nlohmann::json;
using namespace std::chrono;

class Predictor {
public:
  virtual ~Predictor() = default;
  virtual std::optional<u32> predict(const u32 &History) = 0;
  virtual std::vector<u32> predict(const u32 &History, u32 Depth) = 0;
};

class PredictionService {
public:
  struct PredictorOptions {
    std::shared_ptr<Predictor> P;
    int Depth;
  };
  PredictionService() = default;
  std::shared_ptr<Predictor> predictor(const std::string &Application);
  std::function<std::vector<u32>(u32)>
  predictUsing(PredictorOptions Opts) const;

  struct Profiler {
    std::atomic<uint64_t> Total{};
    void onPredict(auto Start, auto End) { Total += End - Start; }
  };

  std::shared_ptr<Profiler> Prof = std::make_shared<Profiler>();

private:
  std::map<std::string, std::shared_ptr<Predictor>> Predictors;
  Lock PredictorsLock;
};
