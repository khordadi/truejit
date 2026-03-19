#include "Wasm/WasmService.h"
#include <fmt/format.h>
#include <fstream>
#include <set>
#include <variant>
#include <wabt/cast.h>

using namespace fmt::literals;

int main(const int Argc, char **Argv) {
  if (Argc != 5)
    ErrAndExit("Usage: -i <input.wat> -o <output.json>");

  std::string Input = Argv[2];
  std::string Output = Argv[4];

  auto Application = Input;

  auto WS = std::make_shared<WasmService>();
  const auto M = WS->get(Application);

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

  // mkdir if not exists
  if (auto OutputPath = std::filesystem::path(Output);
      not exists(OutputPath.parent_path()))
    create_directories(OutputPath.parent_path());

  std::ofstream ProfileFile(Output);
  ProfileFile << J.dump(2);

  return 0;
}