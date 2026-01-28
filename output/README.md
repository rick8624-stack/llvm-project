# Code Analysis Agent for Vectorization Opportunities - Documentation

This folder contains the specification documents for the Code Analysis Agent targeting RISC-V vectorization.

## Files

- **CodeAnalysisAgentVectorizationSpec_EN.rst** - English version of the specification
- **CodeAnalysisAgentVectorizationSpec_ZH.rst** - Chinese version of the specification (中文版本)

## Description

These documents specify an agent responsible for analyzing source code to identify vectorization opportunities for RISC-V Vector Extension (RVV).

**Important**: This is a specification document only - it does not implement any actual code analysis.

## Contents

Both documents include:
- Mission and scope
- Input requirements (source code, LLVM IR, RISC-V parameters)
- Output specifications (candidates, dependency analysis, assessments)
- Four key capabilities:
  1. Loop Analysis
  2. Dependency Checking
  3. Pattern Recognition
  4. Complexity Assessment
- Interaction points with other agents
- Example scenarios with code and analysis
- Glossary of technical terms

---

# 向量化机会代码分析Agent - 文档

本文件夹包含针对RISC-V向量化的代码分析Agent的规范文档。

## 文件

- **CodeAnalysisAgentVectorizationSpec_EN.rst** - 规范的英文版本
- **CodeAnalysisAgentVectorizationSpec_ZH.rst** - 规范的中文版本

## 说明

这些文档规定了一个负责分析源代码以识别RISC-V向量扩展（RVV）向量化机会的Agent。

**重要**：这仅是一个规范文档 - 它不实现任何实际的代码分析。

## 内容

两个文档都包括：
- 任务和范围
- 输入要求（源代码、LLVM IR、RISC-V参数）
- 输出规范（候选、依赖性分析、评估）
- 四个关键能力：
  1. 循环分析
  2. 依赖性检查
  3. 模式识别
  4. 复杂度评估
- 与其他Agent的交互点
- 带有代码和分析的示例场景
- 技术术语表
