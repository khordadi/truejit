#pragma once
#include "Globals.h"
#include <atomic>
#include <chrono>
#include <fmt/printf.h>
#include <memory>
#include <optional>
#include <vector>

using namespace std::chrono;

class CompilerServer;

class CompilerClient {
public:
  struct Options {
    std::optional<int> CacheLimit{};
    Options() = default;
    explicit Options(std::optional<int> CacheLimit)
        : CacheLimit(std::move(CacheLimit)) {}
  };

  char *ActivationFrame{};
  void activation_frame(char *ActivationFrame) {
    this->ActivationFrame = ActivationFrame;
  }

  struct Request {
    u32 Fn;
  };

  using Response = std::vector<char>;

  explicit CompilerClient(std::shared_ptr<CompilerServer> CS, Options Options,
                          std::string Application);
  ~CompilerClient();

  void connect();
  void disconnect() const;
  Response get(Request Req);

  std::vector<u32> History;

private:
  struct Cache {
    int Limit{};
    std::unordered_map<u32, std::vector<char>> Codes{};
    u32 Last{};

    int Hits{};
    int Misses{};
    int Writes{};
    int Evictions{};

    explicit Cache(const std::optional<int> Limit)
        : Limit(Limit.value_or(std::numeric_limits<int>::max())) {}

    ~Cache() {}

    int capacity() const { return Limit; }

    void add(const u32 F, std::vector<char> Code) {
      Writes++;
      if (Codes.size() >= Limit) {
        Evictions++;
        Codes.erase(Last);
      }
      Last = F;
      Codes[F] = std::move(Code);
    }

    std::optional<std::vector<char>> get(const u32 F) {
      if (not Codes.contains(F)) {
        Misses++;
        return std::nullopt;
      }
      Hits++;
      auto Code = Codes[F];
      Codes.erase(F);
      return Code;
    }
  };

public:
  std::shared_ptr<Cache> cache() const { return CodeCache; }

private:
  // application-related
  const std::string Application;
  u32 SessionID{};

  u32 RequestIDCounter{};

  // compiler-related
  std::shared_ptr<CompilerServer> CS;
  std::shared_ptr<Cache> CodeCache;
};
