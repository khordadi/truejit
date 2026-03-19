#include "DigitalSignatureService.h"
#include <Config.h>
#include <chrono>
#include <filesystem>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <string>
#include <vector>

SignatureService::SignatureService(const std::string &PrivateKeyPath,
                                   const std::string &PublicKeyPath) {
  if (not std::filesystem::exists(PrivateKeyPath))
    Panic("Private key file does not exist: " + PrivateKeyPath);
  if (not std::filesystem::exists(PublicKeyPath))
    Panic("Public key file does not exist: " + PublicKeyPath);

  Ctx = EVP_MD_CTX_new();
  FILE *PrivateKeyFile = fopen(PrivateKeyPath.c_str(), "r");
  PrivateKey = PEM_read_PrivateKey(PrivateKeyFile, nullptr, nullptr, nullptr);
  fclose(PrivateKeyFile);
  FILE *PublicKeyFile = fopen(PublicKeyPath.c_str(), "r");
  PublicKey = PEM_read_PUBKEY(PublicKeyFile, nullptr, nullptr, nullptr);
  fclose(PublicKeyFile);
}
SignatureService::~SignatureService() {
  EVP_MD_CTX_free(Ctx);
  EVP_PKEY_free(PrivateKey);
  EVP_PKEY_free(PublicKey);
}
std::vector<unsigned char>
SignatureService::sign(const std::vector<char> &Code) const {
  EVP_MD_CTX_init(Ctx);
  EVP_DigestSignInit(Ctx, nullptr, EVP_sha256(), nullptr, PrivateKey);
  EVP_DigestSignUpdate(Ctx, Code.data(), Code.size());
  size_t SignatureSize;
  EVP_DigestSignFinal(Ctx, nullptr, &SignatureSize);
  std::vector<unsigned char> Signature(SignatureSize);
  EVP_DigestSignFinal(Ctx, Signature.data(), &SignatureSize);
  return Signature;
}
bool SignatureService::verify(const std::vector<char> &Code,
                              const std::vector<u8> &Signature) const {
#ifdef PROFILE_SIGNATURE_SERVICE
  const auto Start = high_resolution_clock::now();
#endif
  EVP_MD_CTX_init(Ctx);
  EVP_DigestVerifyInit(Ctx, nullptr, EVP_sha256(), nullptr, PublicKey);
  EVP_DigestVerifyUpdate(Ctx, Code.data(), Code.size());
  const auto Result =
      EVP_DigestVerifyFinal(Ctx, Signature.data(), Signature.size());
#ifdef PROFILE_SIGNATURE_SERVICE
  const auto End = high_resolution_clock::now();
  const auto Duration = duration_cast<nanoseconds>(End - Start).count();
  Prof->Total += Duration;
#endif
  return Result;
}