#include "Wasm/WasmService.h"
#include <fmt/color.h>
#include <fmt/format.h>
#include <fstream>
#include <optional>
#include <ranges>
#include <set>
#include <stack>
#include <utility>
#include <variant>
#include <wabt/binary-reader-ir.h>
#include <wabt/cast.h>
#include <wabt/expr-visitor.h>
#include <wabt/ir.h>

#define PRINT_ALL true

struct Func;
struct Block;
struct Loop;
struct Br;
struct BrIf;
struct Call;
struct Return;
struct Unreachable;
struct Default;
using Node = std::variant<Func, Block, Loop, Br, BrIf, Call, Return,
                          Unreachable, Default>;

// Metadata
struct BranchCondition {
  std::string Condition;
};
struct BranchTarget {
  std::string Target;
  u32 Depth;
};

struct BranchStaticLikelihood {
  std::string Probability;
  std::string Reason;
};

struct BranchDynamicLikelihood {
  u32 True;
  u32 False;
};

struct Reachability {
  std::string Probability;
  std::string Reason;
};

using Metadata =
    std::variant<BranchCondition, BranchTarget, BranchStaticLikelihood,
                 BranchDynamicLikelihood, Reachability>;

struct MetadataToString {
  std::string operator()(const BranchCondition &BC) const {
    return format(fg(fmt::color::pink), "[{}]", BC.Condition);
  }
  std::string operator()(const BranchTarget &BT) const {
    constexpr auto BlockStyle = fg(fmt::color::yellow);
    constexpr auto LoopStyle = fg(fmt::color::green_yellow);
    const auto Depth = std::string(BT.Depth + 1, '<');
    if (BT.Target == "block")
      return format(BlockStyle, "[{}]", Depth);
    if (BT.Target == "loop")
      return format(LoopStyle, "[{}]", Depth);
    ErrAndExit("Unknown branch target");
  }
  std::string operator()(const BranchStaticLikelihood &BL) const {
    return format(fg(fmt::color::lime_green), "[{}: {}]", BL.Probability,
                  BL.Reason);
  }
  std::string operator()(const BranchDynamicLikelihood &BD) const {
    auto TrueStr = format(fg(fmt::color::light_green), "{}", BD.True);
    auto FalseStr = format(fg(fmt::color::pale_violet_red), "{}", BD.False);
    return fmt::format("[{}|{}]", TrueStr, FalseStr);
  }
  std::string operator()(const Reachability &R) const {
    return fmt::format(fg(fmt::color::red), "[{}]", R.Probability);
  }
};

using Line = std::pair<int, std::string>;

struct Func {
  Line Start;
  std::vector<Node> Nodes;
};

struct Block {
  Line Start;
  Line End;
  std::vector<Node> Nodes;
};
struct Loop {
  Line Start;
  Line End;
  std::vector<Node> Nodes;
};

struct Br {
  Line Loc;

  // metadata
  // std::variant<std::shared_ptr<Block>, std::shared_ptr<Loop>> Target;
  // bool Likely;
};

struct BrIf {
  Line Loc;

  // metadata
  // std::string Condition;
  // std::string BranchTarget;
  std::vector<Metadata> MD;
};

struct Call {
  Line Loc;

  // metadata
  // std::string Function;
};

struct Return {
  Line Loc;
};

struct Unreachable {
  Line Loc;
};

struct Default {
  Line Loc;
};

std::vector<std::string> getModuleLines(const std::string &FilePath) {
  auto ModuleLines = std::vector<std::string>{};
  std::ifstream ModuleFile(FilePath);
  std::string Line;
  while (std::getline(ModuleFile, Line))
    ModuleLines.push_back(Line);
  return ModuleLines;
}

std::shared_ptr<wabt::Module> getModule(const std::string &FilePath) {
  auto WMS = WasmService();
  return WMS.get(FilePath);
}

class BranchHinter {
public:
  explicit BranchHinter(const std::string &Application) {
    Module = getModule(Application);
    Lines = getModuleLines(Application);
  }

  void Process(const std::string &Function) {
    auto F = Parse(Find(Function));

    std::visit(StaticAnalysis{}, F);

    std::visit(DynamicAnalysis{}, F);

    std::visit(Print{PRINT_ALL}, F);

    print(fg(fmt::color::gray) | fmt::emphasis::italic | fmt::emphasis::bold,
          "-----------------------------------------\n");

    std::visit(Print{}, F);
  }

private:
  std::shared_ptr<wabt::Module> Module;
  std::vector<std::string> Lines;

  wabt::Func *Find(const std::string &FuncName) {
    if (const auto Found = std::ranges::find_if(
            Module->funcs,
            [&](const auto &Func) { return Func->name == FuncName; });
        Found != Module->funcs.end())
      return *Found;
    ErrAndExit(fmt::format("Function {} not found", FuncName));
  }

