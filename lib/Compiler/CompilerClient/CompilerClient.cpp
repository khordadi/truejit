#include "Compiler/CompilerClient/CompilerClient.h"
#include "Compiler/CompilerServer/CompilerServer.h"

// #define TRACE_COMPILER_CLIENT

CompilerClient::CompilerClient(std::shared_ptr<CompilerServer> CS,
                               Options Options, std::string Application)
    : Application(std::move(Application)), CS(std::move(CS)),
      CodeCache(std::make_shared<Cache>(Options.CacheLimit)) {}

void CompilerClient::connect() {
  const auto [ID] = CS->connect({Application});
  SessionID = ID;
}
void CompilerClient::disconnect() const { CS->disconnect({SessionID}); }

CompilerClient::~CompilerClient() = default;

CompilerClient::Response CompilerClient::get(Request Req) {
#ifdef TRACE_COMPILER_CLIENT
  fmt::println("[client] req={}", Req.Fn);
#endif
  const auto ID = RequestIDCounter++;
  auto [Fn] = Req;

  History.push_back(Fn);

  if (auto Code = CodeCache->get(Fn)) {
#ifdef TRACE_COMPILER_CLIENT
    fmt::println("[client] req={} cache hit", Req.Fn);
#endif
    return *Code;
  }

#ifdef TRACE_COMPILER_CLIENT
  fmt::println("[client] req={} cache miss", Req.Fn);
#endif

  const auto [Fs] = CS->compile({SessionID, ID, History, ActivationFrame});
  for (const auto &[F, Code] :
       Fs | std::views::drop(1) | std::views::take(CodeCache->capacity())) {
    CodeCache->add(F, Code);
  }

  return Fs.begin()->Code;
}