#pragma once

#include <algorithm>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <set>
#include <shared_mutex>
#include <vector>

using i8 = char;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = unsigned char;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

using Lock = std::shared_mutex;
using ReadLock = std::shared_lock<Lock>;
using WriteLock = std::unique_lock<Lock>;

#define Panic(Msg)                                                             \
  do {                                                                         \
    std::cerr << (Msg) << std::endl;                                           \
    std::exit(EXIT_FAILURE);                                                   \
  } while (false)

constexpr auto LinearMemoryBaseAddress = 0x600000000000;
constexpr auto FourGigaBytes = 0x100000000; // 4GB

template <typename... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

// clang-format off
#define NOW() std::chrono::high_resolution_clock::now().time_since_epoch().count()
// clang-format on

#include <filesystem>
#include <fmt/format.h>
#include <fstream>

inline void metric(std::string Name, std::string Value) {
  auto Dir = std::filesystem::path("/tmp/metric/");
  if (not std::filesystem::exists(Dir))
    std::filesystem::create_directories(Dir);

  if (std::getenv("METRICS_DIR") != nullptr)
    Dir = std::filesystem::path(std::getenv("METRICS_DIR"));

  auto Path = Dir / fmt::format("{}.txt", Name);
  std::ofstream Out{Path, std::ios::out | std::ios::trunc};
  if (not Out.is_open())
    Panic(fmt::format("Failed to open {} for writing", Path.string()));
  Out << Value << std::endl;
  Out.close();
}

std::string SortAndStringify(const auto &Plan) {
  std::vector<u32> Sorted;
  for (const auto &Item : Plan)
    Sorted.push_back(Item);
  std::ranges::sort(Sorted);
  std::string Str = "[";
  for (const auto &Item : Sorted)
    Str += fmt::format("{}, ", Item);
  if (Str.size() > 1)
    Str.pop_back(), Str.pop_back();
  Str += "]";
  return Str;
}

inline std::vector<u32> ReadPlan(const std::string &Record) {
  auto Path = std::filesystem::path("/tmp/plan.json");
  if (std::getenv("PLAN") != nullptr)
    Path = std::filesystem::path(std::getenv("PLAN"));

  nlohmann::json J;
  std::ifstream IFS(Path);
  IFS >> J;
  J = J.at(Record);

  std::vector<u32> Plan;
  for (const auto &Item : J)
    Plan.push_back(Item.get<u32>());

  return Plan;
}

// vector to set
template <typename T> std::set<T> VecToSet(const std::vector<T> &Vec) {
  return std::set<T>(Vec.begin(), Vec.end());
}