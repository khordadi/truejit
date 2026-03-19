#include "Compiler/LLVMCompiler/LLVMCompiler.h"
#include "Config.h"
#include "Globals.h"
#include "ThreadPool.h"
#include "VirtualMachineCompilerProtocol.h"
#include <Compiler/Compiler.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <memory>
#include <wabt/cast.h>

// #define LLVM_VERIFICATION
// #define LLVM_DUMP_IR
// #define PRINT_SPECIALIZATION_CANDIDATES
// #define TRACE_PARAMS
// #define TRACE_SPECIALIZATION
// #define IR_SIZE

// #define ENABLE_NAME
#ifdef ENABLE_NAME
#define WithName(Name) Name
#else
#define WithName(Name) ""
#endif

using namespace llvm;
using namespace wabt;

LLVMCompiler::LLVMCompiler(ModuleOptions Opts) : Compiler(std::move(Opts)) {
  InitializeAllTargetInfos();
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmParsers();
  InitializeAllAsmPrinters();
}

struct Host {
  Host(IRBuilder<> &B, Value *VM) : B(B), VM(VM) {}

private:
  IRBuilder<> &B;
  Value *VM = nullptr;

  LoadInst *LoadPointer(const State Pointer,
                        const std::string &Name = "") const {
    const auto Ptr = B.CreateGEP(B.getPtrTy(), VM, {B.getInt32(Pointer)});
    const auto Val = B.CreateLoad(B.getPtrTy(), Ptr);
    WithName(Val->setName(Name));
    return Val;
  }

  FunctionCallee LoadFunctionPointer(const State Pointer,
                                     const std::string &Type,
                                     const std::string &Name = "") const {
    auto GetFnType = [this](std::string FnStr) {
      auto GetTy = [this](const std::string &TyStr) -> llvm::Type * {
        // clang-format off
        if (TyStr == "void") return B.getVoidTy();
        if (TyStr == "ptr") return B.getPtrTy();
        if (TyStr == "u32") return B.getInt32Ty();
        if (TyStr == "u64") return B.getInt64Ty();
        if (TyStr == "bool") return B.getInt1Ty();
        if (TyStr == "f32") return B.getFloatTy();
        if (TyStr == "f64") return B.getDoubleTy();
        Panic(fmt::format("Unknown type: {}", TyStr));
        // clang-format on
      };
      // "void(ptr, u32, u32, u32)" --> "void"
      const auto Ret = FnStr.substr(0, FnStr.find('('));
      FnStr = FnStr.substr(FnStr.find('('));
      // "(ptr, u32, u32, u32)" --> "ptr, u32, u32, u32"
      auto Params = FnStr.substr(1, FnStr.size() - 2);
      // "ptr, u32, u32, u32" --> {"ptr", "u32", "u32", "u32"}
      std::vector<llvm::Type *> ParamsTypes;
      while (not Params.empty()) {
        const auto Comma = Params.find(',');
        ParamsTypes.push_back(GetTy(Params.substr(0, Comma)));
        if (Comma == std::string::npos)
          break;
        Params = Params.substr(Comma + 2);
      }
      return FunctionType::get(GetTy(Ret), ParamsTypes, false);
    };

    const auto Ptr = LoadPointer(Pointer, Name);
    const auto Ty = GetFnType(Type);
    return FunctionCallee(Ty, Ptr);
  }

  auto CreateVMCall(const FunctionCallee Fn, std::vector<Value *> Args) const {
    Args.insert(Args.begin(), VM);
    return B.CreateCall(Fn, Args);
  }

public:
  auto MemoryFill(Value *Dest, Value *Val, Value *Size) const {
    return CreateVMCall(MemoryFillFn, std::move(std::vector{Dest, Val, Size}));
  }
  auto MemoryCopy(Value *Dest, Value *Src, Value *Size) const {
    return CreateVMCall(MemoryCopyFn, std::move(std::vector{Dest, Src, Size}));
  }
  auto MemoryGrow(Value *Delta) const {
    return CreateVMCall(MemoryGrowFn, std::move(std::vector{Delta}));
  }
  auto MemorySize() const { return CreateVMCall(MemorySizeFn, {}); }

  auto CallFn(Value *Fn) const {
    return CreateVMCall(OnFn, std::move(std::vector{Fn}));
  }

  FunctionCallee MemoryFillFn = LoadFunctionPointer(
      State::MemoryFill, "void(ptr, u32, u32, u32)", "memory_fill");
  FunctionCallee MemoryCopyFn = LoadFunctionPointer(
      State::MemoryCopy, "void(ptr, u32, u32, u32)", "memory_copy");
  FunctionCallee MemoryGrowFn =
      LoadFunctionPointer(State::MemoryGrow, "u32(ptr, u32)", "memory_grow");
  FunctionCallee MemorySizeFn =
      LoadFunctionPointer(State::MemorySize, "u32(ptr)", "memory_size");

  FunctionCallee OnFn =
      LoadFunctionPointer(State::OnFn, "void(ptr, u32)", "on_fn");

#ifdef PUBLISH_STACK
  FunctionCallee TraceStackFn = LoadFunctionPointer(
      State::OnStackOp, "void(ptr, u32, u32, u64)", "on-stack-op");
  auto PublishStack(const u32 Op, Value *Val) {
    auto Serialize = [this](Value *V) {
      const auto Ty = V->getType();
      // clang-format off
          if (Ty == B.getInt32Ty()) return std::make_pair(B.CreateZExt(V, B.getInt64Ty()), U32);
          if (Ty == B.getInt64Ty()) return std::make_pair(V, U64);
          if (Ty == B.getFloatTy()) return std::make_pair(B.CreateFPToUI(V, B.getInt64Ty()), F32);
          if (Ty == B.getDoubleTy()) return std::make_pair(B.CreateFPToUI(V, B.getInt64Ty()), F64);
          ErrAndExit(fmt::format("Unknown StackOp type"));
      // clang-format on
    };
    auto [Val64, Ty] = Serialize(Val);
    return CreateVMCall(TraceStackFn, {B.getInt32(Op), B.getInt32(Ty), Val64});
  };
#endif

#ifdef PUBLISH_INSTRUCTION

  FunctionCallee TraceInstructionFn = LoadFunctionPointer(
      State::OnInstruction, "void(ptr, u32)", "on-instruction");
  auto PublishInstruction(const Expr &E) {
    return CreateVMCall(TraceInstructionFn, {B.getInt32(E.loc.line)});
  };

#endif

#ifdef PUBLISH_BRANCHES
  FunctionCallee ProfileBranchFn =
      LoadFunctionPointer(State::OnBranch, "void(ptr, u32, bool)", "on-branch");
  auto PublishBranch(const int Line, Value *Condition) {
    return CreateVMCall(ProfileBranchFn, {B.getInt32(Line), Condition});
  }
#endif

#ifdef PUBLISH_CALLS
  FunctionCallee ProfileCallFn =
      LoadFunctionPointer(State::OnCall, "void(ptr, u32)", "on-call");
  auto PublishCall(Value *FnIdx) {
    return CreateVMCall(ProfileCallFn, {FnIdx});
  };
#endif
#ifdef PUBLISH_RETURNS
  FunctionCallee ProfileReturnFn =
      LoadFunctionPointer(State::OnReturn, "void(ptr, u32)", "on-return");
  auto PublishReturn(Value *FnIdx) {
    return CreateVMCall(ProfileReturnFn, {FnIdx});
  };
#endif

  LoadInst *Globals = LoadPointer(State::Globals, "globals");
  LoadInst *Mem = LoadPointer(LinearMemory, "linear_memory");
  LoadInst *JumpTable = LoadPointer(State::JumpTable, "jump_table");
  LoadInst *IndirectTable = LoadPointer(State::IndirectTable, "indirect_table");
  LoadInst *Frame = LoadPointer(ActiveFrame, "activation_frame");
};

