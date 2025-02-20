#include <map>
#include <vector>
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/ErrorHandling.h"
#include "ast.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
using namespace llvm;

extern "C" int yyparse();
extern ASTNode* root;
extern void printAST(ASTNode* node, int level);

LLVMContext Context;
Module *TheModule = new Module("GoofyLang", Context);
IRBuilder<> Builder(Context);
std::map<std::string, Value*> NamedValues;

// Utility: Create an alloca in the entry block.
static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, const std::string &VarName, Type *type) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(type, nullptr, VarName);
}

// Helper: Get or create declaration for printf.
Function* getPrintfFunction() {
  Function *printfFunc = TheModule->getFunction("printf");
  if (!printfFunc) {
    std::vector<Type*> printfArgs;
    printfArgs.push_back(PointerType::get(Type::getInt8Ty(Context), 0));
    FunctionType* printfType = FunctionType::get(Type::getInt32Ty(Context), printfArgs, true);
    printfFunc = Function::Create(printfType, Function::ExternalLinkage, "printf", TheModule);
  }
  return printfFunc;
}

// Global format strings.
GlobalVariable* getFormatStringInt() {
  GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_int");
  if (!fmtStrVar) {
    Constant *formatStr = ConstantDataArray::getString(Context, "%d\n", true);
    fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                   GlobalValue::PrivateLinkage, formatStr, ".str_int");
  }
  return fmtStrVar;
}

GlobalVariable* getFormatStringFloat() {
  GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_float");
  if (!fmtStrVar) {
    Constant *formatStr = ConstantDataArray::getString(Context, "%.1f\n", true);
    fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                   GlobalValue::PrivateLinkage, formatStr, ".str_float");
  }
  return fmtStrVar;
}

GlobalVariable* getFormatStringChar() {
  GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_char");
  if (!fmtStrVar) {
    Constant *formatStr = ConstantDataArray::getString(Context, "%c\n", true);
    fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                   GlobalValue::PrivateLinkage, formatStr, ".str_char");
  }
  return fmtStrVar;
}

GlobalVariable* getFormatStringStr() {
  GlobalVariable *fmtStrVar = TheModule->getNamedGlobal(".str_string");
  if (!fmtStrVar) {
    Constant *formatStr = ConstantDataArray::getString(Context, "%s\n", true);
    fmtStrVar = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                   GlobalValue::PrivateLinkage, formatStr, ".str_string");
  }
  return fmtStrVar;
}

// Helper: String concatenation.
Function* getConcatFunction() {
  Function *concatFunc = TheModule->getFunction("concat_strings");
  if (!concatFunc) {
    std::vector<Type*> concatArgs;
    concatArgs.push_back(PointerType::get(Type::getInt8Ty(Context), 0));
    concatArgs.push_back(PointerType::get(Type::getInt8Ty(Context), 0));
    FunctionType* concatType = FunctionType::get(PointerType::get(Type::getInt8Ty(Context), 0), concatArgs, false);
    concatFunc = Function::Create(concatType, Function::ExternalLinkage, "concat_strings", TheModule);
  }
  return concatFunc;
}

