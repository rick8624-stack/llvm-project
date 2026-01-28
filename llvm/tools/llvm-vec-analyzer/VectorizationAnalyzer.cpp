//===-- VectorizationAnalyzer.cpp - Vectorization Opportunity Analyzer ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "VectorizationAnalyzer.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"
#include <map>

using namespace llvm;

VectorizationAnalyzer::VectorizationAnalyzer(Module &M, unsigned VLen,
                                             unsigned LMul,
                                             const std::string &OutDir,
                                             bool Chinese, bool Verbose)
    : Mod(M), VLEN(VLen), LMUL(LMul), OutputDir(OutDir),
      EnableChinese(Chinese), VerboseMode(Verbose) {}

bool VectorizationAnalyzer::analyze() {
  if (VerboseMode)
    outs() << "Starting analysis...\n";

  // Analyze each function in the module
  for (Function &F : Mod) {
    if (F.isDeclaration())
      continue;

    analyzeFunction(F);
  }

  if (VerboseMode) {
    outs() << "Analysis complete:\n";
    outs() << "  Found " << Opportunities.size() << " vectorization opportunities\n";
    outs() << "  Found " << Issues.size() << " dependency issues\n";
  }

  return true;
}

void VectorizationAnalyzer::analyzeFunction(Function &F) {
  if (VerboseMode)
    outs() << "Analyzing function: " << F.getName() << "\n";

  // Build dominator tree
  auto &DT = FunctionDT[&F];
  DT = std::make_unique<DominatorTree>(F);

  // Build loop info
  auto &LI = FunctionLoops[&F];
  LI = std::make_unique<LoopInfo>(*DT);

  // Build target library info and assumption cache with proper lifetime
  auto &TLI = FunctionTLI[&F];
  TargetLibraryInfoImpl TLII(Triple(F.getParent()->getTargetTriple()));
  TLI = std::make_unique<TargetLibraryInfo>(TLII, &F);
  
  auto &AC = FunctionAC[&F];
  AC = std::make_unique<AssumptionCache>(F);

  // Build scalar evolution
  auto &SE = FunctionSCEV[&F];
  SE = std::make_unique<ScalarEvolution>(F, *TLI, *AC, *DT, *LI);

  // Analyze each loop
  for (Loop *L : LI->getLoopsInPreorder()) {
    analyzeLoop(F, L, *LI, *SE);
    checkDependencies(F, L, *SE);
    recognizePatterns(F, L, *SE);
  }
}

void VectorizationAnalyzer::analyzeLoop(Function &F, Loop *L, LoopInfo &LI,
                                        ScalarEvolution &SE) {
  // Get the dominator tree for this function
  DominatorTree *DT = nullptr;
  auto it = FunctionDT.find(&F);
  if (it != FunctionDT.end()) {
    DT = it->second.get();
  }

  // Check if loop is vectorizable
  BasicBlock *Header = L->getHeader();
  if (!Header)
    return;

  // Check for simple loop structure
  bool isSimple = L->isLoopSimplifyForm() && (DT ? L->isLCSSAForm(*DT) : false);

  if (isSimple) {
    VectorizationOpportunity Opp(VectorizationOpportunity::SimpleLoop, &F, L);
    Opp.Description = "Simple loop structure suitable for vectorization";
    Opp.ConfidenceScore = 0.8;
    // Check trip count
    if (SE.hasLoopInvariantBackedgeTakenCount(L)) {
      Opp.Reasons.push_back("Loop has invariant trip count");
      Opp.ConfidenceScore += 0.1;
    }

    // Estimate potential speedup based on VLEN
    unsigned VF = VLEN / 32; // Assume 32-bit elements
    Opp.EstimatedSpeedup = (VF > 1) ? ((VF - 1) * 100 / VF) : 0;

    Opportunities.push_back(Opp);
  }

  // Check for nested loops
  if (!L->getSubLoops().empty()) {
    VectorizationOpportunity Opp(VectorizationOpportunity::NestedLoop, &F, L);
    Opp.Description = "Nested loop structure - consider outer loop vectorization";
    Opp.ConfidenceScore = 0.6;
    Opp.Reasons.push_back("Nested loop depth: " + std::to_string(L->getLoopDepth()));
    Opportunities.push_back(Opp);
  }

  // Check for regular memory access patterns (only once per loop)
  bool hasMemoryOps = false;
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      if (isa<LoadInst>(I) || isa<StoreInst>(I)) {
        hasMemoryOps = true;
        break;
      }
    }
    if (hasMemoryOps)
      break;
  }
  
  if (hasMemoryOps) {
    VectorizationOpportunity Opp(VectorizationOpportunity::MemoryAccess,
                                 &F, L);
    Opp.Description = "Regular memory access pattern detected";
    Opp.ConfidenceScore = 0.7;
    Opp.Reasons.push_back("Memory operations can benefit from vector loads/stores");
    Opportunities.push_back(Opp);
  }
}