class Stack {
  std::vector<AllocaInst *> ValueStack;
  IRBuilder<> &B;
  Host &H;

public:
  explicit Stack(IRBuilder<> &B, Host &H) : B(B), H(H) {}

  auto Push(Value *Val, const std::string &Name = "", const bool Trace = true) {
    const auto Ptr = B.CreateAlloca(Val->getType());
    WithName(Ptr->setName(Name + "_ptr"));
    B.CreateStore(Val, Ptr);
    ValueStack.push_back(Ptr);
#ifdef PUBLISH_STACK
    if (Trace)
      H.PublishStack(StackPush, Val);
#endif
  };

  auto Pop(const std::string &Name = "", const bool Trace = true) {
    const auto Ptr = ValueStack.back();
    ValueStack.pop_back();
    auto Val = B.CreateLoad(Ptr->getAllocatedType(), Ptr);
    WithName(Val->setName(Name));
#ifdef PUBLISH_STACK
    if (Trace)
      H.PublishStack(StackPop, Val);
#endif
    return Val;
  };

  auto Pick(const u32 Idx, const std::string &Name = "") {
    const auto Ptr = ValueStack[Idx];
    auto Ty = Ptr->getAllocatedType();
    auto Val = B.CreateLoad(Ty, Ptr);
    WithName(Val->setName(Name));
    return std::make_pair(Val, Ty);
  };

  auto Top(const std::string &Name = "") {
    const auto Ptr = ValueStack.back();
    auto Ty = Ptr->getAllocatedType();
    auto Val = B.CreateLoad(Ty, Ptr);
    WithName(Val->setName(Name));
    return std::make_pair(Val, Ty);
  };

  auto Jot(const u32 Idx, Value *Val, const std::string &Name = "") {
    const auto Ptr = ValueStack[Idx];
    B.CreateStore(Val, Ptr);
    WithName(Val->setName(Name));
  };
};

