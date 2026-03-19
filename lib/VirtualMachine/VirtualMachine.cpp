#include "VirtualMachine.h"
#include "Compiler/CompilerServer/CompilationService/GlobalCodeCache.h"
#include "Config.h"
#include "Instrumentation/BranchProfiler/BranchProfiler.h"
#include "Instrumentation/CallProfiler/CallProfiler.h"
#include "Instrumentation/SpecializationProfiler/SpecializationProfiler.h"
#include "Instrumentation/Subscribers.h"
#include "Instrumentation/WebAssemblyPrinter/WebAssemblyPrinter.h"
#include "Logging.h"
#include "ThreadPool.h"
#include "VirtualMachine/VMCalls.h"
#include "VirtualMachine/VirtualMachine.h"
#include "VirtualMachineCompilerProtocol.h"
#include "Wasm/WasmService.h"
#include <DigitalSignature/DigitalSignatureService.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <iostream>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITLink/JITLinkMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/MapperJITLinkMemoryManager.h>
#include <llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Support/SmallVectorMemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <nlohmann/json.hpp>
#include <queue>
#include <sys/mman.h>
#include <unordered_set>
#include <utility>
#include <wabt/cast.h>
#include <wabt/common.h>

// #define DEBUG_INTERPRETER
// #define TRACE_LINKER

#include <uvwasi.h>

using nlohmann::json;
using namespace llvm;
using namespace llvm::orc;
using namespace llvm::object;

ExitOnError ExitOnErr;