void VectorizationAnalyzer::checkDependencies(Function &F, Loop *L,
                                              ScalarEvolution &SE) {
  // Check for complex control flow
  unsigned NumBlocks = L->getNumBlocks();
  if (NumBlocks > 5) {
    DependencyIssue Issue(DependencyIssue::ComplexControlFlow, &F, L);
    Issue.Description = "Loop has complex control flow";
    Issue.Details.push_back("Number of basic blocks: " + std::to_string(NumBlocks));
    Issue.Details.push_back("May require predication or loop versioning");
    Issues.push_back(Issue);
  }

  // Check for function calls (only report once per loop)
  bool hasFunctionCall = false;
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      if (auto *CI = dyn_cast<CallInst>(&I)) {
        Function *Callee = CI->getCalledFunction();
        if (!Callee || !Callee->isIntrinsic()) {
          hasFunctionCall = true;
          break;
        }
      }
    }
    if (hasFunctionCall)
      break;
  }
  
  if (hasFunctionCall) {
    DependencyIssue Issue(DependencyIssue::CallToUnknownFunction, &F, L);
    Issue.Description = "Loop contains function call";
    Issue.Details.push_back("Vectorization may require function cloning or inlining");
    Issues.push_back(Issue);
  }
}

void VectorizationAnalyzer::recognizePatterns(Function &F, Loop *L,
                                              ScalarEvolution &SE) {
  // Look for reduction patterns - check for proper reduction operations
  bool foundReduction = false;
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      if (auto *Phi = dyn_cast<PHINode>(&I)) {
        // Check if this is a reduction variable
        for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
          Value *Inc = Phi->getIncomingValue(i);
          if (auto *BinOp = dyn_cast<BinaryOperator>(Inc)) {
            // Check if this is an associative and commutative operation
            unsigned Opcode = BinOp->getOpcode();
            bool isReductionOp = (Opcode == Instruction::Add ||
                                   Opcode == Instruction::FAdd ||
                                   Opcode == Instruction::Mul ||
                                   Opcode == Instruction::FMul ||
                                   Opcode == Instruction::And ||
                                   Opcode == Instruction::Or ||
                                   Opcode == Instruction::Xor);
            
            if (isReductionOp && 
                (BinOp->getOperand(0) == Phi || BinOp->getOperand(1) == Phi)) {
              foundReduction = true;
              break;
            }
          }
        }
        
        if (foundReduction) {
          VectorizationOpportunity Opp(VectorizationOpportunity::Reduction,
                                       &F, L);
          Opp.Description = "Reduction pattern detected";
          Opp.ConfidenceScore = 0.85;
          Opp.Reasons.push_back("Can use RISC-V vector reduction instructions");
          Opp.EstimatedSpeedup = 60;
          Opportunities.push_back(Opp);
          break;
        }
      }
    }
    if (foundReduction)
      break;
  }

  // Look for induction variables - check for AddRec SCEV expressions
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      if (auto *Phi = dyn_cast<PHINode>(&I)) {
        if (SE.isSCEVable(Phi->getType())) {
          const SCEV *S = SE.getSCEV(Phi);
          // Induction variables have AddRec SCEV expressions
          if (isa<SCEVAddRecExpr>(S)) {
            VectorizationOpportunity Opp(
                VectorizationOpportunity::InductionVariable, &F, L);
            Opp.Description = "Induction variable pattern";
            Opp.ConfidenceScore = 0.9;
            Opp.Reasons.push_back("Can use vector index generation");
            Opportunities.push_back(Opp);
            break;
          }
        }
      }
    }
  }
}

double VectorizationAnalyzer::calculateConfidence(
    const VectorizationOpportunity &Opp) {
  return Opp.ConfidenceScore;
}

