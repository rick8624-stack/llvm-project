//===-- llvm-vec-analyzer.cpp - Vectorization Opportunity Analyzer -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This tool analyzes LLVM IR to identify scalar regions, loop nests, and data
// structures that are potentially eligible for vectorization, with a focus on
// RISC-V target architecture.
//
//===----------------------------------------------------------------------===//

#include "VectorizationAnalyzer.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::OptionCategory VecAnalyzerCategory("llvm-vec-analyzer options");

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<input LLVM IR file>"),
                                          cl::value_desc("filename"),
                                          cl::cat(VecAnalyzerCategory),
                                          cl::Required);

static cl::opt<std::string> OutputDir("output-dir",
                                      cl::desc("Output directory for analysis reports"),
                                      cl::value_desc("directory"),
                                      cl::init("vec_analysis_output"),
                                      cl::cat(VecAnalyzerCategory));

static cl::opt<unsigned> VLEN("vlen",
                              cl::desc("RISC-V Vector register length in bits"),
                              cl::value_desc("bits"),
                              cl::init(128),
                              cl::cat(VecAnalyzerCategory));

static cl::opt<unsigned> LMUL("lmul",
                              cl::desc("RISC-V Vector LMUL (register grouping) value"),
                              cl::value_desc("value"),
                              cl::init(1),
                              cl::cat(VecAnalyzerCategory));

static cl::opt<bool> EnableChinese("chinese",
                                   cl::desc("Enable Chinese translation for outputs"),
                                   cl::init(true),
                                   cl::cat(VecAnalyzerCategory));

static cl::opt<bool> Verbose("verbose",
                             cl::desc("Enable verbose output"),
                             cl::init(false),
                             cl::cat(VecAnalyzerCategory));

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  cl::HideUnrelatedOptions(VecAnalyzerCategory);
  cl::ParseCommandLineOptions(argc, argv,
                              "LLVM Vectorization Opportunity Analyzer\n\n"
                              "This tool analyzes LLVM IR to identify vectorization "
                              "opportunities for RISC-V architecture.\n");

  LLVMContext Context;
  SMDiagnostic Err;

  // Load the input module
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  // Create output directory
  std::error_code EC = sys::fs::create_directories(OutputDir);
  if (EC) {
    errs() << "Error creating output directory: " << EC.message() << "\n";
    return 1;
  }

  if (Verbose) {
    outs() << "Analyzing module: " << M->getName() << "\n";
    outs() << "RISC-V Parameters: VLEN=" << VLEN << ", LMUL=" << LMUL << "\n";
    outs() << "Output directory: " << OutputDir << "\n";
  }

  // Create the vectorization analyzer
  VectorizationAnalyzer Analyzer(*M, VLEN, LMUL, OutputDir, EnableChinese, Verbose);

  // Run the analysis
  if (!Analyzer.analyze()) {
    errs() << "Analysis failed\n";
    return 1;
  }

  // Generate reports
  if (!Analyzer.generateReports()) {
    errs() << "Report generation failed\n";
    return 1;
  }

  if (Verbose) {
    outs() << "Analysis complete. Reports generated in: " << OutputDir << "\n";
  }

  return 0;
}