VirtualMachine::VirtualMachine(
    Options Opts, std::optional<static_code::Cache> StaticCodeCache,
    // TODO: M is a dependency of tracing. instantiate tracer before
    // VirtualMachine, and pass as an option?!
    const std::shared_ptr<wabt::Module> &M, std::shared_ptr<CompilerClient> CC)
    : CC(std::move(CC)), M(M) {
#ifdef PROFILE_VIRTUAL_MACHINE
  Prof->InitStart = NOW();
#endif

  auto [App, CacheLimit, Interp] = std::move(Opts);

  if (CacheLimit)
    Cache.emplace(*CacheLimit);

  Application = App.application();

  auto [PreOpens, AppArgs] = std::move(App);

  // chdir to the application directory
  if (not PreOpens.empty())
    chdir(PreOpens.front().c_str());

  // initialize uvwasi
  uvwasi_options_t Inits{};

  if (const auto HasInputRedirect = std::ranges::find(AppArgs, "<");
      HasInputRedirect != AppArgs.end()) {
    // change stdin to file after <
    WasiIn = fopen((HasInputRedirect + 1)->c_str(), "r");
    if (not WasiIn.has_value())
      Panic(fmt::format("Cannot open input redirect file: {}",
                        *(HasInputRedirect + 1)));
    Inits.in = fileno(WasiIn.value());

    // remove < and the file name from the arguments
    AppArgs.erase(HasInputRedirect, HasInputRedirect + 2);
  } else
    Inits.in = 0;

  if (const auto HasOutputRedirect = std::ranges::find(AppArgs, ">");
      HasOutputRedirect != AppArgs.end()) {
    // change stdout to file after >
    WasiOut = fopen((HasOutputRedirect + 1)->c_str(), "w");
    if (not WasiOut.has_value())
      Panic(fmt::format("Cannot open output redirect file: {}",
                        *(HasOutputRedirect + 1)));
    Inits.out = fileno(WasiOut.value());

    // remove > and the file name from the arguments
    AppArgs.erase(HasOutputRedirect, HasOutputRedirect + 2);
  } else
    Inits.out = 1;

  if (const auto HasErrorRedirect = std::ranges::find(AppArgs, "2>");
      HasErrorRedirect != AppArgs.end()) {
    // change stderr to file after 2>
    WasiErr = fopen((HasErrorRedirect + 1)->c_str(), "w");
    if (not WasiErr.has_value())
      Panic(fmt::format("Cannot open error redirect file: {}",
                        *(HasErrorRedirect + 1)));
    Inits.err = fileno(WasiErr.value());

    // remove 2> and the file name from the arguments
    AppArgs.erase(HasErrorRedirect, HasErrorRedirect + 2);
  } else
    Inits.err = 2;

  Inits.fd_table_size = 3;
  Inits.argc = AppArgs.size();
  Inits.argv = (const char **)calloc(Inits.argc, sizeof(char *));
  for (auto i = 0; i < Inits.argc; ++i) {
    Inits.argv[i] = (const char *)calloc(AppArgs[i].size() + 1, sizeof(char));
    memcpy((void *)Inits.argv[i], AppArgs[i].c_str(), AppArgs[i].size());
  }
  Inits.envp = NULL;

  // always pre-open the current directory
  PreOpens.insert(PreOpens.begin(), ".");
  Inits.preopenc = PreOpens.size();
  Inits.preopens =
      (uvwasi_preopen_t *)calloc(Inits.preopenc, sizeof(uvwasi_preopen_t));
  for (auto i = 0; i < Inits.preopenc; ++i) {
    Inits.preopens[i].mapped_path = PreOpens[i].c_str();
    Inits.preopens[i].real_path = PreOpens[i].c_str();
  }
  Inits.allocator = NULL;
  Wasi = (uvwasi_t *)calloc(1, sizeof(uvwasi_t));
  auto Err = uvwasi_init(Wasi, &Inits);
  assert(Err == UVWASI_ESUCCESS);
  // free the argv and preopens
  for (auto i = 0; i < Inits.argc; ++i)
    free((void *)Inits.argv[i]);
  free(Inits.argv);
  free(Inits.preopens);

  //// Policies ////
  // assert all function types have at most one return value
  // FIXME: we can use this assumption to make parameter passing more
  // efficient
  for (auto &Type : M->types) {
    if (Type->kind() != wabt::TypeEntryKind::Func)
      continue;
    auto FTy = wabt::cast<wabt::FuncType>(Type);
    assert(FTy->sig.result_types.size() <= 1);
  }

  // initialize the linear memory
  auto Memories = M->memories;
  assert(Memories.size() <= 1 and "Maximum one memory is supported");
  if (not Memories.empty()) {
    auto InitialPages = Memories[0]->page_limits.initial;
    MaxPages = Memories[0]->page_limits.has_max
                   ? std::optional<u32>{Memories[0]->page_limits.max}
                   : std::nullopt;
    LinearMemorySize = InitialPages * WABT_DEFAULT_PAGE_SIZE;
    LinearMemory = (char *)mmap((void *)LinearMemoryBaseAddress, FourGigaBytes,
                                PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    for (auto DS : M->data_segments) {
      assert(DS->memory_var.index() == 0 and "Only one memory is supported");
      auto Offset =
          wabt::cast<wabt::ConstExpr>(&*DS->offset.begin())->const_.u32();
      memcpy(LinearMemory + Offset, DS->data.data(), DS->data.size());
    }
  }

  // initialize globals.
  Globals.resize(M->globals.size(), 0);
  for (auto i = 0; i < M->globals.size(); ++i) {
    auto G = M->globals[i];
    assert(G->type == wabt::Type::I32 and "Only i32 globals are supported");
    auto &Init = G->init_expr;
    assert(
        (Init.size() == 1 and Init.begin()->type() == wabt::ExprType::Const) and
        "G initializer must be a constant");
    Globals[i] =
        wabt::cast<wabt::ConstExpr>(&*G->init_expr.begin())->const_.u32();
  }

  // initialize the indirect_call function table
  assert(M->tables.size() <= 1 and "Multiple tables are not supported");
  if (not M->tables.empty()) {
    auto TableSize = M->tables[0]->elem_limits.initial;
    // the type of function table is int32*, and it should match with LLVM
    IndirectJumpTable.resize(TableSize, 0);

    assert(M->elem_segments.size() <= 1 and
           "Multiple elem segments are not supported");
    if (not M->elem_segments.empty()) {
      auto Elem = M->elem_segments[0];
      auto Offset =
          wabt::cast<wabt::ConstExpr>(&*Elem->offset.begin())->const_.u32();
      for (auto i = 0; i < Elem->elem_exprs.size(); ++i) {
        auto RefFunc =
            wabt::cast<wabt::RefFuncExpr>(&*Elem->elem_exprs[i].begin());
        auto FuncIdxInModule = RefFunc->var.index();
        auto FuncIdxInTable = Offset + i;
        IndirectJumpTable[FuncIdxInTable] = FuncIdxInModule;
      }
    }
  }

  // initialize the activation frame
  auto MaxActivationSize = 0;
  for (const auto &Type : M->types) {
    if (Type->kind() != wabt::TypeEntryKind::Func)
      continue;
    auto ParamsByteSize = 0;
    auto FTy = wabt::cast<wabt::FuncType>(Type);
    for (auto &ParamTy : FTy->sig.param_types)
      switch (ParamTy) {
      case wabt::Type::I32:
      case wabt::Type::F32:
        ParamsByteSize += 4;
        break;
      case wabt::Type::I64:
      case wabt::Type::F64:
        ParamsByteSize += 8;
        break;
      default:
        Panic("Unsupported param type");
      }
    MaxActivationSize = std::max(MaxActivationSize, ParamsByteSize);
  }
  MaxActivationSize = std::max(
      MaxActivationSize, 8); // in case we just want to return a single value
  ActivationFrame.resize(MaxActivationSize);

  // find the _start function index
  StartFIdx = -1;
  for (const auto &Export : M->exports)
    if (Export->kind == wabt::ExternalKind::Func and Export->name == "_start") {
      StartFIdx = int(Export->var.index());
      break;
    }
  if (StartFIdx == -1)
    Panic("No function _start found");

  // initialize the jump table (with stubs)
  auto FuncsSize = M->funcs.size();
  JumpTable.resize(FuncsSize, (void *)nullptr);
  FunctionPointers.resize(FuncsSize, (void *)nullptr);

  // bind wasi functions to the jump table entries
  for (auto i = 0; i < M->imports.size(); ++i) {
    auto Import = M->imports[i];
    if (Import->kind() != wabt::ExternalKind::Func)
      Panic("Only function imports are supported");

    if (Import->module_name != "wasi_snapshot_preview1" and
        Import->module_name != "wasi_unstable") {

      if (Import->module_name == "bench") {
        FunctionPointers[i] = (void *)vmcalls::noop;
        continue;
      }
      Panic("Only WASI imports are supported");
    }

    NumOfWasiFunctions++;
    auto WasiFunc = wabt::cast<wabt::FuncImport>(Import);

#define WASI_FUNCTION(NAME)                                                    \
  if (WasiFunc->field_name == #NAME) {                                         \
    JumpTable[i] = (void *)vmcalls::NAME;                                      \
    continue;                                                                  \
  }
#include "VirtualMachine/WASI.api.def"

#undef WASI_FUNCTION
    Panic(fmt::format("Unknown WASI function: {}", WasiFunc->field_name));
  }

  // Initialize the JIT runtime
  InitializeAllTargetInfos();
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmParsers();
  InitializeAllAsmPrinters();

  Linker = ExitOnErr(LLJITBuilder().create());

  for (auto I = NumOfWasiFunctions; I < JumpTable.size(); ++I)
    JumpTable[I] = (void *)vmcalls::onStartUp;

  for (auto I = NumOfWasiFunctions; I < FunctionPointers.size(); ++I) {
    FunctionPointers[I] = (void *)vmcalls::onJIT;
    // FunctionsExecMode[I] = ExecMode::JIT;
  }

  if (Interp) {
    if (Interp == Options::Interpretation::All)
      for (auto I = NumOfWasiFunctions; I < FunctionPointers.size(); ++I) {
        FunctionPointers[I] = (void *)vmcalls::onInterp;
        // FunctionsExecMode[I] = ExecMode::Interp;
      }
    else if (Interp == Options::Interpretation::Planned) {
      auto Plan = ReadPlan("interpret");
      fmt::println("[interpreter] {} functions", Plan.size());
      for (auto I : Plan) {
        FunctionPointers[I] = (void *)vmcalls::onInterp;
        // FunctionsExecMode[I] = ExecMode::Interp;
      }
    }
  }

  // Initialize the runtime state
  Runtime.resize(State::Size, nullptr);
  Runtime[State::VMPtr] = (void *)this;
  Runtime[State::Globals] = (void *)Globals.data();
  Runtime[State::LinearMemory] = (void *)LinearMemory;
  Runtime[State::LinearMemorySize] = (void *)&LinearMemorySize;
  Runtime[State::ActiveFrame] = (void *)ActivationFrame.data();
  Runtime[State::JumpTable] = (void *)JumpTable.data();
  Runtime[State::OnFn] = (void *)vmcalls::onFn;
  Runtime[State::IndirectTable] = (void *)IndirectJumpTable.data();
  Runtime[State::OnInstruction] = (void *)vmcalls::onInstruction;
  Runtime[State::OnStackOp] = (void *)vmcalls::onStackOp;
  Runtime[State::OnMemoryOp] = (void *)vmcalls::onMemoryOp;
  Runtime[State::OnCall] = (void *)vmcalls::onCall;
  Runtime[State::OnReturn] = (void *)vmcalls::onReturn;
  Runtime[State::OnOutOfBoundMemory] = (void *)vmcalls::onOutOfBoundMemory;
  Runtime[State::OnBranch] = (void *)vmcalls::onBranch;
  Runtime[State::OnString] = (void *)vmcalls::onString;
  Runtime[State::MemoryFill] = (void *)vmcalls::memoryFill;
  Runtime[State::MemoryCopy] = (void *)vmcalls::memoryCopy;
  Runtime[State::MemoryGrow] = (void *)vmcalls::memoryGrow;
  Runtime[State::MemorySize] = (void *)vmcalls::memorySize;

#ifdef PROFILE_VIRTUAL_MACHINE
  Prof->InitEnd = NOW();
#endif

  if (StaticCodeCache.has_value()) {
#ifdef PROFILE_VIRTUAL_MACHINE
    Prof->StaticStart = NOW();
#endif
    for (auto &&[Fn, Code] : *StaticCodeCache) {
      StaticSizes.push_back(Code.size());
      FunctionPointers[Fn] = (void *)link(Fn, std::move(Code));
      JumpTable[Fn] = (void *)vmcalls::onStartUp;
    }
#ifdef PROFILE_VIRTUAL_MACHINE
    Prof->StaticEnd = NOW();
#endif
  }

#ifdef DEBUG_INTERPRETER
  std::string Line;
  std::ifstream IFS(Application);
  while (std::getline(IFS, Line))
    MLines.push_back(Line);
  IFS.close();
#endif

#ifdef PRINT_WEBASSEMBLY_EXECUTION
  if (not Application.ends_with(".wat"))
    ErrAndExit("Only .wat files are supported");
  std::vector<std::string> Fs;
  for (auto &F : this->M->funcs)
    Fs.push_back(F->name);
  std::vector<std::string> Lines;
  std::ifstream IFS(Application);
  std::string L;
  while (std::getline(IFS, L))
    Lines.push_back(L);
  IFS.close();

  auto WasmPrinter = std::make_shared<WebAssemblyPrinter>(Lines, Fs);
  InstructionSubscribers.push_back(WasmPrinter);
  StackSubscribers.push_back(WasmPrinter);

#endif

#ifdef PROFILE_BRANCHES
  auto BrProf = std::make_shared<BranchProfiler>();
  BrSubscribers.push_back(BrProf);
#endif

#ifdef PROFILE_FOR_SPECIALIZATION
  auto SpecProf = std::make_shared<SpecializationProfiler>(M, ActivationFrame);
  CallSubscribers.push_back(SpecProf);
  ReturnSubscribers.push_back(SpecProf);
#endif

#ifdef PROFILE_CALLS
  auto CallProf = std::make_shared<CallProfiler>(M);
  CallSubscribers.push_back(CallProf);
  ReturnSubscribers.push_back(CallProf);
#endif
}

VirtualMachine::~VirtualMachine() {
  if (WasiIn.has_value())
    fclose(WasiIn.value());
  if (WasiOut.has_value())
    fclose(WasiOut.value());
  if (WasiErr.has_value())
    fclose(WasiErr.value());
  munmap(LinearMemory, LinearMemorySize);
  uvwasi_destroy(Wasi);
  // free all memory allocated by LLJIT
  for (auto &I : Runtime)
    I = nullptr; // clear the runtime state

  const auto End = Prof->End;
  const auto Start = Prof->Start;
  const auto Waiting = Prof->Waiting.load();

  const auto E2E = End - Start;
  const auto Exec = E2E - Waiting;

  auto CodeSize = 0;
  for (const auto &[Fn, Start, End, Size] : Prof->Records)
    CodeSize += Size;

  fmt::println("[vm] waiting={:.2f} e2e={:.2f} exec={:.2f} (seconds) | "
               "dynamic-code-size={:.1f} KB",
               Waiting / 1e9, E2E / 1e9, Exec / 1e9, CodeSize / 1024.0);

  metric("waiting", std::to_string(Waiting));
  metric("e2e", std::to_string(E2E));
  metric("exec", std::to_string(Exec));

  {
    nlohmann::json J;
    for (const auto &[Fn, StartupTime] : FunctionsStartup)
      J.push_back({Fn, StartupTime});
    metric("startups", J.dump());
  }

  metric("static-sizes", nlohmann::json(StaticSizes).dump());
}

void VirtualMachine::start() {
#ifdef PROFILE_VIRTUAL_MACHINE
  Prof->Start = NOW();
#endif

  CC->activation_frame(ActivationFrame.data());
  CC->connect();

#ifdef PUBLISH_CALLS
  onCall(StartFIdx);
#endif

  Mon = std::make_unique<Monitor>(M);

  // FIXME: add onFn(StartFIdx); ???
  VirtualCall(StartFIdx);

  // if (Interp)
  //   onInterp(StartFIdx);
  // else {
  //   if (JumpTable[StartFIdx] != (void *)vmcalls::onJIT)
  //     ((void (*)(void **))JumpTable[StartFIdx])(Runtime.data());
  //   else
  //     onJIT(StartFIdx);
  // }

#ifdef PUBLISH_RETURNS
  onReturn(StartFIdx);
#endif

  CC->disconnect();

#ifdef PROFILE_VIRTUAL_MACHINE
  Prof->End = NOW();
#endif
}

// this is for jitted, and cached functions
// TODO: clear up the boundaries between onfn, cache, jit, etc.
void VirtualMachine::onFn(const u32 Fn) {
  Mon->set(Fn);
  if (Cache and NotWasi(Fn))
    if (const auto Evicted = Cache->get(Fn); Evicted.has_value())
      JumpTable[*Evicted] = (void *)vmcalls::onJIT;
}

VirtualMachine::LinkedFunction VirtualMachine::link(const u32 Fn,
                                                    std::vector<char> &&Code) {
  auto Symbol = Linker->lookup(std::to_string(Fn));
  if (not Symbol) {
#ifdef TRACE_LINKER
    fmt::println("[vm][link] {}", Fn);
#endif
    SmallVector<char> SV{Code.begin(), Code.end()};
    auto Buff = std::make_unique<SmallVectorMemoryBuffer>(std::move(SV), false);
    ExitOnErr(Linker->addObjectFile(std::move(Buff)));
    Symbol = Linker->lookup(std::to_string(Fn));
  } else {
#ifdef TRACE_LINKER
    fmt::println("[vm][link] {} not needed", Fn);
#endif
  }
  return reinterpret_cast<LinkedFunction>(Symbol->toPtr<void *>());
}

void VirtualMachine::onJIT(const u32 Fn) {
  // fmt::println("[jit] {}", Fn);
  const auto Current = Mon->unset();
  // fmt::println("[jit] {}", Fn);
  // std::cout.flush();

#ifdef PROFILE_VIRTUAL_MACHINE
  const auto Start = high_resolution_clock::now();
#endif

  auto Code = CC->get(CompilerClient::Request{Fn});
  const auto CodeSize = Code.size();
  JumpTable[Fn] = (void *)link(Fn, std::move(Code));

#ifdef PROFILE_VIRTUAL_MACHINE
  const auto End = high_resolution_clock::now();
  Prof->onFn(Fn, Start, End, CodeSize);
#endif

  Mon->reset(Current);
  VirtualCall(Fn);
  // Callee(Runtime.data());
}

void VirtualMachine::onInterp(u32 Fn) { interpret(Fn); }

u32 VirtualMachine::memorySize() const {
  assert(LinearMemorySize % WABT_DEFAULT_PAGE_SIZE == 0);
  return LinearMemorySize >> 16;
}
void VirtualMachine::onStartUp(u32 Fn) {
  // fmt::println("[startup] {}", Fn);
  FunctionsStartup[Fn] = NOW();
  // fmt::println("[startup] fn={} startup={:.2f} ms", Fn,
  //              static_cast<double>(FunctionsStartup[Fn]-Prof->Start) / 1e6);
  JumpTable[Fn] = FunctionPointers[Fn];
  VirtualCall(Fn);
}
void VirtualMachine::memoryFill(u32 Dest, u32 Value, u32 Size) const {
  std::fill_n(LinearMemory + Dest, Size, Value);
}
void VirtualMachine::memoryCopy(u32 DstOffset, u32 SrcOffset, u32 Size) const {
  std::copy_n(LinearMemory + SrcOffset, Size, LinearMemory + DstOffset);
}
u32 VirtualMachine::memoryGrow(u32 Pages) {
  assert(LinearMemorySize % WABT_DEFAULT_PAGE_SIZE == 0);
  auto OldPages = LinearMemorySize / WABT_DEFAULT_PAGE_SIZE;
  auto NewPages = OldPages + Pages;
  if (MaxPages.has_value() and NewPages > MaxPages.value())
    return -1;

  LinearMemorySize = NewPages * WABT_DEFAULT_PAGE_SIZE;
  return OldPages;
}

void VirtualMachine::onInstruction(u32 Line) {
  for (auto &S : InstructionSubscribers)
    S->update({Line});
}

void VirtualMachine::onStackOp(u32 Op, u32 Ty, u64 Val) {
  for (const auto &S : StackSubscribers)
    S->update(
        {static_cast<StackOperation>(Op), static_cast<ValueType>(Ty), Val});
}

void VirtualMachine::onMemoryOp(u32 Op, u32 Ty, u64 Base, u64 Offset,
                                u64 Addr) {
  Panic(fmt::format("'{}' not implemented", __func__));
}

void VirtualMachine::onOutOfBoundMemory(u32 Line, u32 Ty, u64 Base, u64 Offset,
                                        u64 Addr) {
  Panic(fmt::format("'{}' not implemented", __func__));
}
void VirtualMachine::onBranch(const u32 Line, const u32 Cond) {
  for (auto &S : BrSubscribers)
    S->update({Line, Cond});
}

void VirtualMachine::onCall(const u32 FIdx) {
  for (const auto &S : CallSubscribers)
    S->update({FIdx});
}

void VirtualMachine::onReturn(const u32 FIdx) {
  for (const auto &S : ReturnSubscribers)
    S->update({FIdx});
}

void VirtualMachine::onString(const char *Str) {
  // print the string
  print(fg(fmt::color::green), "{}", Str);
}
