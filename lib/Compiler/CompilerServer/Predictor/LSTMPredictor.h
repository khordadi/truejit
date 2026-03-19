#pragma once

#include "Globals.h"
#include "PredictionService.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#define PROBABILITY_THRESHOLD 0.8

namespace torch::jit {
class Module;
}

class LSTMPredictor final : public Predictor {
public:
  explicit LSTMPredictor(const std::string &ModelPath);
  std::optional<u32> predict(const u32 &History) override;
  std::vector<u32> predict(const u32 &History, const u32 Depth) override;

private:
  std::shared_ptr<torch::jit::Module> Model;
  std::unordered_map<u32, std::optional<u32>> Cache;
  Lock CacheLock;
};