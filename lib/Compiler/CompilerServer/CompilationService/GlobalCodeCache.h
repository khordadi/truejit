#pragma once
#include <list>
#include <thread>

std::string str(auto Keys) {
  std::ostringstream OSS;
  OSS << "[";
  for (auto &Key : Keys)
    OSS << fmt::format("{},", Key);
  auto S = OSS.str();
  if (S.size() > 1)
    S.resize(S.size() - 1); // trim last comma
  S += "]";
  return S;
}

class GlobalCodeCache {
  using Key = u32;
  using Value = std::vector<char>;
  using Item = std::pair<Key, Value>;
  using It = std::list<Item>::iterator;

  const int Limit;
  std::list<Item> Funcs;                  // front = most recent, back = oldest
  std::unordered_map<Key, It> FuncsIndex; // key -> list iterator
  std::mutex Mutex;

  std::unordered_map<Key, std::unique_ptr<std::once_flag>> Flags;
  std::mutex FlagsMutex;

  std::unique_ptr<std::once_flag> &flag(const Key Key) {
    std::lock_guard Lock(FlagsMutex);
    if (const auto It = Flags.find(Key); It != Flags.end())
      return It->second;

    auto Flag = std::make_unique<std::once_flag>();
    auto [It, _] = Flags.emplace(Key, std::move(Flag));
    return It->second;
  }

  std::vector<Key> keys() {
    std::lock_guard Lock(Mutex);
    std::vector<Key> Keys;
    Keys.reserve(Funcs.size());
    for (const auto &K : Funcs | std::views::keys)
      Keys.push_back(K);
    return Keys;
  }

  class Monitor {
    bool Stop = false;
    std::thread Thread;
    std::vector<std::pair<uint64_t, std::vector<u32>>> Observations;

    static auto now() { return steady_clock::now().time_since_epoch().count(); }
    static double gini(std::vector<uint64_t> Counts) {
      const uint64_t N = Counts.size();
      if (N == 0)
        return 0.0;

      std::ranges::sort(Counts);

      const auto Sum = std::accumulate(Counts.begin(), Counts.end(), 0ULL);
      if (Sum == 0)
        return 0.0;

      uint64_t WeightedSum = 0;
      for (size_t I = 0; I < N; ++I)
        WeightedSum += (2 * (I + 1) - N - 1) * Counts[I];

      return static_cast<double>(WeightedSum) / (N * static_cast<double>(Sum));
    }

  public:
    explicit Monitor(std::function<std::vector<u32>()> Observe,
                     milliseconds SamplingInterval = 10ms) {
      Thread = std::thread([this, Observe, SamplingInterval] {
        const auto Init = now();
        while (not Stop) {
          auto Timestamp = now() - Init;
          auto Cache = Observe();
          if (Cache.empty())
            continue;
          Observations.emplace_back(Timestamp, Cache);
          std::this_thread::sleep_for(SamplingInterval);
        }
      });
    }

    ~Monitor() {
      Stop = true;
      if (Thread.joinable())
        Thread.join();

      std::vector<std::pair<uint64_t, double>> Ginis;
      std::unordered_map<uint64_t, uint64_t> FuncsCounts;
      for (auto &&[Timestamp, Cache] : Observations) {
        std::vector<uint64_t> Counts;
        Counts.reserve(Cache.size());
        for (const auto Fn : Cache) {
          ++FuncsCounts[Fn];
          Counts.push_back(FuncsCounts[Fn]);
        }
        auto Gini = gini(std::move(Counts));
        Ginis.emplace_back(Timestamp, Gini);
      }

      json J;
      for (auto [Timestamp, Gini] : Ginis)
        J.push_back({Timestamp, Gini});

      metric("global-cache.gini", J.dump());
    }
  };

  // std::shared_ptr<Monitor> Mon;

public:
  explicit GlobalCodeCache(const std::optional<int> Limit)
      : Limit(Limit.value_or(std::numeric_limits<int>::max())) {
    // Mon = std::make_shared<Monitor>([this] { return keys(); }, 10ms);
  }

  ~GlobalCodeCache() = default;

  Value get(const Key Key, const std::function<Value()> &OnMiss) {
    {
      std::lock_guard Lock(Mutex);

      // If Key exists, move it to front and return its value
      if (const auto It = FuncsIndex.find(Key); It != FuncsIndex.end()) {
        Funcs.splice(Funcs.begin(), Funcs, It->second);
        const auto &[K, V] = *It->second;
        return V;
      }

      ++Prof->Misses;

      // If cache is full, evict the oldest item
      if (Funcs.size() >= Limit) {
        ++Prof->Evictions;
        auto &[K, V] = Funcs.back();
        FuncsIndex.erase(K);
        Funcs.pop_back();
        flag(K) = std::make_unique<std::once_flag>();
      }
    }

    std::call_once(*flag(Key), [this, &OnMiss, Key] {
      auto Code = OnMiss();
      ++Prof->Compilations;
      std::lock_guard Lock(Mutex);
      Funcs.push_front({Key, std::move(Code)});
      FuncsIndex[Key] = Funcs.begin();
    });

    return get(Key, OnMiss);
  }

  std::string str() { return ::str(Funcs | std::views::keys); }

  struct Profiler {
    std::atomic<u32> Misses{};
    std::atomic<u32> Evictions{};
    std::atomic<u32> Compilations{};

    ~Profiler() {
      metric("global-cache.misses", std::to_string(Misses.load()));
      metric("global-cache.evictions", std::to_string(Evictions.load()));
      metric("global-cache.compilations", std::to_string(Compilations.load()));
      // fmt::println("[global-cache] misses={} compilations={} evictions={}",
      //              Misses.load(), Compilations.load(), Evictions.load());
    }
  };
  std::shared_ptr<Profiler> Prof{std::make_shared<Profiler>()};
};