// --- New: Input runtime functions (from second file) ---
Function* getReadIntFunction() {
  Function *readIntFunc = TheModule->getFunction("read_int");
  if (!readIntFunc) {
    FunctionType* funcType = FunctionType::get(Type::getInt32Ty(Context), false);
    readIntFunc = Function::Create(funcType, Function::ExternalLinkage, "read_int", TheModule);
  }
  return readIntFunc;
}
Function* getReadFloatFunction() {
  Function *readFloatFunc = TheModule->getFunction("read_float");
  if (!readFloatFunc) {
    FunctionType* funcType = FunctionType::get(Type::getFloatTy(Context), false);
    readFloatFunc = Function::Create(funcType, Function::ExternalLinkage, "read_float", TheModule);
  }
  return readFloatFunc;
}
Function* getReadBoolFunction() {
  Function *readBoolFunc = TheModule->getFunction("read_bool");
  if (!readBoolFunc) {
    FunctionType* funcType = FunctionType::get(Type::getInt1Ty(Context), false);
    readBoolFunc = Function::Create(funcType, Function::ExternalLinkage, "read_bool", TheModule);
  }
  return readBoolFunc;
}
Function* getReadCharFunction() {
  Function *readCharFunc = TheModule->getFunction("read_char");
  if (!readCharFunc) {
    FunctionType* funcType = FunctionType::get(Type::getInt8Ty(Context), false);
    readCharFunc = Function::Create(funcType, Function::ExternalLinkage, "read_char", TheModule);
  }
  return readCharFunc;
}
Function* getReadStringFunction() {
  Function *readStrFunc = TheModule->getFunction("read_string");
  if (!readStrFunc) {
    FunctionType* funcType = FunctionType::get(PointerType::get(Type::getInt8Ty(Context), 0), false);
    readStrFunc = Function::Create(funcType, Function::ExternalLinkage, "read_string", TheModule);
  }
  return readStrFunc;
}