bool VectorizationAnalyzer::generateReports() {
  std::error_code EC;

  // Generate English report
  std::string EnglishPath = OutputDir + "/analysis_report_en.txt";
  raw_fd_ostream EnglishOS(EnglishPath, EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "Error opening English report file: " << EC.message() << "\n";
    return false;
  }
  generateEnglishReport(EnglishOS);
  EnglishOS.close();

  // Generate Chinese report if enabled
  std::string ChinesePath = OutputDir + "/analysis_report_zh.txt";
  if (EnableChinese) {
    raw_fd_ostream ChineseOS(ChinesePath, EC, sys::fs::OF_Text);
    if (EC) {
      errs() << "Error opening Chinese report file: " << EC.message() << "\n";
      return false;
    }
    generateChineseReport(ChineseOS);
    ChineseOS.close();
  }

  // Generate JSON report
  std::string JSONPath = OutputDir + "/analysis_report.json";
  raw_fd_ostream JSONOS(JSONPath, EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "Error opening JSON report file: " << EC.message() << "\n";
    return false;
  }
  generateJSONReport(JSONOS);
  JSONOS.close();

  if (VerboseMode) {
    outs() << "Generated reports:\n";
    outs() << "  English: " << EnglishPath << "\n";
    if (EnableChinese)
      outs() << "  Chinese: " << ChinesePath << "\n";
    outs() << "  JSON: " << JSONPath << "\n";
  }

  return true;
}

void VectorizationAnalyzer::generateEnglishReport(raw_ostream &OS) {
  OS << "========================================\n";
  OS << "LLVM Vectorization Opportunity Analysis\n";
  OS << "========================================\n\n";

  OS << "Target Architecture: RISC-V\n";
  OS << "Vector Length (VLEN): " << VLEN << " bits\n";
  OS << "LMUL: " << LMUL << "\n";
  OS << "Module: " << Mod.getName() << "\n\n";

  OS << "========================================\n";
  OS << "VECTORIZATION OPPORTUNITIES\n";
  OS << "========================================\n\n";

  if (Opportunities.empty()) {
    OS << "No vectorization opportunities found.\n\n";
  } else {
    OS << "Total opportunities found: " << Opportunities.size() << "\n\n";

    unsigned OpNum = 1;
    for (const auto &Opp : Opportunities) {
      OS << "Opportunity #" << OpNum++ << ":\n";
      OS << "  Type: " << getOpportunityKindName(Opp.OpKind) << "\n";
      OS << "  Function: " << Opp.Func->getName() << "\n";
      OS << "  Description: " << Opp.Description << "\n";
      OS << "  Confidence Score: " << format("%.2f", Opp.ConfidenceScore) << "\n";
      OS << "  Estimated Speedup: " << Opp.EstimatedSpeedup << "%\n";

      if (!Opp.Reasons.empty()) {
        OS << "  Reasons:\n";
        for (const auto &Reason : Opp.Reasons) {
          OS << "    - " << Reason << "\n";
        }
      }
      OS << "\n";
    }
  }

  OS << "========================================\n";
  OS << "DEPENDENCY ISSUES\n";
  OS << "========================================\n\n";

  if (Issues.empty()) {
    OS << "No dependency issues found.\n\n";
  } else {
    OS << "Total issues found: " << Issues.size() << "\n\n";

    unsigned IssueNum = 1;
    for (const auto &Issue : Issues) {
      OS << "Issue #" << IssueNum++ << ":\n";
      OS << "  Type: " << getIssueKindName(Issue.Kind) << "\n";
      OS << "  Function: " << Issue.Func->getName() << "\n";
      OS << "  Description: " << Issue.Description << "\n";

      if (!Issue.Details.empty()) {
        OS << "  Details:\n";
        for (const auto &Detail : Issue.Details) {
          OS << "    - " << Detail << "\n";
        }
      }
      OS << "\n";
    }
  }

  OS << "========================================\n";
  OS << "INTERACTION POINTS\n";
  OS << "========================================\n\n";

  OS << "Code Transformation Agent:\n";
  OS << "  - " << Opportunities.size() << " regions ready for transformation\n\n";

  OS << "Validation Agent:\n";
  OS << "  - Baseline metrics available for comparison\n\n";

  OS << "Performance Analysis Agent:\n";
  OS << "  - Pre-optimization metrics ready\n\n";
}

