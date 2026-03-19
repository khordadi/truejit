#pragma once
#include "Globals.h"

#include <fmt/color.h>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <variant>
#include <vector>
#include <wabt/type.h>

using json = nlohmann::json;

using SpecializationStrategy = std::set<u32>;
inline SpecializationStrategy
ParseSpecializationStrategy(const std::string &Str = "") {
  std::ifstream File("/tmp/specs.json");
  if (not File.is_open())
    Panic("Failed to open /tmp/specs.json");
  json J;
  File >> J;

  fmt::print("#specs: {}\n", J.size());

  SpecializationStrategy Plan;
  for (const auto &Spec : J) {
    assert(Spec["params_list"].size() == 1 and
           "Only one param list is supported");
    Plan.insert(Spec["idx"].get<u32>());
  }
  return Plan;
}
inline std::string GetName(const SpecializationStrategy &SS) {
  return fmt::format("{}-functions", SS.size());
}

using WasmValue = std::variant<u32, u64, f32, f64>;
struct to_type {
  wabt::Type operator()(u32) const { return wabt::Type::I32; }
  wabt::Type operator()(u64) const { return wabt::Type::I64; }
  wabt::Type operator()(f32) const { return wabt::Type::F32; }
  wabt::Type operator()(f64) const { return wabt::Type::F64; }
};

struct to_value {
  // clang-format off
  std::string operator()(const u32 V) const { return std::to_string(V); }
  std::string operator()(const u64 V) const { return std::to_string(V); }
  std::string operator()(const f32 V) const { return std::to_string(*reinterpret_cast<const u32 *>(&V)); }
  std::string operator()(const f64 V) const { return std::to_string(*reinterpret_cast<const u64 *>(&V)); }
  // clang-format on
};

inline std::string serialize(const WasmValue &V) {
  const auto Ty = std::visit(to_type{}, V);
  const auto Val = std::visit(to_value{}, V);
  return fmt::format("{}:{}", Ty.GetName(), Val);
}
inline WasmValue deserialize(const std::string &S) {
  const auto Col = S.find(':');
  const auto Ty = std::string(S.begin(), S.begin() + Col);
  if (Ty == "f32" or Ty == "f64")
    Panic("Floating point deserialization is not supported");

  const auto Val = std::stoul(std::string(S.begin() + Col + 1, S.end()));
  if (Ty == "i32")
    return static_cast<u32>(Val);
  if (Ty == "i64")
    return Val;

  Panic("Unsupported type");
}

inline std::vector<WasmValue> deserialize(const wabt::TypeVector &Tys,
                                          char *ActivationFrame) {
  if (Tys.empty())
    return {};

  std::vector<WasmValue> Vs;
  unsigned Offset = 0;
  for (const auto &Ty : Tys) {
    switch (Ty) {
    case wabt::Type::I32: {
      Vs.emplace_back(*reinterpret_cast<const u32 *>(ActivationFrame + Offset));
      Offset += sizeof(u32);
      break;
    }
    case wabt::Type::I64: {
      Vs.emplace_back(*reinterpret_cast<const u64 *>(ActivationFrame + Offset));
      Offset += sizeof(u64);
      break;
    }
    case wabt::Type::F32:
    case wabt::Type::F64: {
      Panic("Floating point deserialization is not supported");
    }
    default:
      Panic("Unsupported type");
    }
  }
  return Vs;
}

inline std::string serialize(const std::vector<WasmValue> &V) {
  std::string S = "[";
  for (const auto &Val : V)
    S += fmt::format("{}, ", serialize(Val));
  if (S.size() > 1)
    S.pop_back(), S.pop_back();
  S += "]";
  return S;
}

inline void deserialize(std::string S, std::vector<WasmValue> &Vs) {
  // e.g., [i32:1, i64:2, i32:31, i32:5, i64:7]
  if (S.empty() || S.front() != '[' || S.back() != ']')
    Panic("Invalid list format");

  S = S.substr(1, S.size() - 2); // strip '[' and ']'
  std::stringstream ss(S);
  std::string item;

  while (std::getline(ss, item, ',')) {
    // Trim spaces
    size_t start = item.find_first_not_of(" \t\n");
    size_t end = item.find_last_not_of(" \t\n");
    if (start == std::string::npos)
      continue;
    item = item.substr(start, end - start + 1);

    if (!item.empty())
      Vs.push_back(deserialize(item));
  }
}

using SpecializationCandidate = std::vector<WasmValue>;
using Functions = std::map<std::string, std::vector<SpecializationCandidate>>;

inline std::vector<WasmValue> to_constants(const wabt::TypeVector &Types,
                                           char const *Frame) {
  std::vector<WasmValue> Constants;
  for (const auto Ty : Types) {
    switch (Ty) {
    case wabt::Type::I32:
      Constants.emplace_back(*reinterpret_cast<const u32 *>(Frame));
      Frame += sizeof(u32);
      break;
    case wabt::Type::I64:
      Constants.emplace_back(*reinterpret_cast<const u64 *>(Frame));
      Frame += sizeof(u64);
      break;
    case wabt::Type::F32:
      Constants.emplace_back(*reinterpret_cast<const f32 *>(Frame));
      Frame += sizeof(f32);
      break;
    case wabt::Type::F64:
      Constants.emplace_back(*reinterpret_cast<const f64 *>(Frame));
      Frame += sizeof(f64);
      break;
    default:
      Panic("Unsupported type");
    }
  }
  return Constants;
}

struct Conditional {
  std::vector<std::vector<WasmValue>> ParamsList;
};
struct SpecializationChild {
  std::vector<WasmValue> Params;
};

struct UnconditionalDynamic {
  char *ActivationFrame{};
};

struct UnconditionalStatic {
  std::vector<WasmValue> Params;
};

using Specialization = std::variant<Conditional, SpecializationChild,
                                    UnconditionalDynamic, UnconditionalStatic>;

struct SpecializationPlan {
  std::set<u32> Plan;
  explicit SpecializationPlan(std::set<u32> &&Plan) : Plan(std::move(Plan)) {}
};