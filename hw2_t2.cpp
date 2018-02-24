/* Yonathan Fisseha, yf2ey
 * Creates a dot file for 'for' loops and if statements. Credit to Eli Bendersky
 * for the 'if' part of the code. I have added the part for 'for' loops per the
 * assignment instruction.*/

#include <sstream>
#include <string>

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

vector<IfStmt *> ifstmtvector;
vector<ForStmt *> forstmtvector;
vector<FunctionDecl> functiondeclvector;
vector<Stmt *> stmtvector;

static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

// get the source code of the specific parts of AST
template <typename T>
static std::string getText(const SourceManager &SourceManager, const T &Node) {
  SourceLocation StartSpellingLocation =
      SourceManager.getSpellingLoc(Node.getLocStart());
  SourceLocation EndSpellingLocation =
      SourceManager.getSpellingLoc(Node.getLocEnd());
  if (!StartSpellingLocation.isValid() || !EndSpellingLocation.isValid()) {
    return std::string();
  }
  bool Invalid = true;
  const char *Text =
      SourceManager.getCharacterData(StartSpellingLocation, &Invalid);
  if (Invalid) {
    return std::string();
  }
  std::pair<FileID, unsigned> Start =
      SourceManager.getDecomposedLoc(StartSpellingLocation);
  std::pair<FileID, unsigned> End =
      SourceManager.getDecomposedLoc(Lexer::getLocForEndOfToken(
          EndSpellingLocation, 0, SourceManager, LangOptions()));
  if (Start.first != End.first) {
    // Start and end are in different files.
    return std::string();
  }
  if (End.second < Start.second) {
    // Shuffling text with macros may cause this.
    return std::string();
  }
  return std::string(Text, End.second - Start.second);
}

// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
  MyASTVisitor(Rewriter &R) : TheRewriter(R) {}

  bool VisitStmt(Stmt *s) {
    // Collect If statements.
    if (isa<IfStmt>(s)) {
      ifstmtvector.push_back(cast<IfStmt>(s));
    }

    // collect for statements as well
    else if (isa<ForStmt>(s)) {
      forstmtvector.push_back(cast<ForStmt>(s));
    }
    // Collect other statements
    else {
      stmtvector.push_back(s);
    }

    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *f) {
    // Only function definitions (with bodies), not declarations.
    if (f->hasBody()) {
      functiondeclvector.push_back(*f);
    }

    return true;
  }

private:
  Rewriter &TheRewriter;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(Rewriter &R) : Visitor(R) {}

  // Override the method that gets called for each parsed top-level
  // declaration.
  bool HandleTopLevelDecl(DeclGroupRef DR) override {
    for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
      // Traverse the declaration using our AST visitor.
      Visitor.TraverseDecl(*b);
      //(*b)->dump();
    }
    return true;
  }

private:
  MyASTVisitor Visitor;
};

// A function that produces the dot file for nested if statements
std::string createDotIf(IfStmt *IfStatement, Rewriter &TheRewriter, int &index,
                        ASTContext *Context) {
  FullSourceLoc FullLocation = Context->getFullLoc(IfStatement->getLocStart());

  // grab the condition line num
  std::string lineNum = std::to_string(FullLocation.getSpellingLineNumber());
  lineNum = " Line: " + lineNum;

  std::string condition =
      getText(TheRewriter.getSourceMgr(), *IfStatement->getCond());
  std::string dotted = "";

  std::string index1 = std::to_string(index + 1);
  std::string index0 = std::to_string(index);
  std::string index2 = std::to_string(index + 2);
  std::string index3 = std::to_string(index + 3);
  std::string index4 = std::to_string(index + 4);

  // index example: 2
  // if block: node3
  dotted += "Node" + index1 + " [shape=record,label=\"" + lineNum + "\"]\n";
  index++;

  // node2 points to node3
  dotted += "Node" + index0 + "->Node" + index1 + ";\n";

  Stmt *Then = IfStatement->getThen();
  string thenpart = getText(TheRewriter.getSourceMgr(), *Then);

  // then block: node 4
  dotted += "Node" + index2 + " [shape=record,label=\"" + thenpart + "\"]\n";
  index++;

  // node3 points to node 4
  dotted += "Node" + index1 + "->Node" + index2 + ";\n";

  // end node: node6
  dotted +=
      "Node" + index4 + " [shape=record,label=\"{ [(DummyNode)]\\l}\"];\n";
  index++;

  // node4 points to node 6
  dotted += "Node" + index2 + " -> Node" + index4 + ";\n";

  Stmt *Else = IfStatement->getElse();
  if (Else) {
    string elsepart = getText(TheRewriter.getSourceMgr(), *Else);
    // else block: block 5
    dotted += "Node" + index3 + " [shape=record,label=\"" + elsepart + "\"]\n";
    index++;

    // index4 points at index5
    dotted += "Node" + index2 + " -> Node" + index3 + ";\n";

    // index5 points at index6
    dotted += "Node" + index3 + " -> Node" + index4 + ";\n";
  }

  return dotted;
}