void VectorizationAnalyzer::generateChineseReport(raw_ostream &OS) {
  OS << "========================================\n";
  OS << "LLVM 向量化机会分析报告\n";
  OS << "========================================\n\n";

  OS << "目标架构: RISC-V\n";
  OS << "向量长度 (VLEN): " << VLEN << " 位\n";
  OS << "LMUL: " << LMUL << "\n";
  OS << "模块: " << Mod.getName() << "\n\n";

  OS << "========================================\n";
  OS << "向量化机会\n";
  OS << "========================================\n\n";

  if (Opportunities.empty()) {
    OS << "未发现向量化机会。\n\n";
  } else {
    OS << "发现的机会总数: " << Opportunities.size() << "\n\n";

    unsigned OpNum = 1;
    for (const auto &Opp : Opportunities) {
      OS << "机会 #" << OpNum++ << ":\n";
      OS << "  类型: " << translateToChinese(getOpportunityKindName(Opp.OpKind)) << "\n";
      OS << "  函数: " << Opp.Func->getName() << "\n";
      OS << "  描述: " << translateToChinese(Opp.Description) << "\n";
      OS << "  置信度分数: " << format("%.2f", Opp.ConfidenceScore) << "\n";
      OS << "  预计加速: " << Opp.EstimatedSpeedup << "%\n";

      if (!Opp.Reasons.empty()) {
        OS << "  原因:\n";
        for (const auto &Reason : Opp.Reasons) {
          OS << "    - " << translateToChinese(Reason) << "\n";
        }
      }
      OS << "\n";
    }
  }

  OS << "========================================\n";
  OS << "依赖问题\n";
  OS << "========================================\n\n";

  if (Issues.empty()) {
    OS << "未发现依赖问题。\n\n";
  } else {
    OS << "发现的问题总数: " << Issues.size() << "\n\n";

    unsigned IssueNum = 1;
    for (const auto &Issue : Issues) {
      OS << "问题 #" << IssueNum++ << ":\n";
      OS << "  类型: " << translateToChinese(getIssueKindName(Issue.Kind)) << "\n";
      OS << "  函数: " << Issue.Func->getName() << "\n";
      OS << "  描述: " << translateToChinese(Issue.Description) << "\n";

      if (!Issue.Details.empty()) {
        OS << "  详细信息:\n";
        for (const auto &Detail : Issue.Details) {
          OS << "    - " << translateToChinese(Detail) << "\n";
        }
      }
      OS << "\n";
    }
  }

  OS << "========================================\n";
  OS << "交互点\n";
  OS << "========================================\n\n";

  OS << "代码转换代理:\n";
  OS << "  - " << Opportunities.size() << " 个区域准备进行转换\n\n";

  OS << "验证代理:\n";
  OS << "  - 基线指标可用于比较\n\n";

  OS << "性能分析代理:\n";
  OS << "  - 预优化指标已准备就绪\n\n";
}

void VectorizationAnalyzer::generateJSONReport(raw_ostream &OS) {
  json::Object Root;

  // Metadata
  json::Object Metadata;
  Metadata["architecture"] = "RISC-V";
  Metadata["vlen"] = VLEN;
  Metadata["lmul"] = LMUL;
  Metadata["module"] = Mod.getName().str();
  Root["metadata"] = std::move(Metadata);

  // Opportunities
  json::Array OppsArray;
  for (const auto &Opp : Opportunities) {
    json::Object OppObj;
    OppObj["type"] = getOpportunityKindName(Opp.OpKind);
    OppObj["function"] = Opp.Func->getName().str();
    OppObj["description"] = Opp.Description;
    OppObj["confidence_score"] = Opp.ConfidenceScore;
    OppObj["estimated_speedup_percent"] = Opp.EstimatedSpeedup;

    json::Array ReasonsArray;
    for (const auto &Reason : Opp.Reasons) {
      ReasonsArray.push_back(Reason);
    }
    OppObj["reasons"] = std::move(ReasonsArray);

    OppsArray.push_back(std::move(OppObj));
  }
  Root["opportunities"] = std::move(OppsArray);

  // Issues
  json::Array IssuesArray;
  for (const auto &Issue : Issues) {
    json::Object IssueObj;
    IssueObj["type"] = getIssueKindName(Issue.Kind);
    IssueObj["function"] = Issue.Func->getName().str();
    IssueObj["description"] = Issue.Description;

    json::Array DetailsArray;
    for (const auto &Detail : Issue.Details) {
      DetailsArray.push_back(Detail);
    }
    IssueObj["details"] = std::move(DetailsArray);

    IssuesArray.push_back(std::move(IssueObj));
  }
  Root["issues"] = std::move(IssuesArray);

  // Summary
  json::Object Summary;
  Summary["total_opportunities"] = Opportunities.size();
  Summary["total_issues"] = Issues.size();
  Root["summary"] = std::move(Summary);

  OS << json::Value(std::move(Root)) << "\n";
}

