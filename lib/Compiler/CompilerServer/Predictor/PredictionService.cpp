#include "PredictionService.h"
#include "Config.h"
#include "LSTMPredictor.h"
#include <functional>

inline std::string GetModelPath(const std::string &Application) {
  auto ModelPath = Application;
  if (ModelPath.ends_with(".wasm"))
    ModelPath.replace(ModelPath.find(".wasm"), 5, ".pt");
  if (ModelPath.ends_with(".wat"))
    ModelPath.replace(ModelPath.find(".wat"), 4, ".pt");
  if (ModelPath.find("benchmarks") != std::string::npos)
    ModelPath.replace(ModelPath.find("benchmarks"), 10, "models");
  if (not std::filesystem::exists(ModelPath))
    Panic(fmt::format("Model file {} does not exist", ModelPath));
  return ModelPath;
}

std::shared_ptr<Predictor>
PredictionService::predictor(const std::string &Application) {
  WriteLock WL(PredictorsLock);
  if (not Predictors.contains(Application))
    Predictors[Application] =
        std::make_shared<LSTMPredictor>(GetModelPath(Application));
  return Predictors[Application];
}

std::function<std::vector<u32>(u32)>
PredictionService::predictUsing(PredictorOptions Opts) const {
  auto [Predictor, Depth] = std::move(Opts);
  return [this, Predictor, Depth](const u32 F) {
#ifdef PROFILE_PREDICTION_SERVICE
    const auto Start = NOW();
#endif

    auto Predictions = Predictor->predict(F, Depth);

#ifdef PROFILE_PREDICTION_SERVICE
    const auto End = NOW();
    Prof->onPredict(Start, End);
#endif

    return Predictions;
  };
}