  Node Parse(const wabt::Func *F) {
    auto Nodes = std::vector<Node>{};
    for (auto I = F->exprs.begin(); I != F->exprs.end(); ++I)
      Nodes.emplace_back(Parse(I));
    return {Func{Line{F->loc.line, Lines[F->loc.line - 1]}, Nodes}};
  }
  Node Parse(const wabt::ExprList::const_iterator I) {
    using BlockStack = std::vector<
        std::variant<const wabt::BlockExpr *, const wabt::LoopExpr *>>;
    static BlockStack Stack;
    auto &E = *I;
    switch (E.type()) {
    case wabt::ExprType::Block: {
      const auto B = wabt::cast<wabt::BlockExpr>(&E);

      Stack.emplace_back(B);

      std::vector<Node> Nodes;
      for (auto II = B->block.exprs.begin(); II != B->block.exprs.end(); ++II)
        Nodes.emplace_back(Parse(II));

      Stack.pop_back();

      return {
          Block{Line{B->loc.line, Lines[B->loc.line - 1]},
                Line{B->block.end_loc.line, Lines[B->block.end_loc.line - 1]},
                Nodes}};
    }
    case wabt::ExprType::Loop: {
      const auto L = wabt::cast<wabt::LoopExpr>(&E);

      Stack.emplace_back(L);

      std::vector<Node> Nodes;
      for (auto II = L->block.exprs.begin(); II != L->block.exprs.end(); ++II)
        Nodes.emplace_back(Parse(II));

      Stack.pop_back();

      return {
          Loop{Line{L->loc.line, Lines[L->loc.line - 1]},
               Line{L->block.end_loc.line, Lines[L->block.end_loc.line - 1]},
               Nodes}};
    }

    case wabt::ExprType::Br: {
      const auto BE = wabt::cast<wabt::BrExpr>(&E);

      return {Br{Line{BE->loc.line, Lines[BE->loc.line - 1]}}};
    }
    case wabt::ExprType::BrIf: {
      const auto BrIfE = wabt::cast<wabt::BrIfExpr>(&E);

      const auto &CondE = *std::prev(I);

      auto CondStr = Lines[CondE.loc.line - 1];
      CondStr.erase(CondStr.begin(),
                    std::ranges::find_if(CondStr, [](const unsigned char Ch) {
                      return not std::isspace(Ch);
                    }));
      BranchCondition Cond{CondStr};

      const auto TargetDepth = BrIfE->var.index();
      const auto TargetE = Stack[Stack.size() - 1 - TargetDepth];
      std::string TargetStr;
      if (std::holds_alternative<const wabt::BlockExpr *>(TargetE)) {
        TargetStr = "block";
      } else if (std::holds_alternative<const wabt::LoopExpr *>(TargetE)) {
        TargetStr = "loop";
      }
      BranchTarget Target{TargetStr, TargetDepth};

      return {BrIf{Line{BrIfE->loc.line, Lines[BrIfE->loc.line - 1]},
                   {Cond, Target}}};
    }
    case wabt::ExprType::Call: {
      const auto CE = wabt::cast<wabt::CallExpr>(&E);

      return {Call{Line{CE->loc.line, Lines[CE->loc.line - 1]}}};
    }

    case wabt::ExprType::Return: {
      const auto RE = wabt::cast<wabt::ReturnExpr>(&E);

      return {Return{Line{RE->loc.line, Lines[RE->loc.line - 1]}}};
    }

    case wabt::ExprType::Unreachable: {
      const auto UE = wabt::cast<wabt::UnreachableExpr>(&E);

      return {Unreachable{Line{UE->loc.line, Lines[UE->loc.line - 1]}}};
    }

    default:
      return {Default{Line{E.loc.line, Lines[E.loc.line - 1]}}};
    }
  };

  struct StaticAnalysis {
    void operator()(Func &Func) const {
      for (auto &N : Func.Nodes)
        std::visit(*this, N);
    }
    void operator()(Block &Block) const {
      for (auto &N : Block.Nodes)
        std::visit(*this, N);
    }
    void operator()(Loop &Loop) const {
      for (auto &N : Loop.Nodes)
        std::visit(*this, N);
    }
    void operator()(Br &Br) const {}
    void operator()(BrIf &BrIf) const {
      for (const auto &MD : BrIf.MD) {
        if (std::holds_alternative<BranchCondition>(MD)) {
          if (const auto &[Condition] = std::get<BranchCondition>(MD);
              Condition == "i32.eqz")
            BrIf.MD.emplace_back(BranchStaticLikelihood{
                "unlikely", "condition is compare with zero"});
          else if (Condition.starts_with("call"))
            BrIf.MD.emplace_back(BranchStaticLikelihood{
                "unlikely", "condition is a function call"});
        }
        if (std::holds_alternative<BranchTarget>(MD)) {
          if (const auto &[Target, Depth] = std::get<BranchTarget>(MD);
              Target == "loop") {
            BrIf.MD.emplace_back(
                BranchStaticLikelihood{"likely", "branch target is loop"});
            // TODO: do something about depth too! e.g. if depth is big...
          }
        }
      }
    }
    void operator()(Call &Call) const {}
    void operator()(Return &Return) const {}
    void operator()(Unreachable &Unreachable) const {}
    void operator()(Default &Default) const {}
  };