std::string VectorizationAnalyzer::translateToChinese(const std::string &Text) {
  // Simple translation map for common terms
  static const std::map<std::string, std::string> TranslationMap = {
      {"Simple loop structure suitable for vectorization", "适合向量化的简单循环结构"},
      {"Nested loop structure - consider outer loop vectorization", "嵌套循环结构 - 考虑外层循环向量化"},
      {"Regular memory access pattern detected", "检测到规则的内存访问模式"},
      {"Reduction pattern detected", "检测到规约模式"},
      {"Induction variable pattern", "归纳变量模式"},
      {"Loop has complex control flow", "循环具有复杂的控制流"},
      {"Loop contains function call", "循环包含函数调用"},
      {"SimpleLoop", "简单循环"},
      {"NestedLoop", "嵌套循环"},
      {"Reduction", "规约"},
      {"Scan", "扫描"},
      {"Gather", "收集"},
      {"Scatter", "分散"},
      {"InductionVariable", "归纳变量"},
      {"MemoryAccess", "内存访问"},
      {"LoopCarriedDependency", "循环携带依赖"},
      {"UnknownPointerAliasing", "未知指针别名"},
      {"ComplexControlFlow", "复杂控制流"},
      {"UnsupportedOperation", "不支持的操作"},
      {"MemoryDependency", "内存依赖"},
      {"CallToUnknownFunction", "调用未知函数"},
      {"Loop has invariant trip count", "循环具有不变的迭代次数"},
      {"Memory operations can benefit from vector loads/stores", "内存操作可以从向量加载/存储中受益"},
      {"Can use RISC-V vector reduction instructions", "可以使用 RISC-V 向量规约指令"},
      {"Can use vector index generation", "可以使用向量索引生成"},
      {"May require predication or loop versioning", "可能需要谓词化或循环版本控制"},
      {"Vectorization may require function cloning or inlining", "向量化可能需要函数克隆或内联"}
  };

  auto it = TranslationMap.find(Text);
  if (it != TranslationMap.end()) {
    return it->second;
  }

  // For dynamic strings, return as-is with original text
  return Text;
}

std::string VectorizationAnalyzer::getOpportunityKindName(
    VectorizationOpportunity::Kind K) {
  switch (K) {
  case VectorizationOpportunity::SimpleLoop:
    return "SimpleLoop";
  case VectorizationOpportunity::NestedLoop:
    return "NestedLoop";
  case VectorizationOpportunity::Reduction:
    return "Reduction";
  case VectorizationOpportunity::Scan:
    return "Scan";
  case VectorizationOpportunity::Gather:
    return "Gather";
  case VectorizationOpportunity::Scatter:
    return "Scatter";
  case VectorizationOpportunity::InductionVariable:
    return "InductionVariable";
  case VectorizationOpportunity::MemoryAccess:
    return "MemoryAccess";
  }
  llvm_unreachable("Unknown opportunity kind");
}

std::string
VectorizationAnalyzer::getIssueKindName(DependencyIssue::IssueKind K) {
  switch (K) {
  case DependencyIssue::LoopCarriedDependency:
    return "LoopCarriedDependency";
  case DependencyIssue::UnknownPointerAliasing:
    return "UnknownPointerAliasing";
  case DependencyIssue::ComplexControlFlow:
    return "ComplexControlFlow";
  case DependencyIssue::UnsupportedOperation:
    return "UnsupportedOperation";
  case DependencyIssue::MemoryDependency:
    return "MemoryDependency";
  case DependencyIssue::CallToUnknownFunction:
    return "CallToUnknownFunction";
  }
  llvm_unreachable("Unknown issue kind");
}
