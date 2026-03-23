#pragma once

#include <Globals.h>
#include <atomic>
#include <memory>
#include <openssl/types.h>
#include <vector>

using namespace std::chrono;

class SignatureService {
public:
  struct Profiler {
    std::atomic<u64> Total{};
  };

  explicit SignatureService(
      const std::string &PrivateKeyPath = PROJECT_ROOT / "keys/private_key.pem",
      const std::string &PublicKeyPath = PROJECT_ROOT / "keys/public_key.pem");
  ~SignatureService();
  std::vector<unsigned char> sign(const std::vector<char> &Code) const;
  bool verify(const std::vector<char> &Code,
              const std::vector<u8> &Signature) const;

  std::shared_ptr<Profiler> Prof = std::make_shared<Profiler>();

private:
  EVP_MD_CTX *Ctx;
  EVP_PKEY *PrivateKey;
  EVP_PKEY *PublicKey;
};