  struct DynamicAnalysis {
    struct Branch {
      u32 True;
      u32 False;
    };
    std::unordered_map<u32, Branch> Branches;
    DynamicAnalysis() {
      std::ifstream ProfileFile("/tmp/branches.json");
      json Profile;
      ProfileFile >> Profile;
      for (const auto &[Line, Branch] : Profile.items())
        Branches[std::stoi(Line)] = {Branch[0], Branch[1]};
    }
    void operator()(Func &Func) const {
      for (auto &N : Func.Nodes)
        std::visit(*this, N);
    }
    void operator()(Block &Block) const {
      for (auto &N : Block.Nodes)
        std::visit(*this, N);
    }
    void operator()(Loop &Loop) const {
      for (auto &N : Loop.Nodes)
        std::visit(*this, N);
    }
    void operator()(Br &Br) const {}
    void operator()(BrIf &BrIf) const {
      if (const auto Found = Branches.find(BrIf.Loc.first);
          Found != Branches.end()) {
        const auto &[True, False] = Found->second;
        BrIf.MD.emplace_back(BranchDynamicLikelihood{True, False});
      }
    }
    void operator()(Call &Call) const {}
    void operator()(Return &Return) const {}
    void operator()(Unreachable &Unreachable) const {}
    void operator()(Default &Default) const {}
  };

  struct Print {
    explicit Print(const bool PrintAll = false) : PrintAll(PrintAll) {}
    bool PrintAll;

    void operator()(const Func &Func) const {
      constexpr auto LineStyle = fg(fmt::color::orchid);

      println(LineStyle, Func.Start);
      for (const auto &N : Func.Nodes)
        std::visit(*this, N);
    }
    void operator()(const Block &Block) const {
      constexpr auto LineStyle = fg(fmt::color::yellow);

      println(LineStyle, Block.Start);
      for (const auto &N : Block.Nodes)
        std::visit(*this, N);
      println(LineStyle, Block.End);
    }
    void operator()(const Loop &Loop) const {
      constexpr auto LineStyle = fg(fmt::color::green_yellow);

      println(LineStyle, Loop.Start);
      for (const auto &N : Loop.Nodes)
        std::visit(*this, N);
      println(LineStyle, Loop.End);
    }
    void operator()(const Br &Br) const {
      constexpr auto LineStyle = fg(fmt::color::orange);

      println(LineStyle, Br.Loc);
    }
    void operator()(const BrIf &BrIf) const {
      // TODO: move some of the logic to the visitor
      constexpr auto LineStyle = fg(fmt::color::red);

      auto Metadata = std::string{};
      for (const auto &MD : BrIf.MD)
        Metadata += std::visit(MetadataToString{}, MD) + " ";

      println(LineStyle, BrIf.Loc, Metadata);
    }
    void operator()(const Call &Call) const {
      constexpr auto LineStyle = fg(fmt::color::sky_blue);

      println(LineStyle, Call.Loc);
    }
    void operator()(const Return &Return) const {
      constexpr auto LineStyle = fg(fmt::color::light_green);

      println(LineStyle, Return.Loc);
    }
    void operator()(const Unreachable &Unreachable) const {
      constexpr auto LineStyle = fg(fmt::color::purple);

      println(LineStyle, Unreachable.Loc);
    }
    void operator()(const Default &Default) const {
      constexpr auto LineStyle = fg(fmt::color::white);

      if (not PrintAll)
        return;
      println(LineStyle, Default.Loc);
    }

  private:
    fmt::text_style LineNumStyle = fg(fmt::color::cyan);

    void println(const fmt::text_style &LineStyle, const Line &L,
                 const std::string &Metadata = "") const {
      auto LineNum = format(LineNumStyle, "{:>4}", L.first);
      auto Line = format(LineStyle, "{}", L.second);
      fmt::println("{}{} {}", LineNum, Line, Metadata);
    }
  };
};