void transpile(std::shared_ptr<wabt::Module> WasmM,
               std::shared_ptr<LLVMContext> C, std::shared_ptr<llvm::Module> M,
               u32 FnIdx, std::optional<Specialization> Spec) {
  auto WasmF = WasmM->funcs[FnIdx];

  if (WasmF->name.starts_with("$__imported_wasi_snapshot_preview1"))
    Panic(fmt::format("{} is a WASI function", WasmF->name));

  const auto Specialized =
      Spec.has_value() and
      std::holds_alternative<SpecializationChild>(Spec.value());
  auto ChildParams = [&] -> const auto & {
    assert(Specialized and "This isn't a child");
    return std::get<SpecializationChild>(Spec.value()).Params;
  };
  const auto ConditionalSpec =
      Spec.has_value() and std::holds_alternative<Conditional>(Spec.value());
  auto ParentParamsList = [&] -> const auto & {
    assert(ConditionalSpec and "This isn't a parent");
    return std::get<Conditional>(Spec.value()).ParamsList;
  };
  if (ConditionalSpec)
    for (const auto &Params : ParentParamsList())
      transpile(WasmM, C, M, FnIdx, SpecializationChild{Params});

  const auto UnconditionalSpec =
      Spec.has_value() and
      std::holds_alternative<UnconditionalDynamic>(Spec.value());
  auto UnconditionalParams = [&] -> const auto {
    assert(UnconditionalSpec and "This isn't an unconditional");
    return deserialize(
        WasmF->decl.sig.param_types,
        std::get<UnconditionalDynamic>(Spec.value()).ActivationFrame);
  };

  auto B = IRBuilder(*C);

  auto GetSpecializedConstant =
      [&](const WasmValue &Const) -> llvm::Constant * {
    struct Visitor {
      IRBuilder<> &B;
      llvm::Constant *operator()(const u32 V) const { return B.getInt32(V); }
      llvm::Constant *operator()(const u64 V) const { return B.getInt64(V); }
      llvm::Constant *operator()(const f32 V) const {
        return ConstantFP::get(B.getFloatTy(), V);
      }
      llvm::Constant *operator()(const f64 V) const {
        return ConstantFP::get(B.getDoubleTy(), V);
      }
    };
    return std::visit(Visitor{B}, Const);
  };

  auto GetSpecializedFunctionName =
      [&](const std::vector<WasmValue> &Candidate) {
        auto Name = fmt::format("{}_specialized___", FnIdx);
        for (auto I = 0; I < Candidate.size(); ++I) {
          auto Val = std::visit(to_value{}, Candidate[I]);
          Name += fmt::format("{}", Val);
          if (I != Candidate.size() - 1)
            Name += "__";
        }
        return Name;
      };

  auto GetType = [&](const wabt::Type &Type) -> llvm::Type * {
    switch (Type) {
    case wabt::Type::I32:
      return B.getInt32Ty();
    case wabt::Type::I64:
      return B.getInt64Ty();
    case wabt::Type::F32:
      return B.getFloatTy();
    case wabt::Type::F64:
      return B.getDoubleTy();
    default:
      Panic(fmt::format("Unsupported type: {}", Type.GetName()));
    }
  };

  auto GetFn = [&](const FuncSignature &Sig) {
    auto GetReturnType = [&](const TypeVector &Types) {
      assert(Types.size() <= 1 and "Only one return type is supported");
      return not Specialized ? B.getVoidTy()
             : Types.empty() ? B.getVoidTy()
                             : GetType(Types.front());
    };

    auto GetParamsTy = [&] {
      // external: void(ptr vm, u32 fn_idx)
      // internal: void(ptr vm)
      return not Specialized
                 ? std::vector<llvm::Type *>{B.getPtrTy(), B.getInt32Ty()}
                 : std::vector<llvm::Type *>{B.getPtrTy()};
    };

    const auto FnTy = FunctionType::get(GetReturnType(Sig.result_types),
                                        GetParamsTy(), false);

    auto GetName = [&] {
      return not Specialized ? fmt::format("{}", FnIdx)
                             : GetSpecializedFunctionName(ChildParams());
    };

    auto GetLinkage = [&] {
      return not Specialized
                 ? Function::ExternalLinkage
                 : Function::PrivateLinkage; // TODO: InternalLinkage
    };
    const auto Fn = Function::Create(FnTy, GetLinkage(), GetName(), *M);
    Fn->getArg(0)->setName("vm");
    if (not Specialized)
      Fn->getArg(1)->setName("fn_idx");
    return Fn;
  };
  auto F = GetFn(WasmF->decl.sig);

  const auto EntryBB = BasicBlock::Create(*C, "entry", F);
  B.SetInsertPoint(EntryBB);

  auto GetTime = [&]() {
    // clang-format off
      const auto Ty = FunctionType::get(B.getInt64Ty(), {B.getPtrTy()}, false);
      const auto Fn = M->getOrInsertFunction("time", Ty);
      return B.CreateCall(Fn, {ConstantPointerNull::get(B.getPtrTy())});
    // clang-format on
  };

  auto Print = [&](const std::string &FmtStr, const std::vector<Value *> &Vs) {
    // clang-format off
      const auto Ty = FunctionType::get(B.getInt32Ty(), {B.getPtrTy()}, true); // int printf(const char *restrict format, ...);
      const auto Fn = M->getOrInsertFunction("printf", Ty);
      std::vector<Value*> Args = {B.CreateGlobalStringPtr(FmtStr)};
      Args.insert(Args.end(), Vs.begin(), Vs.end());
      B.CreateCall(Fn, Args);
    // clang-format on
  };
  auto PrintTime = [&](std::string Msg = "") {
    // clang-format off
      const auto Now = GetTime();
      const auto FmtStr = fmt::format("\n[timestamp=%lu] {}\n", Msg);
      Print(FmtStr, {Now});
    // clang-format on
  };

  const auto PrologueBB = BasicBlock::Create(*C, "prologue", F);
  B.CreateBr(PrologueBB);
  B.SetInsertPoint(PrologueBB);

  Host Host(B, F->getArg(0));

  bool HookCondition;
  auto InitHook = [&] {
    if (not HookCondition)
      return;
  };
  auto FinalizeHook = [&] {
    if (not HookCondition)
      return;
  };

  InitHook();

  Stack Stack{B, Host};

  auto FromStack = [&](const TypeVector &Types) {
    std::vector<Value *> Vs;
    Vs.reserve(Types.size());
    for (int i = 0; i < Types.size(); ++i)
      Vs.push_back(Stack.Pop("", false));
    std::ranges::reverse(Vs);
    return Vs;
  };
  auto ToStack = [&](const std::vector<Value *> &Vs) {
    for (const auto V : Vs)
      Stack.Push(V, WithName(""), false);
  };

  auto FromFrame = [&](const TypeVector &Types) {
    std::vector<Value *> Args;
    Args.reserve(Types.size());
    u32 Addr = 0;
    for (const auto Type : Types) {
      const auto Ty = GetType(Type);
      const auto Ptr =
          B.CreateGEP(B.getInt8Ty(), Host.Frame, {B.getInt32(Addr)});
      Args.push_back(B.CreateLoad(Ty, Ptr));
      Addr += Ty->getPrimitiveSizeInBits() / 8;
    }
    return Args;
  };
  auto ToFrame = [&](const std::vector<Value *> &Vs) {
    auto Addr = 0u;
    for (const auto V : Vs) {
      const auto Ptr =
          B.CreateGEP(B.getInt8Ty(), Host.Frame, {B.getInt32(Addr)});
      B.CreateStore(V, Ptr);
      Addr += V->getType()->getPrimitiveSizeInBits() / 8;
    }
  };

  // get the function arguments from the frame
  auto GetParams = [&](const TypeVector &ParamsTypes) {
    if (UnconditionalSpec) {
      auto CPs = UnconditionalParams();
      assert(CPs.size() == ParamsTypes.size() and
             "Unconditional specialization params size mismatch");

#ifdef PRINT_SPECIALIZATION_CANDIDATES
      print(fg(fmt::color::purple), "[unconditional-specialization] ");
      print(fg(fmt::color::pink), "{}\n", WasmF->name);

      for (auto &CP : CPs)
        print(fg(fmt::color::sky_blue), "{}:{} ",
              std::visit(to_type{}, CP).GetName(), std::visit(to_value{}, CP));
      fmt::print("\n");

      std::cout.flush();
#endif

      const auto ParamsBB =
          BasicBlock::Create(*C, "unconditional_specialized_params", F);
      B.CreateBr(ParamsBB);
      B.SetInsertPoint(ParamsBB);

      std::vector<Value *> Ps;
      Ps.reserve(CPs.size());
      for (const auto &CP : CPs)
        Ps.push_back(GetSpecializedConstant(CP));
      return Ps;
    }
    if (Specialized) {
      auto CPs = ChildParams();
      assert(CPs.size() == ParamsTypes.size() and
             "Specialization params size mismatch");

      const auto ParamsBB =
          BasicBlock::Create(*C, "conditional_specialized_params", F);
      B.CreateBr(ParamsBB);
      B.SetInsertPoint(ParamsBB);

      std::vector<Value *> Ps;
      Ps.reserve(CPs.size());
      for (const auto &CP : CPs)
        Ps.push_back(GetSpecializedConstant(CP));
      return Ps;
    }
    const auto ParamsBB = BasicBlock::Create(*C, "frame_to_params", F);
    B.CreateBr(ParamsBB);
    B.SetInsertPoint(ParamsBB);
    return FromFrame(ParamsTypes);
  };
  auto Params = GetParams(WasmF->decl.sig.param_types);

#ifdef TRACE_PARAMS
  if (auto _ = std::getenv("TRACE_PARAMS")) {
    // open file Path in LLVM IR
    auto PrintParams = [Print](auto FunctionName, auto &Ps) {
      auto FmtString = fmt::format("\n[trace-params] {}(", FunctionName);
      for (int I = 0; I < Ps.size(); ++I) {
        if (auto Ty = Ps[I]->getType(); Ty->isIntegerTy(32))
          FmtString += fmt::format("i32:%u", I);
        else if (Ty->isIntegerTy(64))
          FmtString += fmt::format("i64:%lu", I);
        else if (Ty->isFloatTy())
          FmtString += fmt::format("f32:%u", I);
        else if (Ty->isDoubleTy())
          FmtString += fmt::format("f64:%lu", I);

        if (I != Ps.size() - 1)
          FmtString += ", ";
      }
      FmtString += ")\n";

      std::vector<Value *> Vs;
      Vs.reserve(Ps.size());
      for (auto Arg : Ps)
        Vs.push_back(Arg);

      Print(FmtString, Vs);
    };
    PrintParams(WasmF->name, Params);
  }
#endif

  auto DoReturn = [&](const TypeVector &ReturnTypes) {
    FinalizeHook();
    const auto Returns = FromStack(ReturnTypes);
    if (not Specialized) {
      ToFrame(Returns);
      B.CreateRetVoid();
    } else {
      if (Returns.empty())
        B.CreateRetVoid();
      else {
        assert(Returns.size() == 1 and "Only one return value is supported");
        B.CreateRet(Returns.front());
      }
    }
  };

  // dispatch in parent to the child
  if (ConditionalSpec) {
    if (auto ParamsList = ParentParamsList(); not ParamsList.empty()) {
#ifdef PRINT_SPECIALIZATION_CANDIDATES
      print(fg(fmt::color::purple), "[conditional-specialization] ");
      print(fg(fmt::color::pink), "{} ", WasmF->name);
      print(fg(fmt::color::light_green), "[{} candidates]\n",
            ParamsList.size());

      for (auto &Ps : ParamsList) {
        for (auto &P : Ps)
          print(fg(fmt::color::sky_blue), "{}:{} ",
                std::visit(to_type{}, P).GetName(), std::visit(to_value{}, P));
        fmt::print("\n");
      }
      std::cout.flush();
#endif

      auto SpecInBB = BasicBlock::Create(*C, "specs_start", F);
      B.CreateBr(SpecInBB);
      B.SetInsertPoint(SpecInBB);

      for (auto I = 0; I < ParamsList.size(); ++I) {
        auto SpecBB = BasicBlock::Create(*C, fmt::format("spec_cond_{}", I), F);
        B.CreateBr(SpecBB);
        B.SetInsertPoint(SpecBB);

        auto &Candidate = ParamsList[I];

        Value *Cond = ConstantInt::getTrue(*C);
        for (auto i = 0; i < Candidate.size(); ++i) {
          auto Param = Params[i];
          auto SParam = GetSpecializedConstant(Candidate[i]);

          Value *Eq;
          if (Param->getType()->isIntegerTy())
            Eq = B.CreateICmpEQ(Param, SParam);
          else if (Param->getType()->isFloatTy() or
                   Param->getType()->isDoubleTy())
            Eq = B.CreateFCmpOEQ(Param, SParam);
          else {
            Param->getType()->dump();
            Panic(fmt::format("Unsupported type"));
          }
          Cond = B.CreateAnd(Cond, Eq);
        }

        auto ThenBB = BasicBlock::Create(*C, fmt::format("spec_do_{}", I), F);
        auto ElseBB = BasicBlock::Create(*C, fmt::format("spec_dont_{}", I), F);
        B.CreateCondBr(Cond, ThenBB, ElseBB);

        auto DirectCall =
            [&](const std::string &CalleeName) -> std::optional<Value *> {
          const auto Fn = M->getFunction(CalleeName);

#ifdef TRACE_SPECIALIZATION
          Print(fmt::format(">>> [specialized_{}]\n", I), {});
#endif

          auto Ret = B.CreateCall(Fn, {F->getArg(0)});

#ifdef TRACE_SPECIALIZATION
          Print(fmt::format("<<< [specialized_{}]\n", I), {});
#endif

          if (Fn->getReturnType()->isVoidTy())
            return std::nullopt;
          return Ret;
        };

        B.SetInsertPoint(ThenBB);
        auto CalleeName = GetSpecializedFunctionName(Candidate);
        if (auto Ret = DirectCall(CalleeName); Ret.has_value())
          Stack.Push(Ret.value(), "ret", false);
        DoReturn(WasmF->decl.sig.result_types);

        B.SetInsertPoint(ElseBB);
      }

      auto SpecOutBB = BasicBlock::Create(*C, "specs_end", F);
      B.CreateBr(SpecOutBB);
      B.SetInsertPoint(SpecOutBB);

      // get current llvm function
      // F->dump();
    }
  }

  // push the arguments to the stack
  auto ParamsToStack = [&](const std::vector<Value *> &Ps) {
    const auto ToStackBB = BasicBlock::Create(*C, "params_to_stack", F);
    B.CreateBr(ToStackBB);
    B.SetInsertPoint(ToStackBB);
    ToStack(Ps);
  };
  ParamsToStack(Params);

  // locals
  auto LocalsToStack = [&](const LocalTypes &Locals) {
    const auto LocalsBB = BasicBlock::Create(*C, "locals", F);
    B.CreateBr(LocalsBB);
    B.SetInsertPoint(LocalsBB);

    std::vector<Value *> Vs;
    for (auto Local : Locals)
      Vs.push_back(llvm::Constant::getNullValue(GetType(Local)));

    ToStack(Vs);
  };
  LocalsToStack(WasmF->local_types);

  std::vector<BasicBlock *> BlockStack;
  auto DoBody = [&] {
    // a lambda for transpiling each instruction
    std::function<void(const ExprList &, BasicBlock *, BasicBlock *)>
        TranspileExprList;
    std::function TranspileExpr = [&](const Expr &E) -> void {
      auto Line = E.loc.line;
      auto ExprType = E.type();
      auto Label = fmt::format("line_{}_{}", Line, GetExprTypeName(ExprType));
      // std::cout << Label << std::endl;

      // pretty dump
      // FIXME: remove this later
      auto ExprBB = BasicBlock::Create(*C, Label, F);
      B.CreateBr(ExprBB);
      B.SetInsertPoint(ExprBB);

#ifdef PUBLISH_INSTRUCTION
      Host.PublishInstruction(E);
#endif
      auto JumpTableCall = [&](Value *FnIdx, const FuncSignature &Sig) {
        const auto Params = FromStack(Sig.param_types);
        ToFrame(Params);
        const auto FnPtr = B.CreateGEP(B.getPtrTy(), Host.JumpTable, {FnIdx});
        WithName(FnPtr->setName("fn_ptr"));
        const auto Fn = B.CreateLoad(B.getPtrTy(), FnPtr);
        WithName(Fn->setName("fn"));
        const auto FnTy = FunctionType::get(
            B.getVoidTy(), {B.getPtrTy(), B.getInt32Ty()}, false);
        const auto Callee = FunctionCallee(FnTy, Fn);
#ifdef PUBLISH_CALLS
        Host.PublishCall(FnIdx);
#endif

#if defined(PROFILE_EXECUTION_TIME)
        Host.CallFn(FnIdx);
#endif

        B.CreateCall(Callee, {F->getArg(0), FnIdx}); // indirect call
#ifdef PUBLISH_RETURNS
        Host.PublishReturn(FnIdx);
#endif
        const auto Return = FromFrame(Sig.result_types);
        ToStack(Return);
      };

      switch (ExprType) {
      case ExprType::Const: {
        auto CE = wabt::cast<ConstExpr>(&E);
        // clang-format off
        auto GetConst = [&B](const Const &Const) -> Constant * {
          switch (Const.type()) {
          case wabt::Type::I32: return B.getInt32(static_cast<u32>(Const.u32()));
          case wabt::Type::I64: return B.getInt64(static_cast<u64>(Const.u64()));
          case wabt::Type::F32: return ConstantFP::get(B.getFloatTy(), wabt::Bitcast<f32>(Const.f32_bits()));
          case wabt::Type::F64: return ConstantFP::get(B.getDoubleTy(), wabt::Bitcast<f64>(Const.f64_bits()));
          default: Panic(fmt::format("Unsupported type: {}\n", Const.type().GetName()));
          }
        };
        // clang-format on
        auto Const = GetConst(CE->const_);
        Stack.Push(Const, WithName(Label));
        break;
      }

      case ExprType::LocalGet: {
        auto LG = wabt::cast<LocalGetExpr>(&E);
        auto SrcIdx = LG->var.index();
        auto [Src, _] = Stack.Pick(SrcIdx, WithName(Label + "_src"));
        Stack.Push(Src, WithName(Label + "_dest"));
        break;
      }
      case ExprType::LocalSet: {
        auto LS = wabt::cast<LocalSetExpr>(&E);
        auto Src = Stack.Pop(WithName(Label + "_src"));
        auto DestIdx = LS->var.index();
        Stack.Jot(DestIdx, Src, WithName(Label + "_dest"));
        break;
      }
      case ExprType::LocalTee: {
        auto LT = wabt::cast<LocalTeeExpr>(&E);
        auto [Src, _] = Stack.Top(WithName(Label + "_src"));
        auto DestIdx = LT->var.index();
        Stack.Jot(DestIdx, Src, WithName(Label + "_dest"));
        break;
      }

      case ExprType::GlobalGet: { // FIXME: assumption: all globals are
                                  // i32
        auto GG = wabt::cast<GlobalGetExpr>(&E);
        auto GIdx = GG->var.index();
        auto GPtr =
            B.CreateGEP(B.getInt32Ty(), Host.Globals, {B.getInt32(GIdx)});
        WithName(GPtr->setName(Label + "_global_ptr"));
        auto G = B.CreateLoad(B.getInt32Ty(), GPtr);
        WithName(G->setName(Label + "_global"));
        Stack.Push(G, WithName(Label + "_dest"));
        break;
      }
      case ExprType::GlobalSet: { // FIXME: assumption: all globals are
                                  // i32
        auto GS = wabt::cast<GlobalSetExpr>(&E);
        auto GIdx = GS->var.index();
        auto G = Stack.Pop(WithName(Label + "_global"));
        auto GPtr =
            B.CreateGEP(B.getInt32Ty(), Host.Globals, {B.getInt32(GIdx)});
        WithName(GPtr->setName(Label + "_global_ptr"));
        B.CreateStore(G, GPtr);
        break;
      }

      case ExprType::Store: {
        auto SE = wabt::cast<StoreExpr>(&E);
        // clang-format off
          auto DoTrunc = [&B](const Opcode Opcode, Value *Src) -> Value * {
            switch (Opcode) {
            case Opcode::I32Store8:
            case Opcode::I64Store8: return B.CreateTrunc(Src, B.getInt8Ty());
            case Opcode::I32Store16:
            case Opcode::I64Store16: return B.CreateTrunc(Src, B.getInt16Ty());
            case Opcode::I64Store32: return B.CreateTrunc(Src, B.getInt32Ty());
            case Opcode::I32Store:
            case Opcode::I64Store:
            case Opcode::F32Store:
            case Opcode::F64Store: return Src;
            default: Panic(fmt::format("Unsupported store opcode '{}'\n", Opcode.GetName()));
            }
          }; // clang-format on

        auto Src = Stack.Pop(WithName(Label + "_src"));
        auto TruncSrc = DoTrunc(SE->opcode, Src);
        WithName(TruncSrc->setName(Label + "_trunc_src"));

        auto Addr = Stack.Pop(WithName(Label + "_addr"));
        assert(Addr->getType()->isIntegerTy() and "Address is not integer");

        auto Offset = B.getInt32(SE->offset);

        auto AddrWithOffset = B.CreateAdd(Addr, Offset);
        WithName(AddrWithOffset->setName(Label + "_addr_with_offset"));

        auto DestPtr = B.CreateGEP(B.getInt8Ty(), Host.Mem, AddrWithOffset);
        WithName(DestPtr->setName(Label + "_dest_ptr"));

        B.CreateStore(TruncSrc, DestPtr);
        break;
      }
      case ExprType::Load: {
        const auto LE = wabt::cast<LoadExpr>(&E);

        auto DoRead = [&B](const Opcode Opcode, Value *SrcPtr) {
          switch (Opcode) {
            // clang-format off
              case Opcode::I32Load8U:
              case Opcode::I32Load8S:
              case Opcode::I64Load8U:
              case Opcode::I64Load8S: return B.CreateLoad(B.getInt8Ty(), SrcPtr);
              case Opcode::I32Load16U:
              case Opcode::I32Load16S:
              case Opcode::I64Load16U:
              case Opcode::I64Load16S: return B.CreateLoad(B.getInt16Ty(), SrcPtr);
              case Opcode::I32Load:
              case Opcode::I64Load32U:
              case Opcode::I64Load32S: return B.CreateLoad(B.getInt32Ty(), SrcPtr);
              case Opcode::I64Load: return B.CreateLoad(B.getInt64Ty(), SrcPtr);
              case Opcode::F32Load: return B.CreateLoad(B.getFloatTy(), SrcPtr);
              case Opcode::F64Load: return B.CreateLoad(B.getDoubleTy(), SrcPtr);
              default: Panic(fmt::format("Unsupported load opcode '{}'", Opcode.GetName()));
            // clang-format on
          }
        };
        auto GetDestType = [&B](const Opcode Opcode) -> llvm::Type * {
          switch (Opcode) {
            // clang-format off
              case Opcode::I32Load8S:
              case Opcode::I32Load8U:
              case Opcode::I32Load16S:
              case Opcode::I32Load16U:
              case Opcode::I32Load: return B.getInt32Ty();
              case Opcode::I64Load8S:
              case Opcode::I64Load8U:
              case Opcode::I64Load16S:
              case Opcode::I64Load16U:
              case Opcode::I64Load32S:
              case Opcode::I64Load32U:
              case Opcode::I64Load: return B.getInt64Ty();
              case Opcode::F32Load: return B.getFloatTy();
              case Opcode::F64Load: return B.getDoubleTy();
              default: Panic(fmt::format("Unsupported load opcode '{}'\n", Opcode.GetName()));
            // clang-format on
          }
        };
        auto DoExtend = [&B](const Opcode Opcode, Value *Src,
                             llvm::Type *DestTy) -> Value * {
          switch (Opcode) {
            // clang-format off
              case Opcode::I32Load8S:
              case Opcode::I32Load16S:
              case Opcode::I64Load8S:
              case Opcode::I64Load16S:
              case Opcode::I64Load32S: return B.CreateSExt(Src, DestTy);
              case Opcode::I32Load8U:
              case Opcode::I32Load16U:
              case Opcode::I64Load8U:
              case Opcode::I64Load16U:
              case Opcode::I64Load32U: return B.CreateZExt(Src, DestTy);
              case Opcode::I32Load:
              case Opcode::I64Load:
              case Opcode::F32Load:
              case Opcode::F64Load: return Src;
              default: Panic(fmt::format("Unsupported load opcode '{}'\n", Opcode.GetName()));
            // clang-format on
          }
        };

        auto Addr = Stack.Pop(WithName(Label + "_addr"));
        assert(Addr->getType()->isIntegerTy() && "Address is not integer");

        auto Offset = B.getInt32(LE->offset);

        auto AddrWithOffset = B.CreateAdd(Addr, Offset);
        WithName(AddrWithOffset->setName(Label + "_addr_with_offset"));

        auto SrcPtr = B.CreateGEP(B.getInt8Ty(), Host.Mem, AddrWithOffset);
        WithName(SrcPtr->setName(Label + "_src_ptr"));

        const auto Src = DoRead(LE->opcode, SrcPtr);
        WithName(Src->setName(Label + "_src"));

        auto DestTy = GetDestType(LE->opcode);

        const auto Dest = DoExtend(LE->opcode, Src, DestTy);

        Stack.Push(Dest, WithName(Label + "_dest"));
        break;
      }

      case ExprType::MemoryFill: {
        auto Size = Stack.Pop(WithName(Label + "_size"));
        auto Value = Stack.Pop(WithName(Label + "_value"));
        auto Dest = Stack.Pop(WithName(Label + "_dest"));
        Host.MemoryFill(Dest, Value, Size);
        break;
      }
      case ExprType::MemoryCopy: {
        auto Size = Stack.Pop(WithName(Label + "_size"));
        auto Src = Stack.Pop(WithName(Label + "_src"));
        auto Dest = Stack.Pop(WithName(Label + "_dest"));
        Host.MemoryCopy(Dest, Src, Size);
        break;
      }
      case ExprType::MemoryGrow: {
        auto Delta = Stack.Pop(WithName(Label + "_delta"));
        auto Result = Host.MemoryGrow(Delta);
        Stack.Push(Result, WithName(Label + "_result"));
        break;
      }
      case ExprType::MemorySize: {
        auto Result = Host.MemorySize();
        Stack.Push(Result, WithName(Label + "_result"));
        break;
      }

      case ExprType::Block: {
        // clang-format off
          auto BE = wabt::cast<BlockExpr>(&E);
          auto InBB = BasicBlock::Create(*C, fmt::format("line_{}_block", BE->loc.line), F);
          auto OutBB = BasicBlock::Create(*C, fmt::format("line_{}_block_end", BE->block.end_loc.line), F);
          BlockStack.emplace_back(OutBB);
          TranspileExprList(BE->block.exprs, InBB, OutBB);
          break;
        // clang-format on
      }
      case ExprType::Loop: {
        // clang-format off
          auto LE = wabt::cast<LoopExpr>(&E);
          auto InBB = BasicBlock::Create(*C, fmt::format("line_{}_loop", LE->loc.line), F);
          auto OutBB = BasicBlock::Create(*C, fmt::format("line_{}_loop_end", LE->block.end_loc.line), F);
          BlockStack.emplace_back(InBB);
          TranspileExprList(LE->block.exprs, InBB, OutBB);
          break;
        // clang-format on
      }

      case ExprType::If: {
        Panic("IfExpr is not supported");
      }

      case ExprType::Br: {
        auto BrExpr = wabt::cast<wabt::BrExpr>(&E);
        auto Depth = BrExpr->var.index();
        auto Target = BlockStack.rbegin()[Depth];
        B.CreateBr(Target);
        break;
      }
      case ExprType::BrIf: {
        auto BrIfExpr = wabt::cast<wabt::BrIfExpr>(&E);
        auto Depth = BrIfExpr->var.index();
        auto Target = BlockStack.rbegin()[Depth];
        auto Operand = Stack.Pop(WithName(Label + "_operand"));
        auto Condition = B.CreateICmpNE(Operand, B.getInt32(0));
        WithName(Condition->setName(Label + "_condition"));
#ifdef PUBLISH_BRANCHES
        Host.PublishBranch(Line, Condition);
#endif
        auto ThenBB = BasicBlock::Create(*C, Label + "_then", F);
        auto ElseBB = BasicBlock::Create(*C, Label + "_else", F);
        B.CreateCondBr(Condition, ThenBB, ElseBB);
        B.SetInsertPoint(ThenBB);
        B.CreateBr(Target);
        B.SetInsertPoint(ElseBB);
        break;
      }
      case ExprType::BrTable: { // FIXME: replace with switch
        auto BT = wabt::cast<BrTableExpr>(&E);

        auto Operand = Stack.Pop(WithName(Label + "_operand"));

        // clang-format off
          //
          // create BrTable using switch
          // Operand == 0 ==> br BrTableExpr->targets[0].index()
          // Operand == 1 ==> br BrTableExpr->targets[1].index()
          // ...
          // Operand == BrTableExpr->targets.size() - 1 ==> br BrTableExpr->targets[BrTableExpr->targets.size() - 1].index()
          // Operand >= BrTableExpr->targets.size() ==> br BrTableExpr->default_target.index()
          //
        // clang-format on

        auto DefaultTarget = BlockStack.rbegin()[BT->default_target.index()];
        auto MergeBB = BasicBlock::Create(*C, Label + "_merge", F);

        for (int i = int(BT->targets.size()) - 1; i >= 0; i--) {
          auto ThenBB = BasicBlock::Create(*C, Label + "_then", F);
          auto ElseBB = BasicBlock::Create(*C, Label + "_else", F);
          auto Condition = B.CreateICmpEQ(Operand, B.getInt32(i));
          WithName(
              Condition->setName(Label + "_condition" + std::to_string(i)));
          B.CreateCondBr(Condition, ThenBB, ElseBB);
          B.SetInsertPoint(ThenBB);
          B.CreateBr(BlockStack.rbegin()[BT->targets[i].index()]);
          B.SetInsertPoint(ElseBB);
        }
        B.CreateBr(DefaultTarget);
        B.SetInsertPoint(MergeBB);
        break;
      }

      case ExprType::Select: {
        auto Operand = Stack.Pop(WithName(Label + "_operand"));

        auto Condition = B.CreateICmpNE(Operand, B.getInt32(0));
        WithName(Condition->setName(Label + "_condition"));

        auto False = Stack.Pop(WithName(Label + "_false"));
        auto True = Stack.Pop(WithName(Label + "_true"));
        assert(True->getType() == False->getType() and
               "true and false must have the same type");

        auto Result = B.CreateSelect(Condition, True, False);
        WithName(Result->setName(Label + "_result"));
        Stack.Push(Result, WithName(Label + "_result"));
        break;
      }

      case ExprType::Binary: {
        auto BE = wabt::cast<BinaryExpr>(&E);

        auto DoBinOp = [&B, &M](const auto Opcode, Value *LHS,
                                Value *RHS) -> Value * {
          switch (Opcode) {
            // clang-format off
              case Opcode::I32Add:
              case Opcode::I64Add: return B.CreateAdd(LHS, RHS);
              case Opcode::I32Sub:
              case Opcode::I64Sub: return B.CreateSub(LHS, RHS);
              case Opcode::I32Mul:
              case Opcode::I64Mul: return B.CreateMul(LHS, RHS);
              case Opcode::I32DivU:
              case Opcode::I64DivU: return B.CreateUDiv(LHS, RHS);
              case Opcode::I32DivS:
              case Opcode::I64DivS: return B.CreateSDiv(LHS, RHS);
              case Opcode::I32RemU:
              case Opcode::I64RemU: return B.CreateURem(LHS, RHS);
              case Opcode::I32RemS:
              case Opcode::I64RemS: return B.CreateSRem(LHS, RHS);
              case Opcode::I32And:
              case Opcode::I64And: return B.CreateAnd(LHS, RHS);
              case Opcode::I32Or:
              case Opcode::I64Or: return B.CreateOr(LHS, RHS);
              case Opcode::I32Xor:
              case Opcode::I64Xor: return B.CreateXor(LHS, RHS);
              case Opcode::I32Shl:
              case Opcode::I64Shl: return B.CreateShl(LHS, RHS);
              case Opcode::I32ShrU:
              case Opcode::I64ShrU: return B.CreateLShr(LHS, RHS);
              case Opcode::I32ShrS:
              case Opcode::I64ShrS: return B.CreateAShr(LHS, RHS);
              case Opcode::I32Rotl:
              case Opcode::I64Rotl: return B.CreateCall(getDeclaration(M.get(), Intrinsic::fshl, LHS->getType()), {LHS, LHS, RHS});
              case Opcode::I32Rotr:
              case Opcode::I64Rotr: return B.CreateCall(getDeclaration(M.get(), Intrinsic::fshr, LHS->getType()), {LHS, LHS, RHS});
              case Opcode::F32Add:
              case Opcode::F64Add: return B.CreateFAdd(LHS, RHS);
              case Opcode::F32Sub:
              case Opcode::F64Sub: return B.CreateFSub(LHS, RHS);
              case Opcode::F32Div:
              case Opcode::F64Div: return B.CreateFDiv(LHS, RHS);
              case Opcode::F32Mul:
              case Opcode::F64Mul: return B.CreateFMul(LHS, RHS);
              case Opcode::F32Min:
              case Opcode::F64Min: return B.CreateCall(getDeclaration(M.get(), Intrinsic::minnum, LHS->getType()), {LHS, RHS});
              case Opcode::F32Max:
              case Opcode::F64Max: return B.CreateCall(getDeclaration(M.get(), Intrinsic::maxnum, LHS->getType()), {LHS, RHS});
              case Opcode::F32Copysign:
              case Opcode::F64Copysign: return B.CreateCall(getDeclaration(M.get(), Intrinsic::copysign, LHS->getType()), {LHS, RHS});
              default: Panic(fmt::format("Unsupported Binary opcode: {}\n", Opcode.GetName()));
            // clang-format on
          }
        };

        auto Right = Stack.Pop(WithName(Label + "_right"));
        auto Left = Stack.Pop(WithName(Label + "_left"));
        assert(Left->getType() == Right->getType() and
               "Binary operands must have same type");

        auto Result = DoBinOp(BE->opcode, Left, Right);

        Stack.Push(Result, WithName(Label + "_result"));
        break;
      }
      case ExprType::Compare: { // TODO: Merge similar ones
        auto CE = wabt::cast<CompareExpr>(&E);

        auto DoCmpOp = [&B](const auto Opcode, Value *LHS, Value *RHS) {
          switch (Opcode) {
            // clang-format off
              case Opcode::I32Eq:
              case Opcode::I64Eq: return B.CreateICmpEQ(LHS, RHS);
              case Opcode::I32Ne:
              case Opcode::I64Ne: return B.CreateICmpNE(LHS, RHS);
              case Opcode::I32LtU:
              case Opcode::I64LtU: return B.CreateICmpULT(LHS, RHS);
              case Opcode::I32LeU:
              case Opcode::I64LeU: return B.CreateICmpULE(LHS, RHS);
              case Opcode::I32GtU:
              case Opcode::I64GtU: return B.CreateICmpUGT(LHS, RHS);
              case Opcode::I32GeU:
              case Opcode::I64GeU: return B.CreateICmpUGE(LHS, RHS);
              case Opcode::I32LtS:
              case Opcode::I64LtS: return B.CreateICmpSLT(LHS, RHS);
              case Opcode::I32LeS:
              case Opcode::I64LeS: return B.CreateICmpSLE(LHS, RHS);
              case Opcode::I32GtS:
              case Opcode::I64GtS: return B.CreateICmpSGT(LHS, RHS);
              case Opcode::I32GeS:
              case Opcode::I64GeS: return B.CreateICmpSGE(LHS, RHS);
              case Opcode::F32Eq:
              case Opcode::F64Eq: return B.CreateFCmpUEQ(LHS, RHS);
              case Opcode::F32Ne:
              case Opcode::F64Ne: return B.CreateFCmpUNE(LHS, RHS);
              case Opcode::F32Lt:
              case Opcode::F64Lt: return B.CreateFCmpULT(LHS, RHS);
              case Opcode::F32Le:
              case Opcode::F64Le: return B.CreateFCmpULE(LHS, RHS);
              case Opcode::F32Gt:
              case Opcode::F64Gt: return B.CreateFCmpUGT(LHS, RHS);
              case Opcode::F32Ge:
              case Opcode::F64Ge: return B.CreateFCmpUGE(LHS, RHS);
              default: Panic(fmt::format("Unsupported Compare opcode: {}", Opcode.GetName()));
            // clang-format on
          }
        };

        auto Right = Stack.Pop(WithName(Label + "_right"));
        auto Left = Stack.Pop(WithName(Label + "_left"));
        assert(Left->getType() == Right->getType() and
               "Binary operands must have same type");

        auto Result = DoCmpOp(CE->opcode, Left, Right);
        Result = B.CreateZExt(Result, B.getInt32Ty());
        WithName(Result->setName(Label + "_result"));

        Stack.Push(Result, WithName(Label + "_result"));
        break;
      }
      case ExprType::Convert: {
        auto CE = wabt::cast<ConvertExpr>(&E);

        auto DoConvertOp = [&B](const auto Opcode, Value *Operand) -> Value * {
          switch (Opcode) {
            // clang-format off
              case Opcode::I32Eqz:
              case Opcode::I64Eqz:
                return B.CreateZExt(B.CreateICmpEQ(Operand, ConstantInt::get(Operand->getType(), 0)), B.getInt32Ty());
              case Opcode::I32WrapI64: // wrap from i64 to i32
                return B.CreateTrunc(Operand, B.getInt32Ty());
              case Opcode::I32TruncF64U: // convert from f64 to unsigned i32 rounding towards zero (.5 will be lost)
              case Opcode::I32TruncF32U: // convert from f32 to signed i32 rounding towards zero (.5 will be lost)
                return B.CreateFPToUI(Operand, B.getInt32Ty());
              case Opcode::I32TruncF64S: // convert from f64 to signed i32 rounding towards zero (.5 will be lost)
              case Opcode::I32TruncF32S: // convert from f32 to signed i32 rounding towards zero (.5 will be lost)
                return B.CreateFPToSI(Operand, B.getInt32Ty());
              case Opcode::I32ReinterpretF32: // reinterpret f32 as i32
                return B.CreateBitCast(Operand, B.getInt32Ty());
              case Opcode::I64ExtendI32U: // zero-extend from i32 to i64
                return B.CreateZExt(Operand, B.getInt64Ty());
              case Opcode::I64ExtendI32S: // sign-extend from i32 to i64
                return B.CreateSExt(Operand, B.getInt64Ty());
              case Opcode::I64TruncF64U: // convert from f64 to unsigned i64 rounding towards zero (.5 will be lost)
              case Opcode::I64TruncF32U: // convert from f32 to unsigned i64 rounding towards zero (.5 will be lost)
                return B.CreateFPToUI(Operand, B.getInt64Ty());
              case Opcode::I64TruncF64S: // convert from f64 to signed i64 rounding towards zero (.5 will be lost)
              case Opcode::I64TruncF32S: // convert from f32 to signed i64 rounding towards zero (.5 will be lost)
                return B.CreateFPToSI(Operand, B.getInt64Ty());
              case Opcode::I64ReinterpretF64: // reinterpret f64 as i64
                return B.CreateBitCast(Operand, B.getInt64Ty());
              case Opcode::F32DemoteF64: // demote from f64 to f32
                return B.CreateFPTrunc(Operand, B.getFloatTy());
              case Opcode::F32ConvertI32S: // convert from signed i32 to f32
              case Opcode::F32ConvertI64S: // convert from signed i64 to f32
                return B.CreateSIToFP(Operand, B.getFloatTy());
              case Opcode::F32ConvertI32U: // convert from unsigned i32 to f32
              case Opcode::F32ConvertI64U: // convert from unsigned i64 to f32
                return B.CreateUIToFP(Operand, B.getFloatTy());
              case Opcode::F32ReinterpretI32: // reinterpret i32 as f32
                return B.CreateBitCast(Operand, B.getFloatTy());
              case Opcode::F64ConvertI32S: // convert from signed i32 to f64
              case Opcode::F64ConvertI64S: // convert from signed i64 to f64
                return B.CreateSIToFP(Operand, B.getDoubleTy());
              case Opcode::F64ConvertI32U: // convert from unsigned i32 to f64
              case Opcode::F64ConvertI64U: // convert from unsigned i64 to f64
                return B.CreateUIToFP(Operand, B.getDoubleTy());
              case Opcode::F64PromoteF32: // promote from f32 to f64
                return B.CreateFPExt(Operand, B.getDoubleTy());
              case Opcode::F64ReinterpretI64: // reinterpret i64 as f64
                return B.CreateBitCast(Operand, B.getDoubleTy());
              default:
                Panic(fmt::format("Unsupported Convert opcode '{}'\n", Opcode.GetName()));
            // clang-format on
          }
        };

        Label = fmt::format("{}.{}", Label, CE->opcode.GetName());

        auto Operand = Stack.Pop(WithName(Label + "_operand"));

        auto Result = DoConvertOp(CE->opcode, Operand);
        WithName(Result->setName(Label + "_result"));

        Stack.Push(Result, WithName(Label + "_result"));
        break;
      }
      case ExprType::Unary: {
        auto UE = wabt::cast<UnaryExpr>(&E);

        auto DoUnaryOp = [&B, &M](const auto Opcode,
                                  Value *Operand) -> Value * {
          switch (Opcode) {
            // clang-format off
              case Opcode::I32Clz:
              case Opcode::I64Clz: return B.CreateCall(getDeclaration(M.get(), Intrinsic::ctlz, Operand->getType()), {Operand, B.getFalse()});
              case Opcode::I32Ctz:
              case Opcode::I64Ctz: return B.CreateCall(getDeclaration(M.get(), Intrinsic::cttz, Operand->getType()), {Operand, B.getFalse()});
              case Opcode::I32Popcnt:
              case Opcode::I64Popcnt: return B.CreateCall(getDeclaration(M.get(), Intrinsic::ctpop, Operand->getType()), {Operand});
              case Opcode::I32Extend16S:
              case Opcode::I32Extend8S: return B.CreateSExt(Operand, B.getInt32Ty());
              case Opcode::I64Extend16S:
              case Opcode::I64Extend32S:
              case Opcode::I64Extend8S: return B.CreateSExt(Operand, B.getInt64Ty());
              case Opcode::F32Sqrt:
              case Opcode::F64Sqrt: return B.CreateCall(getDeclaration(M.get(), Intrinsic::sqrt, Operand->getType()), {Operand});
              case Opcode::F32Nearest:
              case Opcode::F64Nearest: return B.CreateCall(getDeclaration(M.get(), Intrinsic::nearbyint, Operand->getType()), {Operand});
              case Opcode::F32Abs:
              case Opcode::F64Abs: return B.CreateCall(getDeclaration(M.get(), Intrinsic::fabs, Operand->getType()), {Operand});
              case Opcode::F32Ceil:
              case Opcode::F64Ceil: return B.CreateCall(getDeclaration(M.get(), Intrinsic::ceil, Operand->getType()), {Operand});
              case Opcode::F32Floor:
              case Opcode::F64Floor: return B.CreateCall(getDeclaration(M.get(), Intrinsic::floor, Operand->getType()), {Operand});
              case Opcode::F32Trunc:
              case Opcode::F64Trunc: return B.CreateCall(getDeclaration(M.get(), Intrinsic::trunc, Operand->getType()), {Operand});
              case Opcode::F32Neg:
              case Opcode::F64Neg: return B.CreateFNeg(Operand);
              default: Panic(fmt::format("Unsupported Unary opcode '{}'", Opcode.GetName()));
            // clang-format on
          }
        };

        auto Operand = Stack.Pop(WithName(Label + "_operand"));

        auto Result = DoUnaryOp(UE->opcode, Operand);

        Stack.Push(Result, WithName(Label + "_result"));
        break;
      }

      case ExprType::Call: {
        auto CE = wabt::cast<CallExpr>(&E);
        auto Idx = CE->var.index();
        auto Callee = WasmM->funcs[Idx];
        JumpTableCall(B.getInt32(Idx), Callee->decl.sig);
        break;
      }
      case ExprType::CallIndirect: {
        auto CI = wabt::cast<CallIndirectExpr>(&E);

        auto IndirectIdx = Stack.Pop(WithName(Label + "_indirect_idx"));

        // it's important the type be 32-bit integer.
        auto FnIdxPtr = B.CreateGEP(B.getInt32Ty(), Host.IndirectTable,
                                    {IndirectIdx}, "indirect_idx_ptr");
        auto FnIdx = B.CreateLoad(B.getInt32Ty(), FnIdxPtr, "fn_idx");

        JumpTableCall(FnIdx, CI->decl.sig);
        break;
      }

      case ExprType::Return: {
        DoReturn(WasmF->decl.sig.result_types);
        break;
      }

      case ExprType::Unreachable: {
        B.CreateUnreachable();
        break;
      }
      case ExprType::Drop: {
        Stack.Pop(WithName(Label + "_drop"));
        break;
      }

      default:
        Panic(fmt::format("Unsupported ExprType '{}'",
                          GetExprTypeName(ExprType)));
      }
    };
    TranspileExprList = [&B, TranspileExpr, &BlockStack](
                            const ExprList &ExprList, BasicBlock *InBB,
                            BasicBlock *OutBB) -> void {
      B.CreateBr(InBB);
      B.SetInsertPoint(InBB);
      for (auto &Expr : ExprList) {
        TranspileExpr(Expr);
        if (B.GetInsertBlock()->getTerminator())
          break;
      }
      if (not B.GetInsertBlock()->getTerminator())
        B.CreateBr(OutBB);
      OutBB->moveAfter(B.GetInsertBlock());
      B.SetInsertPoint(OutBB);
      BlockStack.pop_back();
    };

    const auto InBB = BasicBlock::Create(*C, "body", F);
    const auto OutBB = BasicBlock::Create(*C, "body_end", F);
    BlockStack.push_back(OutBB);
    TranspileExprList(WasmF->exprs, InBB, OutBB);

    assert(BlockStack.empty() and "BlockStack is not empty");

    if (not B.GetInsertBlock()->getTerminator())
      DoReturn(WasmF->decl.sig.result_types); // implicit return
  };
  DoBody();

#ifdef LLVM_VERIFICATION
  if (verifyFunction(*F, &dbgs())) {
    std::cout << fmt::format("Broken function: {}", WasmF->name) << std::endl;
    F->print(dbgs(), nullptr, false, true);
    Panic(fmt::format("Broken function: {}", WasmF->name));
  }
#endif

#ifdef LLVM_VERIFICATION
  if (verifyModule(*M, &dbgs())) {
    std::cout << fmt::format("Broken Model for function: {}", WasmF->name)
              << std::endl;
    M->print(dbgs(), nullptr, false, true);
    Panic(fmt::format("Broken function: {}", WasmF->name));
  }
#endif
}