/* a function that returns a dot string for 'for' loops */
std::string createDotFor(ForStmt *ForStatement, Rewriter &TheRewriter,
                         int &index, ASTContext *Context) {

  FullSourceLoc FullLocation = Context->getFullLoc(ForStatement->getLocStart());

  // grab the condition line num
  std::string lineNum = std::to_string(FullLocation.getSpellingLineNumber());
  std::string dotted = "";

  // setup the nodes indices
  std::string index0 = std::to_string(index + 0);
  std::string index1 = std::to_string(index + 1);
  std::string index2 = std::to_string(index + 2);
  std::string index3 = std::to_string(index + 3);
  std::string index4 = std::to_string(index + 4);

  lineNum = " Line: " + lineNum;

  // example: prev node is 2

  // the condition block: node3
  dotted += "Node" + index1 + " [shape=record,label=\"" + lineNum + "\"]\n";
  index++;

  // the previous node points to the condition code
  dotted += "Node" + index0 + "->Node" + index1 + ";\n";

  // get body of loop: node4
  Stmt *body = ForStatement->getBody();
  string bodyPart = getText(TheRewriter.getSourceMgr(), *body);

  dotted += "Node" + index2 + " [shape=record,label=\"" + bodyPart + "\"]\n";
  index++;

  // the condition block points to the body
  dotted += "Node" + index1 + "->Node" + index2 + ";\n";

  // the body points back at the condition
  dotted += "Node" + index2 + "->Node" + index1 + ";\n";

  // end node: node3
  dotted +=
      "Node" + index3 + " [shape=record,label=\"{ [(DummyNode)]\\l}\"];\n";
  index++;

  // the condition code points to the exit node
  dotted += "Node" + index1 + " -> Node" + index3 + ";\n";

  return dotted;
}

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
  MyFrontendAction() {}
  void EndSourceFileAction() override {
    SourceManager &SM = TheRewriter.getSourceMgr();
    llvm::errs() << "** EndSourceFileAction for: "
                 << SM.getFileEntryForID(SM.getMainFileID())->getName() << "\n";

    // handle each function definition
    for (unsigned funcid = 0; funcid < functiondeclvector.size(); funcid++) {
      FunctionDecl *f = &functiondeclvector[funcid];
      Stmt *FuncBody = f->getBody();

      // Function name
      DeclarationName DeclName = f->getNameInfo().getName();
      std::string FuncName = DeclName.getAsString();

      FullSourceLoc funcstart = Context->getFullLoc(FuncBody->getLocStart());
      FullSourceLoc funcend = Context->getFullLoc(FuncBody->getLocEnd());
      unsigned startln = funcstart.getSpellingLineNumber();
      unsigned endln = funcend.getSpellingLineNumber();
      ofstream myfile;
      myfile.open(FuncName + ".dot");
      cout << (FuncName + ".dot") << endl;
      myfile << "digraph unnamed {\n";

      myfile << "Node1 [shape=record,label=\"{ [(ENTRY)]\\l}\"];\n";

      myfile << "Node2 [shape=record,label=\"Dummy Node\"]\n";
      myfile << "Node1 -> Node2;\n";

      /* call function to produce the dot strings */

      string dottedIf = "";
      int j = 2;
      for (int i = 0; i < ifstmtvector.size(); i++) {
        IfStmt *IfStatement = ifstmtvector[i];
        dottedIf += createDotIf(IfStatement, TheRewriter, j, Context);
      }

      for (int i = 0; i < forstmtvector.size(); i++) {
        ForStmt *ForStatement = forstmtvector[i];
        dottedIf += createDotFor(ForStatement, TheRewriter, j, Context);
      }

      myfile << dottedIf << "\n";
      myfile << "}\n";
      myfile.close();
    }
  }

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    llvm::errs() << "** Creating AST consumer for: " << file << "\n";
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    Context = &CI.getASTContext();
    return llvm::make_unique<MyASTConsumer>(TheRewriter);
  }

private:
  Rewriter TheRewriter;
  ASTContext *Context;
};

int main(int argc, const char **argv) {
  CommonOptionsParser op(argc, argv, ToolingSampleCategory);
  ClangTool Tool(op.getCompilations(), op.getSourcePathList());

  // ClangTool::run accepts a FrontendActionFactory, which is then used to
  // create new objects implementing the FrontendAction interface. Here we use
  // the helper newFrontendActionFactory to create a default factory that will
  // return a new MyFrontendAction object every time.
  // To further customize this, we could create our own factory class.
  return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