int main(const int Argc, char **Argv) {
  if (Argc != 5)
    ErrAndExit("Usage: -i <input.wat> -o <output.json>");

  auto Input = std::string{};
  auto Output = std::string{};
  for (auto I = 1; I < Argc; I += 2) {
    if (std::string(Argv[I]) == "-i")
      Input = Argv[I + 1];
    if (std::string(Argv[I]) == "-o")
      Output = Argv[I + 1];
  }

  auto Application = Input;

  const std::shared_ptr<wabt::Module> M = getModule(Application);

  // map from caller to callee
  std::map<u32, std::set<u32>> CallGraph;

  using ExpressionType =
      std::variant<const wabt::BlockExpr *, const wabt::LoopExpr *,
                   const wabt::CallExpr *>;

  struct CallGraphBuilder {
    const std::shared_ptr<wabt::Module> M;
    std::set<u32> operator()(const wabt::CallExpr *CE) const {
      auto Idx = CE->var.index();
      if (M->funcs[Idx]->name.starts_with(
              "$__imported_wasi_snapshot_preview1_"))
        return {};
      return {Idx};
    }
    std::set<u32> operator()(const wabt::BlockExpr *BE) const {
      std::set<u32> Callees;
      for (const auto &E : BE->block.exprs) {
        if (E.type() == wabt::ExprType::Call) {
          auto CE = wabt::cast<wabt::CallExpr>(&E);
          auto Res = std::visit(*this, ExpressionType(CE));
          Callees.insert(Res.begin(), Res.end());
        }
        if (E.type() == wabt::ExprType::Block) {
          auto BE = wabt::cast<wabt::BlockExpr>(&E);
          auto Res = std::visit(*this, ExpressionType(BE));
          Callees.insert(Res.begin(), Res.end());
        }
        if (E.type() == wabt::ExprType::Loop) {
          auto LE = wabt::cast<wabt::LoopExpr>(&E);
          auto Res = std::visit(*this, ExpressionType(LE));
          Callees.insert(Res.begin(), Res.end());
        }
      }
      return Callees;
    }

    std::set<u32> operator()(const wabt::LoopExpr *LE) const {
      std::set<u32> Callees;
      for (const auto &E : LE->block.exprs) {
        if (E.type() == wabt::ExprType::Call) {
          auto CE = wabt::cast<wabt::CallExpr>(&E);
          auto Res = std::visit(*this, ExpressionType(CE));
          Callees.insert(Res.begin(), Res.end());
        }
        if (E.type() == wabt::ExprType::Block) {
          auto BE = wabt::cast<wabt::BlockExpr>(&E);
          auto Res = std::visit(*this, ExpressionType(BE));
          Callees.insert(Res.begin(), Res.end());
        }
        if (E.type() == wabt::ExprType::Loop) {
          auto LE = wabt::cast<wabt::LoopExpr>(&E);
          auto Res = std::visit(*this, ExpressionType(LE));
          Callees.insert(Res.begin(), Res.end());
        }
      }
      return Callees;
    }
  };

  json J;
  for (u32 FIdx = 0; FIdx < M->funcs.size(); ++FIdx) {
    auto F = M->funcs[FIdx];
    // if wasi function, skip
    if (F->name.starts_with("$__imported_wasi_snapshot_preview1"))
      continue;
    // std::cout << F->name << " -> "
    // << "\n";
    auto Callees = std::set<u32>{};
    for (const auto &E : F->exprs) {
      CallGraphBuilder CGB{M};
      if (E.type() == wabt::ExprType::Call) {
        auto CE = wabt::cast<wabt::CallExpr>(&E);
        auto Res = std::visit(CGB, ExpressionType(CE));
        Callees.insert(Res.begin(), Res.end());
      }
      if (E.type() == wabt::ExprType::Block) {
        auto BE = wabt::cast<wabt::BlockExpr>(&E);
        auto Res = std::visit(CGB, ExpressionType(BE));
        Callees.insert(Res.begin(), Res.end());
      }
      if (E.type() == wabt::ExprType::Loop) {
        auto LE = wabt::cast<wabt::LoopExpr>(&E);
        auto Res = std::visit(CGB, ExpressionType(LE));
        Callees.insert(Res.begin(), Res.end());
      }
    }
    J[std::to_string(FIdx)] = Callees;
  }

  // std::cout << J.dump(2) << "\n";

  // write J to file
  std::ofstream ProfileFile(Output);
  ProfileFile << J.dump(2);

  return 0;

  auto BH = BranchHinter(Application);

  auto HistoryPath = "/truejit/benchmarks/ffmpeg/ffmpeg.oracle.json";
  std::ifstream ProfileFile(HistoryPath);
  json Profile;
  ProfileFile >> Profile;
  auto History = Profile.get<std::vector<u32>>();

  for (const auto I : History) {
    auto F = M->funcs[I];

    BH.Process(F->name);

    print(fg(fmt::color::gray) | fmt::emphasis::italic | fmt::emphasis::bold,
          "=========================================\n");
    std::cout.flush();
    // std::this_thread::sleep_for(milliseconds(100));
    // wait for user key press
    std::cin.get();
  }

  return 0;
}