// Generate LLVM IR from AST.
Value *generateIR(ASTNode *node, Function* currentFunction) {
  if (!node) return nullptr;
  
  // --- Literals ---
  if (strcmp(node->type, "NUMBER") == 0)
    return ConstantInt::get(Type::getInt32Ty(Context), atoi(node->value));
  
  if (strcmp(node->type, "FLOAT") == 0)
    return ConstantFP::get(Type::getFloatTy(Context), strtof(node->value, nullptr));
  
  if (strcmp(node->type, "BOOLEAN") == 0)
    return ConstantInt::get(Type::getInt1Ty(Context), (strcmp(node->value, "true") == 0) ? 1 : 0);
  
  if (strcmp(node->type, "CHAR") == 0) {
    if (strlen(node->value) < 3) {
      std::cerr << "Invalid char literal: " << node->value << "\n";
      return ConstantInt::get(Type::getInt8Ty(Context), 0);
    }
    return ConstantInt::get(Type::getInt8Ty(Context), node->value[1]);
  }
  
  if (strcmp(node->type, "STRING") == 0) {
    std::string strLiteral(node->value);
    if (!strLiteral.empty() && strLiteral.front() == '"' && strLiteral.back() == '"')
      strLiteral = strLiteral.substr(1, strLiteral.size() - 2);
    return Builder.CreateGlobalStringPtr(strLiteral, "strlit");
  }
  
  // --- Identifier lookup ---
  if (strcmp(node->type, "IDENTIFIER") == 0) {
    Value* varPtr = NamedValues[node->value];
    if (!varPtr) {
      report_fatal_error(Twine("Error: Unknown variable '") + node->value + "'");
    }
    if (AllocaInst *alloca = dyn_cast<AllocaInst>(varPtr))
      return Builder.CreateLoad(alloca->getAllocatedType(), varPtr, node->value);
    else {
      PointerType *ptrType = dyn_cast<PointerType>(varPtr->getType());
      if (!ptrType || ptrType->getNumContainedTypes() < 1)
        report_fatal_error(Twine("Error: Variable ") + node->value + " is not a proper pointer type!");
      return Builder.CreateLoad(ptrType->getContainedType(0), varPtr, node->value);
    }
  }
  
  // --- Unary minus ---
  if (strcmp(node->type, "NEG") == 0) {
    Value *val = generateIR(node->left, currentFunction);
    if (val->getType()->isFloatTy())
      return Builder.CreateFNeg(val, "fnegtmp");
    else
      return Builder.CreateNeg(val, "negtmp");
  }
  
  // --- Binary Arithmetic Operations ---
  if (strcmp(node->type, "ADD") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    // String concatenation case.
    if (L->getType() == PointerType::get(Type::getInt8Ty(Context), 0) &&
        R->getType() == PointerType::get(Type::getInt8Ty(Context), 0)) {
      Function *concatFunc = getConcatFunction();
      return Builder.CreateCall(concatFunc, {L, R}, "concat");
    }
    if (L->getType()->isFloatTy() && R->getType()->isIntegerTy())
      R = Builder.CreateSIToFP(R, Type::getFloatTy(Context), "intToFloat");
    else if (L->getType()->isIntegerTy() && R->getType()->isFloatTy())
      L = Builder.CreateSIToFP(L, Type::getFloatTy(Context), "intToFloat");
    
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFAdd(L, R, "faddtmp");
    return Builder.CreateAdd(L, R, "addtmp");
  }
  
  if (strcmp(node->type, "SUB") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFSub(L, R, "fsubtmp");
    return Builder.CreateSub(L, R, "subtmp");
  }
  
  if (strcmp(node->type, "MUL") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFMul(L, R, "fmultmp");
    return Builder.CreateMul(L, R, "multmp");
  }
  
  if (strcmp(node->type, "DIV") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFDiv(L, R, "fdivtmp");
    return Builder.CreateSDiv(L, R, "divtmp");
  }
  
  // --- Relational Operators ---
  if (strcmp(node->type, "LT") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOLT(L, R, "cmptmp");
    else
      return Builder.CreateICmpSLT(L, R, "cmptmp");
  }
  
  if (strcmp(node->type, "GT") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOGT(L, R, "cmptmp");
    else
      return Builder.CreateICmpSGT(L, R, "cmptmp");
  }
  
  if (strcmp(node->type, "LE") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOLE(L, R, "cmptmp");
    else
      return Builder.CreateICmpSLE(L, R, "cmptmp");
  }
  
  if (strcmp(node->type, "GE") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOGE(L, R, "cmptmp");
    else
      return Builder.CreateICmpSGE(L, R, "cmptmp");
  }
  
  if (strcmp(node->type, "EQ") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
      return Builder.CreateFCmpOEQ(L, R, "eqtmp");
    else
      return Builder.CreateICmpEQ(L, R, "eqtmp");
  }
  
  // --- Logical Operators ---
  if (strcmp(node->type, "AND") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    return Builder.CreateAnd(L, R, "andtmp");
  }
  
  if (strcmp(node->type, "OR") == 0) {
    Value *L = generateIR(node->left, currentFunction);
    Value *R = generateIR(node->right, currentFunction);
    return Builder.CreateOr(L, R, "ortmp");
  }

  // --- Not Equals operator ---
if (strcmp(node->type, "NE") == 0) {
  Value *L = generateIR(node->left, currentFunction);
  Value *R = generateIR(node->right, currentFunction);
  if (L->getType()->isFloatTy() && R->getType()->isFloatTy())
    return Builder.CreateFCmpONE(L, R, "netmp");
  else
    return Builder.CreateICmpNE(L, R, "netmp");
}

// --- Unary NOT operator ---
if (strcmp(node->type, "NOT") == 0) {
  Value *val = generateIR(node->left, currentFunction);
  // If already a boolean (i1) use the LLVM not operator.
  if (val->getType()->isIntegerTy() && val->getType()->getIntegerBitWidth() == 1)
    return Builder.CreateNot(val, "nottmp");
  // Otherwise, compare against zero.
  return Builder.CreateICmpEQ(val, ConstantInt::get(val->getType(), 0), "nottmp");
}

  
  // --- Assignments ---
  if (strcmp(node->type, "ASSIGN_INT") == 0 ||
      strcmp(node->type, "ASSIGN_FLOAT") == 0 ||
      strcmp(node->type, "ASSIGN_BOOL") == 0 ||
      strcmp(node->type, "ASSIGN_CHAR") == 0 ||
      strcmp(node->type, "ASSIGN_STRING") == 0) {
    std::string varName = node->value;
    Value *exprVal = generateIR(node->left, currentFunction);
    Value *varPtr = NamedValues[varName];
    if (!varPtr) {
      if (strcmp(node->type, "ASSIGN_INT") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt32Ty(Context));
      else if (strcmp(node->type, "ASSIGN_FLOAT") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getFloatTy(Context));
      else if (strcmp(node->type, "ASSIGN_BOOL") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt1Ty(Context));
      else if (strcmp(node->type, "ASSIGN_CHAR") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt8Ty(Context));
      else if (strcmp(node->type, "ASSIGN_STRING") == 0)
        varPtr = CreateEntryBlockAlloca(currentFunction, varName, PointerType::get(Type::getInt8Ty(Context), 0));
      NamedValues[varName] = varPtr;
    }
    if (strcmp(node->type, "ASSIGN_FLOAT") == 0) {
      if (exprVal->getType()->isIntegerTy())
        exprVal = Builder.CreateSIToFP(exprVal, Type::getFloatTy(Context), "intToFloat");
    }
    if (strcmp(node->type, "ASSIGN_STRING") == 0) {
      Value *strVal = Builder.CreateBitCast(exprVal, PointerType::get(Type::getInt8Ty(Context), 0), "strcast");
      Builder.CreateStore(strVal, varPtr);
      return strVal;
    }
    Builder.CreateStore(exprVal, varPtr);
    return exprVal;
  }
  
  if (strcmp(node->type, "REASSIGN") == 0) {
    std::string varName = node->value;
    Value *varPtr = NamedValues[varName];
    if (!varPtr) {
      report_fatal_error(Twine("Error: Undeclared variable '") + varName + "'");
    }
    Value *exprVal = generateIR(node->left, currentFunction);
    if (AllocaInst *AI = dyn_cast<AllocaInst>(varPtr)) {
      if (AI->getAllocatedType()->isFloatTy() && exprVal->getType()->isIntegerTy())
        exprVal = Builder.CreateSIToFP(exprVal, Type::getFloatTy(Context), "intToFloat");
    } else if (PointerType *PT = dyn_cast<PointerType>(varPtr->getType())) {
      if (PT->getContainedType(0)->isFloatTy() && exprVal->getType()->isIntegerTy())
        exprVal = Builder.CreateSIToFP(exprVal, Type::getFloatTy(Context), "intToFloat");
    }
    Builder.CreateStore(exprVal, varPtr);
    return exprVal;
  }
  
  // --- PRINT ---
  if (strcmp(node->type, "PRINT") == 0) {
    Value *exprVal = generateIR(node->left, currentFunction);
    Type *exprType = exprVal->getType();
    GlobalVariable *fmtStrVar = nullptr;
    if (exprType->isIntegerTy(1)) {
      GlobalVariable *trueStr = TheModule->getNamedGlobal(".str_true");
      if (!trueStr) {
        Constant *tstr = ConstantDataArray::getString(Context, "true", true);
        trueStr = new GlobalVariable(*TheModule, tstr->getType(), true,
                                     GlobalValue::PrivateLinkage, tstr, ".str_true");
      }
      GlobalVariable *falseStr = TheModule->getNamedGlobal(".str_false");
      if (!falseStr) {
        Constant *fstr = ConstantDataArray::getString(Context, "false", true);
        falseStr = new GlobalVariable(*TheModule, fstr->getType(), true,
                                      GlobalValue::PrivateLinkage, fstr, ".str_false");
      }
      Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
      std::vector<Constant*> indices = {zero, zero};
      Constant *trueStrPtr = ConstantExpr::getGetElementPtr(trueStr->getValueType(), trueStr, indices);
      Constant *falseStrPtr = ConstantExpr::getGetElementPtr(falseStr->getValueType(), falseStr, indices);
      Value *boolStr = Builder.CreateSelect(exprVal, trueStrPtr, falseStrPtr, "boolstr");
      GlobalVariable *fmtStrVarBool = TheModule->getNamedGlobal(".str_bool");
      if (!fmtStrVarBool) {
        Constant *formatStr = ConstantDataArray::getString(Context, "%s\n", true);
        fmtStrVarBool = new GlobalVariable(*TheModule, formatStr->getType(), true,
                                           GlobalValue::PrivateLinkage, formatStr, ".str_bool");
      }
      fmtStrVar = fmtStrVarBool;
      exprVal = boolStr;
    } else if (exprType->isFloatTy()) {
      fmtStrVar = getFormatStringFloat();
      exprVal = Builder.CreateFPExt(exprVal, Type::getDoubleTy(Context), "toDouble");
    } else if (exprType->isIntegerTy(8)) {
      fmtStrVar = getFormatStringChar();
    } else if (exprType->isPointerTy()) {
      if (exprType == PointerType::get(Type::getInt8Ty(Context), 0))
        fmtStrVar = getFormatStringStr();
      else
        fmtStrVar = getFormatStringInt();
    } else {
      fmtStrVar = getFormatStringInt();
    }
    Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
    std::vector<Constant*> indices = {zero, zero};
    Constant *fmtStrPtr = ConstantExpr::getGetElementPtr(fmtStrVar->getValueType(), fmtStrVar, indices);
    Builder.CreateCall(getPrintfFunction(), {fmtStrPtr, exprVal});
    return exprVal;
  }
  
  // --- INPUT (new functionality from second file) ---
  if (strcmp(node->type, "INPUT") == 0) {
    std::string varName = node->value;
    Value *varPtr = NamedValues[varName];
    // If not declared, auto-create as int.
    if (!varPtr) {
      varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt32Ty(Context));
      NamedValues[varName] = varPtr;
    }
    if (AllocaInst *allocaInst = dyn_cast<AllocaInst>(varPtr)) {
      Type *allocatedType = allocaInst->getAllocatedType();
      Value *inputVal = nullptr;
      if (allocatedType == Type::getInt32Ty(Context)) {
        inputVal = Builder.CreateCall(getReadIntFunction(), {}, "readInt");
      } else if (allocatedType == Type::getFloatTy(Context)) {
        inputVal = Builder.CreateCall(getReadFloatFunction(), {}, "readFloat");
      } else if (allocatedType == Type::getInt1Ty(Context)) {
        inputVal = Builder.CreateCall(getReadBoolFunction(), {}, "readBool");
      } else if (allocatedType == Type::getInt8Ty(Context)) {
        inputVal = Builder.CreateCall(getReadCharFunction(), {}, "readChar");
      } else if (allocatedType->isPointerTy() &&
                 allocatedType == PointerType::get(Type::getInt8Ty(Context), 0)) {
        inputVal = Builder.CreateCall(getReadStringFunction(), {}, "readStr");
      } else {
        GlobalVariable *fmtStrVar = getFormatStringStr();
        Constant *errMsg = Builder.CreateGlobalStringPtr("Input Is not of the expected type\n", "errMsg");
        Constant *zero = ConstantInt::get(Type::getInt32Ty(Context), 0);
        std::vector<Constant*> indices = {zero, zero};
        Constant *fmtStrPtr = ConstantExpr::getGetElementPtr(fmtStrVar->getValueType(), fmtStrVar, indices);
        Builder.CreateCall(getPrintfFunction(), {fmtStrPtr, errMsg});
        inputVal = ConstantInt::get(Type::getInt32Ty(Context), 0);
      }
      Builder.CreateStore(inputVal, varPtr);
      return inputVal;
    }
  }
  
  // --- LOOP ---
  if (strcmp(node->type, "LOOP") == 0) {
    Value *loopCountVal = generateIR(node->left, currentFunction);
    if (!loopCountVal) {
      std::cerr << "Error: Invalid loop count expression\n";
      return nullptr;
    }
    if (loopCountVal->getType() != Type::getInt32Ty(Context))
      loopCountVal = Builder.CreateIntCast(loopCountVal, Type::getInt32Ty(Context), true, "loopcount");
    
    AllocaInst *loopVar = CreateEntryBlockAlloca(currentFunction, "i", Type::getInt32Ty(Context));
    Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(Context), 0), loopVar);
    BasicBlock *loopCondBB = BasicBlock::Create(Context, "loopcond", currentFunction);
    BasicBlock *loopBodyBB = BasicBlock::Create(Context, "loopbody", currentFunction);
    BasicBlock *afterLoopBB = BasicBlock::Create(Context, "afterloop", currentFunction);
    Builder.CreateBr(loopCondBB);
    Builder.SetInsertPoint(loopCondBB);
    Value *currVal = Builder.CreateLoad(Type::getInt32Ty(Context), loopVar, "i");
    Value *cond = Builder.CreateICmpSLT(currVal, loopCountVal, "loopcond");
    Builder.CreateCondBr(cond, loopBodyBB, afterLoopBB);
    Builder.SetInsertPoint(loopBodyBB);
    generateIR(node->right, currentFunction);
    Value *nextVal = Builder.CreateAdd(currVal, ConstantInt::get(Type::getInt32Ty(Context), 1), "inc");
    Builder.CreateStore(nextVal, loopVar);
    Builder.CreateBr(loopCondBB);
    Builder.SetInsertPoint(afterLoopBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- LOOP_UNTIL ---
  if (strcmp(node->type, "LOOP_UNTIL") == 0) {
    BasicBlock *condBB = BasicBlock::Create(Context, "until.cond", currentFunction);
    BasicBlock *loopBB = BasicBlock::Create(Context, "until.body", currentFunction);
    BasicBlock *afterBB = BasicBlock::Create(Context, "until.after", currentFunction);
    Builder.CreateBr(condBB);
    Builder.SetInsertPoint(condBB);
    Value *condVal = generateIR(node->left, currentFunction);
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "untilcond");
    Value *notCond = Builder.CreateNot(condVal, "untilnot");
    Builder.CreateCondBr(notCond, loopBB, afterBB);
    Builder.SetInsertPoint(loopBB);
    generateIR(node->right, currentFunction);
    Builder.CreateBr(condBB);
    Builder.SetInsertPoint(afterBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // --- STATEMENT_LIST ---
  if (strcmp(node->type, "STATEMENT_LIST") == 0) {
    if (node->left)
      generateIR(node->left, currentFunction);
    if (node->right)
      return generateIR(node->right, currentFunction);
    return nullptr;
  }
  
  // --- Variable Declaration (VAR_DECL) ---
  if (strcmp(node->type, "VAR_DECL") == 0) {
    std::string varName = node->value;
    if (NamedValues.find(varName) != NamedValues.end()) {
      std::cerr << "Variable " << varName << " already declared!" << std::endl;
      return nullptr;
    }
    Value *exprVal = generateIR(node->left, currentFunction);
    if (!exprVal) {
      std::cerr << "Error evaluating expression for var " << varName << std::endl;
      return nullptr;
    }
    Type *varType = exprVal->getType();
    Value *varPtr = CreateEntryBlockAlloca(currentFunction, varName, varType);
    NamedValues[varName] = varPtr;
    Builder.CreateStore(exprVal, varPtr);
    return exprVal;
  }
  
  // --- TYPE operator ---
  if (strcmp(node->type, "TYPE") == 0) {
    Type *targetType = nullptr;
    if (strcmp(node->left->type, "IDENTIFIER") == 0) {
      Value *varPtr = NamedValues[node->left->value];
      if (!varPtr) {
        std::cerr << "Unknown variable in type(): " << node->left->value << std::endl;
        return Builder.CreateGlobalStringPtr("unknown", "type_unknown");
      }
      if (AllocaInst *alloca = dyn_cast<AllocaInst>(varPtr))
        targetType = alloca->getAllocatedType();
      else
        targetType = varPtr->getType()->getContainedType(0);
    } else {
      Value *temp = generateIR(node->left, currentFunction);
      targetType = temp->getType();
    }
    
    const char *typeName = "unknown";
    if (targetType->isIntegerTy(32))
      typeName = "int";
    else if (targetType->isFloatTy())
      typeName = "float";
    else if (targetType->isIntegerTy(1))
      typeName = "bool";
    else if (targetType->isIntegerTy(8))
      typeName = "char";
    else if (targetType->isPointerTy() &&
             targetType == PointerType::get(Type::getInt8Ty(Context), 0))
      typeName = "string";
    
    Value *typeStr = Builder.CreateGlobalStringPtr(typeName, "typeStr");
    return typeStr;
  }
  
  // --- Specific Declarations ---
  if (strcmp(node->type, "DECL_INT") == 0) {
    std::string varName = node->value;
    if (NamedValues.find(varName) != NamedValues.end()) {
      std::cerr << "Variable " << varName << " already declared!" << std::endl;
      return nullptr;
    }
    Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt32Ty(Context));
    NamedValues[varName] = varPtr;
    Builder.CreateStore(ConstantInt::get(Type::getInt32Ty(Context), 0), varPtr);
    return varPtr;
  }
  if (strcmp(node->type, "DECL_FLOAT") == 0) {
    std::string varName = node->value;
    if (NamedValues.find(varName) != NamedValues.end()) {
      std::cerr << "Variable " << varName << " already declared!" << std::endl;
      return nullptr;
    }
    Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getFloatTy(Context));
    NamedValues[varName] = varPtr;
    Builder.CreateStore(ConstantFP::get(Type::getFloatTy(Context), 0.0), varPtr);
    return varPtr;
  }
  if (strcmp(node->type, "DECL_BOOL") == 0) {
    std::string varName = node->value;
    if (NamedValues.find(varName) != NamedValues.end()) {
      std::cerr << "Variable " << varName << " already declared!" << std::endl;
      return nullptr;
    }
    Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt1Ty(Context));
    NamedValues[varName] = varPtr;
    Builder.CreateStore(ConstantInt::get(Type::getInt1Ty(Context), 0), varPtr);
    return varPtr;
  }
  if (strcmp(node->type, "DECL_CHAR") == 0) {
    std::string varName = node->value;
    if (NamedValues.find(varName) != NamedValues.end()) {
      std::cerr << "Variable " << varName << " already declared!" << std::endl;
      return nullptr;
    }
    Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, Type::getInt8Ty(Context));
    NamedValues[varName] = varPtr;
    Builder.CreateStore(ConstantInt::get(Type::getInt8Ty(Context), 0), varPtr);
    return varPtr;
  }
  if (strcmp(node->type, "DECL_STRING") == 0) {
    std::string varName = node->value;
    if (NamedValues.find(varName) != NamedValues.end()) {
      std::cerr << "Variable " << varName << " already declared!" << std::endl;
      return nullptr;
    }
    Value* varPtr = CreateEntryBlockAlloca(currentFunction, varName, PointerType::get(Type::getInt8Ty(Context), 0));
    NamedValues[varName] = varPtr;
    Builder.CreateStore(ConstantPointerNull::get(PointerType::get(Type::getInt8Ty(Context), 0)), varPtr);
    return varPtr;
  }
  
  // --- Conditional Constructs ---
  // Plain IF statement.
  if (strcmp(node->type, "IF") == 0) {
    Value *condVal = generateIR(node->left, currentFunction);
    if (!condVal) {
      std::cerr << "Error: Invalid condition in if statement\n";
      return nullptr;
    }
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "ifcond");
    BasicBlock *thenBB = BasicBlock::Create(Context, "then", currentFunction);
    BasicBlock *mergeBB = BasicBlock::Create(Context, "ifcont", currentFunction);
    Builder.CreateCondBr(condVal, thenBB, mergeBB);
    Builder.SetInsertPoint(thenBB);
    generateIR(node->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // IF-ELSE statement.
  if (strcmp(node->type, "IF_ELSE") == 0) {
    Value *condVal = generateIR(node->left, currentFunction);
    if (!condVal) {
      std::cerr << "Error: Invalid condition in if-else statement\n";
      return nullptr;
    }
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "ifelsecond");
    BasicBlock *thenBB = BasicBlock::Create(Context, "then", currentFunction);
    BasicBlock *elseBB = BasicBlock::Create(Context, "else", currentFunction);
    BasicBlock *mergeBB = BasicBlock::Create(Context, "ifelsecont", currentFunction);
    Builder.CreateCondBr(condVal, thenBB, elseBB);
    Builder.SetInsertPoint(thenBB);
    // Here, node->right->left represents the "then" branch.
    generateIR(node->right->left, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(elseBB);
    // And node->right->right represents the "else" branch.
    generateIR(node->right->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // Additional conditional constructs (from first file):
  // IF_CHAIN: if with one or more else-if / else parts.
  if (strcmp(node->type, "IF_CHAIN") == 0) {
    // Assume node->left is an "IF" node and node->right is the chain.
    Value *condVal = generateIR(node->left->left, currentFunction);
    if (!condVal) {
      std::cerr << "Error: Invalid condition in if-chain\n";
      return nullptr;
    }
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "ifchaincond");
    BasicBlock *thenBB = BasicBlock::Create(Context, "if.chain.then", currentFunction);
    BasicBlock *elseBB = BasicBlock::Create(Context, "if.chain.else", currentFunction);
    BasicBlock *mergeBB = BasicBlock::Create(Context, "if.chain.merge", currentFunction);
    Builder.CreateCondBr(condVal, thenBB, elseBB);
    Builder.SetInsertPoint(thenBB);
    generateIR(node->left->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(elseBB);
    generateIR(node->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // ELSE_IF node.
  if (strcmp(node->type, "ELSE_IF") == 0) {
    Value *condVal = generateIR(node->left, currentFunction);
    if (!condVal) {
      std::cerr << "Error: Invalid condition in else-if statement\n";
      return nullptr;
    }
    if (condVal->getType()->isIntegerTy() && condVal->getType()->getIntegerBitWidth() != 1)
      condVal = Builder.CreateICmpNE(condVal, ConstantInt::get(condVal->getType(), 0), "elseifcond");
    BasicBlock *thenBB = BasicBlock::Create(Context, "elseif.then", currentFunction);
    BasicBlock *elseBB = BasicBlock::Create(Context, "elseif.else", currentFunction);
    BasicBlock *mergeBB = BasicBlock::Create(Context, "elseif.merge", currentFunction);
    Builder.CreateCondBr(condVal, thenBB, elseBB);
    Builder.SetInsertPoint(thenBB);
    // In this design, node->right->left is the "then" branch.
    generateIR(node->right->left, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(elseBB);
    if (node->right->right)
      generateIR(node->right->right, currentFunction);
    Builder.CreateBr(mergeBB);
    Builder.SetInsertPoint(mergeBB);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // ELSE block.
  if (strcmp(node->type, "ELSE") == 0) {
    generateIR(node->left, currentFunction);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  // IF_ELSE_BODY node.
  if (strcmp(node->type, "IF_ELSE_BODY") == 0) {
    if (node->left)
      generateIR(node->left, currentFunction);
    if (node->right)
      generateIR(node->right, currentFunction);
    return ConstantInt::get(Type::getInt32Ty(Context), 0);
  }
  
  return nullptr;
}

int main() {
  if (yyparse() != 0) {
    return 1;
  }
  
  FunctionType *funcType = FunctionType::get(Type::getInt32Ty(Context), false);
  Function *mainFunc = Function::Create(funcType, Function::ExternalLinkage, "main", TheModule);
  BasicBlock *entryBB = BasicBlock::Create(Context, "entry", mainFunc);
  Builder.SetInsertPoint(entryBB);
  
  generateIR(root, mainFunc);
  
  Builder.CreateRet(ConstantInt::get(Type::getInt32Ty(Context), 0));
  std::string error;
  raw_string_ostream errorStream(error);
  if (verifyModule(*TheModule, &errorStream)) {
    std::cerr << "Error: " << errorStream.str() << "\n";
    return 1;
  }
  TheModule->print(outs(), nullptr);
  delete TheModule;
  return 0;
}
