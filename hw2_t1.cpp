/* Scans through a C file using the AST libraries and tries to find an if
 * statement that uses = instead of == and prints the line number of this if
 * statement
 */

#include <sstream>
#include <string>

// Just get everything lol
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

/* a visitor for the consumer */
class FindBuggyIfVisitor : public RecursiveASTVisitor<FindBuggyIfVisitor> {
public:
  explicit FindBuggyIfVisitor(ASTContext *Context) : Context(Context) {}
  bool VisitIfStmt(IfStmt *s) {
    // check for if statements
    FullSourceLoc FullLocation = Context->getFullLoc(s->getLocStart());
    clang::Expr *expr = s->getCond();
    clang::BinaryOperator *op = (BinaryOperator *)expr;
    if (!op->isComparisonOp()) {
      if (FullLocation.isValid())
        llvm::outs() << "line " << FullLocation.getSpellingLineNumber() << "\n";
    }
    return true;
  }

private:
  ASTContext *Context;
};

/* A consumer for our action */
class FindBuggyIfConsumer : public clang::ASTConsumer {
public:
  explicit FindBuggyIfConsumer(ASTContext *Context) : Visitor(Context) {}
  virtual void HandleTranslationUnit(clang::ASTContext &Context) {
    // visit every node
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  // a visitor
  FindBuggyIfVisitor Visitor;
};

/* The FrontendAction implementation */
class FindBuggyIfAction : public clang::ASTFrontendAction {
public:
  virtual std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
    return std::unique_ptr<clang::ASTConsumer>(
        new FindBuggyIfConsumer(&Compiler.getASTContext()));
  }
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, ToolingSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  return Tool.run(newFrontendActionFactory<FindBuggyIfAction>().get());

  // clang::tooling::runToolOnCode(new FindBuggyIfAction, argv[1]);
}