std::vector<char> optimise(const std::shared_ptr<llvm::Module> &M,
                           Compiler::OptimizationOptions Opts) {

  auto GetOptimizationLevel = [](auto Level) {
    switch (Level) {
      // clang-format off
    case 0: Panic("Optimization level 0 is not supported for LLVM");
    case 1: return OptimizationLevel::O1;
    case 2: return OptimizationLevel::O2;
    case 3: return OptimizationLevel::O3;
    default: Panic(fmt::format("Invalid optimization level '{}' for LLVM", Level));
      // clang-format on
    }
  };
  auto OptLevel = GetOptimizationLevel(Opts.Level);

  auto TargetTriple = sys::getDefaultTargetTriple();
  std::string Err;
  auto T = TargetRegistry::lookupTarget(TargetTriple, Err);
  if (not T)
    Panic(Err);

  auto CPU = sys::getHostCPUName();
  if (not Opts.Extensions)
    CPU = "generic";

  auto TOs = TargetOptions{};
  auto RM = std::optional(llvm::Reloc::PIC_);

  std::string Features;
  if (not Opts.Extensions) {
    auto FeaturesMap = StringMap<bool>{};
    sys::getHostCPUFeatures(FeaturesMap);
    for (const auto &F : FeaturesMap)
      if (F.second) {
        auto Feature = F.first().str();
        if (Feature == "64bit")
          continue;
        Features += "-" + Feature + ",";
      }
  }

  auto TM = T->createTargetMachine(TargetTriple, CPU, Features, TOs, RM);
  M->setDataLayout(TM->createDataLayout());
  M->setTargetTriple(TargetTriple);

  LoopAnalysisManager LAM;
  FunctionAnalysisManager FAM;
  CGSCCAnalysisManager CGAM;
  ModuleAnalysisManager MAM;

  PassBuilder PB;

  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  auto MPM = PB.buildPerModuleDefaultPipeline(OptLevel);

  MPM.run(*M, MAM);

#ifdef IR_SIZE
  if (std::getenv("IR_SIZE")) {
    auto GetSize = [](const Function &Func) {
      std::string Str;
      raw_string_ostream OS(Str);
      Func.print(OS, nullptr);
      return std::ranges::count(Str, '\n');
    };
    for (auto &Func : *M) {
      if (Func.isDeclaration())
        continue;

      auto Name = WasmM->funcs[std::stoi(Func.getName().str())]->name;
      print(fg(fmt::color::gold), "\n[ir] ");
      print(fg(fmt::color::light_golden_rod_yellow), "{:>4} {}\n",
            GetSize(Func), Name);
    }
    std::cout << std::flush;
  }
#endif

#ifdef LLVM_DUMP_IR
  print(fg(fmt::color ::light_green),
        std::string(Dashes, '-') +
            fmt::format(" Start (post-opt) {} ", WasmM->funcs[FnIdx]->name) +
            std::string(Dashes, '-') + "\n");
  std::cout.flush();
  M->dump();
  print(fg(fmt::color ::light_green),
        std::string(Dashes, '-') +
            fmt::format(" End (post-opt) {} ", WasmM->funcs[FnIdx]->name) +
            std::string(Dashes, '-') + "\n");
  std::cout.flush();
#endif

  legacy::PassManager PM;
  SmallVector<char, 0> Buf;
  raw_svector_ostream OS(Buf);
  if (MCContext *MCC; TM->addPassesToEmitMC(PM, MCC, OS))
    Panic("TargetMachine can't emit a file of this type");
  PM.run(*M);

  return {Buf.begin(), Buf.end()};
}

Compiler::Response LLVMCompiler::compile(const Request Req) {
  // TODO: remove specialization from the Req
  const auto Fn = Req.Fn;
  // fmt::println("[compiling] {}", Fn);
  auto FnName = Opts.M->funcs[Fn]->name;

  const auto C = std::make_shared<LLVMContext>();
  const auto M = std::make_shared<llvm::Module>(FnName, *C);
  M->setSourceFileName("");

  const auto Spec = specialization(Req);
  // if (Spec.has_value())
    // fmt::println("[specializing] {}", Fn);

  transpile(Opts.M, C, M, Fn, Spec);

#ifdef LLVM_DUMP_IR
  int Dashes = 20;
  print(fg(fmt::color::red),
        std::string(Dashes, '-') +
            fmt::format(" Start (pre-opt) {} ", WasmM->funcs[FnIdx]->name) +
            std::string(Dashes, '-') + "\n");
  std::cout.flush();
  M->dump();
  print(fg(fmt::color::red),
        std::string(Dashes, '-') +
            fmt::format(" End (pre-opt) {} ", WasmM->funcs[FnIdx]->name) +
            std::string(Dashes, '-') + "\n");
  std::cout.flush();
#endif

  auto Code = optimise(M, optimizations(Req));

  return Code;
}
