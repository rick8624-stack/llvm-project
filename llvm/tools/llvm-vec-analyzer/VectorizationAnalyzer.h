//===-- VectorizationAnalyzer.h - Vectorization Opportunity Analyzer ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the VectorizationAnalyzer class, which analyzes LLVM IR
// for vectorization opportunities.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_VEC_ANALYZER_VECTORIZATIONANALYZER_H
#define LLVM_TOOLS_LLVM_VEC_ANALYZER_VECTORIZATIONANALYZER_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include <memory>
#include <string>
#include <vector>

namespace llvm {

/// Represents a vectorization opportunity
struct VectorizationOpportunity {
  enum Kind {
    SimpleLoop,         // Simple countable loop
    NestedLoop,         // Nested loop structure
    Reduction,          // Reduction pattern
    Scan,               // Scan/prefix-sum pattern
    Gather,             // Gather operation
    Scatter,            // Scatter operation
    InductionVariable,  // Induction variable pattern
    MemoryAccess        // Regular memory access pattern
  };

  Kind OpKind;
  Function *Func;
  Loop *L;
  std::string Description;
  double ConfidenceScore; // 0.0 to 1.0
  std::vector<std::string> Reasons;
  unsigned EstimatedSpeedup; // Percentage

  VectorizationOpportunity(Kind K, Function *F, Loop *TheLoop)
      : OpKind(K), Func(F), L(TheLoop), ConfidenceScore(0.0),
        EstimatedSpeedup(0) {}
};

/// Represents a dependency issue that prevents vectorization
struct DependencyIssue {
  enum IssueKind {
    LoopCarriedDependency,
    UnknownPointerAliasing,
    ComplexControlFlow,
    UnsupportedOperation,
    MemoryDependency,
    CallToUnknownFunction
  };

  IssueKind Kind;
  Function *Func;
  Loop *L;
  std::string Description;
  std::vector<std::string> Details;

  DependencyIssue(IssueKind K, Function *F, Loop *TheLoop)
      : Kind(K), Func(F), L(TheLoop) {}
};

/// Main analyzer class
class VectorizationAnalyzer {
public:
  VectorizationAnalyzer(Module &M, unsigned VLen, unsigned LMul,
                        const std::string &OutDir, bool Chinese, bool Verbose);

  /// Run the complete analysis
  bool analyze();

  /// Generate output reports
  bool generateReports();

private:
  Module &Mod;
  unsigned VLEN;
  unsigned LMUL;
  std::string OutputDir;
  bool EnableChinese;
  bool VerboseMode;

  std::vector<VectorizationOpportunity> Opportunities;
  std::vector<DependencyIssue> Issues;
  DenseMap<Function *, std::unique_ptr<LoopInfo>> FunctionLoops;
  DenseMap<Function *, std::unique_ptr<ScalarEvolution>> FunctionSCEV;
  DenseMap<Function *, std::unique_ptr<DominatorTree>> FunctionDT;

  /// Analyze a single function
  void analyzeFunction(Function &F);

  /// Analyze a loop for vectorization opportunities
  void analyzeLoop(Function &F, Loop *L, LoopInfo &LI, ScalarEvolution &SE);

  /// Check for loop-carried dependencies
  void checkDependencies(Function &F, Loop *L, ScalarEvolution &SE);

  /// Recognize vectorization patterns
  void recognizePatterns(Function &F, Loop *L, ScalarEvolution &SE);

  /// Calculate confidence score for an opportunity
  double calculateConfidence(const VectorizationOpportunity &Opp);

  /// Generate English report
  void generateEnglishReport(raw_ostream &OS);

  /// Generate Chinese report
  void generateChineseReport(raw_ostream &OS);

  /// Generate JSON report
  void generateJSONReport(raw_ostream &OS);

  /// Translate text to Chinese
  std::string translateToChinese(const std::string &Text);

  /// Get kind name as string
  std::string getOpportunityKindName(VectorizationOpportunity::Kind K);
  std::string getIssueKindName(DependencyIssue::IssueKind K);
};

} // namespace llvm

#endif // LLVM_TOOLS_LLVM_VEC_ANALYZER_VECTORIZATIONANALYZER_H
