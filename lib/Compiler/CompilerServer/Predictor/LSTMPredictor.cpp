#include "Compiler/CompilerServer/Predictor/LSTMPredictor.h"
#include "Compiler/CompilerServer/Predictor/PredictionService.h"
#include "Globals.h"
#include <fmt/color.h>
#include <torch/script.h>
#include <utility>

// #define TRACING_PREDICTION_SERVICE

using namespace std::chrono;

LSTMPredictor::LSTMPredictor(const std::string &ModelPath) {
  // check path exists
  if (not std::filesystem::exists(ModelPath))
    Panic(fmt::format("Model file {} does not exist", ModelPath));
  try {
    Model = std::make_shared<torch::jit::script::Module>(
        torch::jit::load(ModelPath));
    Model->eval();
    // warmup
    Model->forward({torch::tensor({42}, torch::kLong).reshape({1, 1})});
  } catch (const c10::Error &Err) {
    Panic(fmt::format("{}\n{}", Err.msg(), Err.backtrace()));
  }
}
std::optional<u32> LSTMPredictor::predict(const u32 &History) {
  {
    ReadLock RL(CacheLock);
    if (Cache.contains(History))
      return Cache[History];
  }

  auto In = torch::tensor({static_cast<int64_t>(History)}, torch::kLong);
  In = In.reshape({1, 1});
  auto Top = Model->forward({In}).toTensor().softmax(1).topk(1);
  auto &[Val, Idx] = Top;
  const auto Probability = Val.item<float>();
  long Prediction = Idx.item<int64_t>();

#ifdef TRACING_PREDICTION_SERVICE
  print(fg(fmt::color::gold), "[lstm] {} => {} ({}%)\n", History, Prediction,
        static_cast<int>(Probability * 100));
  std::cout.flush();
#endif

  WriteLock WL(CacheLock);
  if (Probability > PROBABILITY_THRESHOLD)
    Cache[History] = Prediction;
  else
    Cache[History] = std::nullopt;
  return Cache[History];
}
std::vector<u32> LSTMPredictor::predict(const u32 &History, const u32 Depth) {
  assert(Depth > 0 && "Prediction depth should be greater than 0");
  std::vector<u32> Result;
  auto Current = History;
  for (u32 I = 0; I < Depth; ++I)
    if (auto Next = predict(Current); Next.has_value()) {
      auto NextVal = Next.value();
      Result.push_back(NextVal);
      Current = NextVal;
    } else
      break;
  return Result;
}