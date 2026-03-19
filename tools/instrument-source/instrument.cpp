#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace clang;

class FunctionCallInserter : public RecursiveASTVisitor<FunctionCallInserter> {
public:
  explicit FunctionCallInserter(Rewriter &R) : TheRewriter(R) {}

  bool VisitCallExpr(CallExpr *Call) {
    SourceManager &SM = TheRewriter.getSourceMgr();
    if (SM.isInMainFile(Call->getExprLoc())) {
      std::string FuncName = Call->getDirectCallee()->getNameInfo().getAsString();
      std::string PrintStmt = "printf(\"Calling: " + FuncName + "\\n\");\n";
      TheRewriter.InsertTextBefore(Call->getBeginLoc(), PrintStmt);
    }
    return true;
  }

private:
  Rewriter &TheRewriter;
};

class MyASTConsumer : public ASTConsumer {
public:
  explicit MyASTConsumer(Rewriter &R) : Inserter(R) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Inserter.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  FunctionCallInserter Inserter;
};

class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef) override {
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<MyASTConsumer>(TheRewriter);
  }

  void EndSourceFileAction() override {
    TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(llvm::outs());
  }

private:
  Rewriter TheRewriter;
};

// Register the plugin
static FrontendPluginRegistry::Add<MyFrontendAction>
    X("func-call-logger", "Inserts print statements before function calls");
