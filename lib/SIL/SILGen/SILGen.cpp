//===--- SILGen.cpp - Implements Lowering of ASTs -> SIL ------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "SILGen.h"
#include "llvm/ADT/Optional.h"
#include "swift/AST/AST.h"
using namespace swift;
using namespace Lowering;

//===--------------------------------------------------------------------===//
// SILGenFunction Class implementation
//===--------------------------------------------------------------------===//

// TODO: more accurately port the result schema logic from
// IRGenFunction::emitEpilogue to handle all cases where a default void return
// is needed
static bool isVoidableType(Type type) {
  if (TupleType *tt = type->getAs<TupleType>()) {
    return tt->getFields().empty();
  } else
    return false;
}

SILGenFunction::SILGenFunction(SILGenModule &SGM, Function &F,
                               bool hasVoidReturn)
  : SGM(SGM), F(F), B(new (F.getModule()) BasicBlock(&F), F),
    Cleanups(*this),
    hasVoidReturn(hasVoidReturn) {
}

SILGenFunction::SILGenFunction(SILGenModule &SGM, Function &F, FuncExpr *FE)
  : SILGenFunction(SGM, F,
          /*hasVoidReturn=*/isVoidableType(FE->getResultType(F.getContext()))) {

  emitProlog(FE);
}

TupleInst *SILBuilder::createEmptyTuple(SILLocation Loc) {
  return createTuple(Loc, TupleType::getEmpty(F.getContext()),
                     ArrayRef<Value>());
}

/// SILGenFunction destructor - called after the entire function's AST has been
/// visited.  This handles "falling off the end of the function" logic.
SILGenFunction::~SILGenFunction() {
  // If the end of the function isn't reachable (e.g. it ended in an explicit
  // return), then we're done.
  if (!B.hasValidInsertionPoint())
    return;
  
  // If we have an unterminated block, it is either an implicit return of an
  // empty tuple, or a dynamically unreachable location.
  if (hasVoidReturn) {
    Value emptyTuple = B.createEmptyTuple(SILLocation());
    Cleanups.emitReturnAndCleanups(SILLocation(), emptyTuple);
  } else {
    B.createUnreachable();
  }
}

//===--------------------------------------------------------------------===//
// SILGenModule Class implementation
//===--------------------------------------------------------------------===//

SILGenModule::SILGenModule(SILModule &M)
  : M(M), Types(*this), TopLevelSGF(nullptr) {
  if (M.toplevel)
    TopLevelSGF = new SILGenFunction(*this, *M.toplevel,
                                     /*hasVoidReturn=*/true);
}

SILGenModule::~SILGenModule() {
  delete TopLevelSGF;
}

void SILGenModule::visitFuncDecl(FuncDecl *fd) {
  emitFunction(SILConstant(fd), fd->getBody());
}

Function *SILGenModule::emitFunction(SILConstant decl, FuncExpr *fe) {
  // Ignore prototypes.
  if (fe->getBody() == nullptr) return nullptr;
  
  assert(!M.hasFunction(decl) &&
         "already generated function for decl!");
  
  Function *f = new (M) Function(M);
  SILGenFunction(*this, *f, fe).visit(fe->getBody());
  
  f->verify();
  M.functions[decl] = f;
  
  return f;
}

void SILGenModule::visitPatternBindingDecl(PatternBindingDecl *pd) {
  if (TopLevelSGF) {
    TopLevelSGF->visitPatternBindingDecl(pd);
  } else {
    // FIXME: generate accessor functions for global variables in non-top-level
    // code
  }
}

//===--------------------------------------------------------------------===//
// SILModule::constructSIL method implementation
//===--------------------------------------------------------------------===//


SILModule *SILModule::constructSIL(TranslationUnit *tu) {
  bool hasTopLevel = true;
  switch (tu->Kind) {
  case TranslationUnit::Library:
    hasTopLevel = false;
    break;
  case TranslationUnit::Main:
  case TranslationUnit::Repl:
    hasTopLevel = true;
    break;
  }
  SILModule *m = new SILModule(tu->getASTContext(), hasTopLevel);
  SILGenModule sgm(*m);
  for (Decl *D : tu->Decls)
    sgm.visit(D);
  return m;
}