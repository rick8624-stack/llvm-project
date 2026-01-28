===================================================
Code Analysis Agent for Vectorization Opportunities
===================================================

.. contents::
   :local:

Mission
=======

This document specifies an agent responsible for analyzing source code to identify scalar regions, loop nests, and data structures that are potentially eligible for vectorization, with particular focus on RISC-V Vector Extension (RVV) capabilities.

This agent specification describes the analysis phase where code is examined for vectorization opportunities using loop analysis, dependency checking, and pattern recognition.

.. important::
   **ABSOLUTE PROHIBITION**: This agent specification does NOT describe actual code analysis implementation in this repository. This is a specification document only, intended to guide the design and implementation of external analysis tools.

Scope
=====

The Code Analysis Agent operates during the early stages of the optimization pipeline, identifying code patterns and regions that can benefit from vectorization transformations. It focuses on:

* Scalar code regions with vectorization potential
* Loop structures amenable to SIMD operations
* Data access patterns suitable for vector operations
* Control flow that can be efficiently vectorized

Target Architecture
===================

The agent is designed to analyze code for the **RISC-V Vector Extension (RVV)**, considering:

* **VLEN**: Vector register length (configurable, typically 128, 256, 512, or 1024 bits)
* **LMUL**: Vector register grouping (⅛, ¼, ½, 1, 2, 4, 8)
* **SEW**: Selected element width (8, 16, 32, 64 bits)
* **ELEN**: Maximum element width supported by implementation

See `RISC-V Vector Extension <RISCVVectorExtension.html>`_ for details on RVV support in LLVM.

Inputs
======

The agent accepts the following inputs for analysis:

Source Code
-----------

* **C/C++ source files**: Primary analysis target
* **Fortran source files**: For scientific computing workloads
* **Other compiled languages**: Any language that compiles to LLVM IR

Intermediate Representation
---------------------------

* **LLVM IR**: Primary analysis format
* **Loop metadata**: Information about loop properties
* **Alias analysis results**: Memory dependency information
* **Profile data**: Optional runtime profile information

Architecture Parameters
-----------------------

* **VLEN**: Target vector length
* **LMUL capabilities**: Available vector register grouping options
* **ISA extensions**: Enabled RISC-V extensions (V, Zve*, Zvl*)
* **Microarchitecture features**: Target-specific optimization parameters

Outputs
=======

The agent produces structured analysis results:

Vectorization Candidates
------------------------

A list of code regions identified as vectorization opportunities, including:

* **Location**: Source location or IR instruction range
* **Loop properties**: Trip count, bounds, induction variables
* **Data types**: Element types and sizes
* **Access patterns**: Sequential, strided, gather/scatter
* **Confidence score**: Likelihood of successful vectorization (0.0-1.0)

Dependency Analysis Reports
---------------------------

For each candidate region:

* **Data dependencies**: Loop-carried dependencies, memory aliasing
* **Control dependencies**: Conditional execution, early exits
* **Reduction operations**: Identification of reduction patterns
* **Dependence distance**: Distance vectors for carried dependencies

Vectorization Assessment
------------------------

* **Recommended LMUL**: Optimal vector register grouping
* **Estimated speedup**: Projected performance improvement
* **Vectorization strategy**: Approach to use (strip-mining, unroll-and-jam, etc.)
* **Constraints**: Alignment requirements, trip count assumptions

Non-Vectorizable Regions
------------------------

For regions that cannot be vectorized:

* **Location**: Code region identification
* **Reason**: Specific blocker (dependencies, unsupported operations, etc.)
* **Suggestions**: Potential code transformations to enable vectorization

Capabilities
============

1. Loop Analysis
----------------

The agent identifies and analyzes loop structures:

**Loop Classification**

* Count-controlled loops vs. condition-controlled loops
* Single-entry single-exit (SESE) loops
* Nested loop structures and perfect/imperfect nesting
* Loop bounds analysis (constant, affine, non-affine)

**Induction Variable Analysis**

* Identification of primary and secondary induction variables
* Recognition of linear, polynomial induction patterns
* Detection of induction variable updates and uses

**Trip Count Analysis**

* Static trip count determination
* Runtime trip count profiling
* Trip count estimation and bounds

**Loop Characteristics**

* Loop body complexity (instruction count, operations)
* Control flow within loop body
* Function calls and side effects
* Memory access patterns

2. Dependency Checking
----------------------

The agent performs comprehensive dependency analysis:

**Memory Dependencies**

* Loop-carried dependencies (RAW, WAR, WAW)
* Cross-iteration dependencies
* Memory aliasing analysis
* Pointer disambiguation

**Control Dependencies**

* Conditional execution paths
* Early loop exits (break, return)
* Loop-variant conditions
* Exception handling

**Reduction Detection**

* Associative reduction operations (+, *, min, max, &, |, ^)
* Non-associative reductions requiring special handling
* Reduction variable identification
* Reduction pattern validation

**Dependency Distance**

* Dependence vectors for multi-dimensional access
* Uniform and non-uniform stride analysis
* Forward and backward dependencies
* Distance bounds computation

3. Pattern Recognition
----------------------

The agent recognizes common vectorizable patterns:

**Memory Access Patterns**

* **Sequential access**: Contiguous memory access (``a[i]``)
* **Strided access**: Regular stride (``a[i*stride]``)
* **Gather operations**: Indirect indexing (``a[idx[i]]``)
* **Scatter operations**: Indirect store operations
* **Broadcast**: Single value to all vector elements

**Computational Patterns**

* **Element-wise operations**: Independent operations per element
* **Reductions**: Accumulation across iterations (sum, product, etc.)
* **Scans**: Prefix operations (prefix sum, etc.)
* **Histogram operations**: Bucketing and counting
* **Dot products and matrix operations**: Linear algebra patterns

**Control Flow Patterns**

* **Predicated execution**: Conditional operations suitable for masking
* **Select operations**: Ternary conditional patterns
* **Min/max operations**: Comparison-based selection

**Data Type Patterns**

* Integer operations (signed/unsigned, various widths)
* Floating-point operations (single, double precision)
* Mixed-type operations and type conversions
* Saturation arithmetic

4. Complexity Assessment
------------------------

The agent evaluates vectorization feasibility:

**Code Complexity Metrics**

* Loop body instruction count
* Control flow complexity (branches, nested conditions)
* Function call overhead
* Memory access complexity

**Vectorization Feasibility**

* **High feasibility**: Simple loops with no dependencies
* **Medium feasibility**: Loops with manageable dependencies or predication
* **Low feasibility**: Complex control flow or non-vectorizable operations
* **Not feasible**: Fundamental blockers (recursive calls, complex aliasing)

**Cost-Benefit Analysis**

* Estimated vector overhead (mask generation, gather/scatter cost)
* Scalar vs. vector code size impact
* Register pressure considerations
* Memory bandwidth utilization

**Architecture Suitability**

* Match between code patterns and RISC-V vector capabilities
* LMUL selection based on register pressure
* VLEN utilization efficiency
* Instruction support in target configuration

Interaction Points
==================

The Code Analysis Agent integrates with other components:

Code Transformation Agent
-------------------------

* **Provides**: Prioritized list of vectorization candidates
* **Receives**: Transformation success/failure feedback
* **Protocol**: Structured candidate descriptions with metadata

**Data Exchange Format**::

    {
      "region_id": "loop_123",
      "location": { "file": "kernel.c", "line": 42 },
      "loop_properties": { ... },
      "recommended_strategy": "strip-mine",
      "confidence": 0.85
    }

Validation Agent
----------------

* **Provides**: Baseline performance metrics and correctness data
* **Receives**: Validation test results
* **Purpose**: Enable comparison of vectorized vs. scalar code

**Validation Requirements**:

* Functional correctness verification
* Numerical accuracy for floating-point operations
* Edge case handling (zero trip count, alignment, etc.)

Performance Analysis Agent
--------------------------

* **Provides**: Pre-optimization performance characteristics
* **Receives**: Post-vectorization performance data
* **Purpose**: Measure and report vectorization effectiveness

**Performance Metrics**:

* Execution time (cycles, wall-clock time)
* Instruction throughput
* Memory bandwidth utilization
* Vectorization factor achieved

Compilation Pipeline Integration
--------------------------------

The agent fits into the LLVM optimization pipeline:

1. **Early Analysis**: After initial IR generation, before major optimizations
2. **Loop Optimization Phase**: Integrated with loop analysis passes
3. **Vectorization Phase**: Provides input to loop vectorizer
4. **Late Analysis**: Post-optimization opportunity identification

Analysis Methodology
====================

Algorithm Overview
------------------

The agent follows this high-level algorithm:

1. **Loop Identification**: Find all loops in the program
2. **Candidate Filtering**: Filter loops by basic vectorizability criteria
3. **Dependency Analysis**: Perform detailed dependency checking
4. **Pattern Matching**: Identify specific vectorizable patterns
5. **Cost Modeling**: Estimate vectorization benefit
6. **Ranking**: Prioritize candidates by expected benefit
7. **Report Generation**: Produce structured analysis results

Heuristics
----------

The agent employs heuristics for decision-making:

* **Minimum trip count threshold**: Skip short loops (typically < 4 iterations)
* **Complexity cutoff**: Avoid overly complex loop bodies
* **Memory access regularity**: Favor predictable access patterns
* **Architecture alignment**: Prefer patterns well-suited to RISC-V RVV

Confidence Scoring
------------------

Confidence scores reflect vectorization success probability:

* **0.9-1.0**: High confidence - simple, obviously vectorizable loops
* **0.7-0.9**: Good confidence - manageable dependencies or conditions
* **0.5-0.7**: Medium confidence - requires advanced techniques (masking, etc.)
* **0.3-0.5**: Low confidence - significant challenges or unknowns
* **0.0-0.3**: Very low confidence - borderline cases

Limitations
===========

This specification acknowledges the following limitations:

**Analysis Scope**

* Analysis is limited to loop-level vectorization
* Whole-program analysis is not performed
* Interprocedural analysis is limited

**Dependency Analysis**

* Conservative alias analysis may over-report dependencies
* Indirect memory access may prevent accurate analysis
* Dynamic pointer values limit compile-time analysis

**Pattern Recognition**

* Novel or complex patterns may not be recognized
* Code obfuscation can prevent pattern matching
* Language-specific idioms may require custom handling

**Architecture Assumptions**

* Analysis assumes specific RISC-V vector capabilities
* May not generalize to other vector architectures
* Microarchitecture details may not be fully captured

Example Analysis Scenarios
==========================

Scenario 1: Simple Loop - High Confidence
-----------------------------------------

**Input Code**::

    void add_arrays(float *a, float *b, float *c, int n) {
        for (int i = 0; i < n; i++) {
            c[i] = a[i] + b[i];
        }
    }

**Analysis Output**:

* **Vectorization candidate**: Yes
* **Confidence**: 0.95
* **Pattern**: Element-wise operation, sequential access
* **Recommended LMUL**: 1 or 2 (depending on VLEN)
* **Estimated speedup**: 4-8x (depending on architecture)
* **Dependencies**: None
* **Blockers**: None

Scenario 2: Loop with Reduction - Medium Confidence
---------------------------------------------------

**Input Code**::

    float dot_product(float *a, float *b, int n) {
        float sum = 0.0f;
        for (int i = 0; i < n; i++) {
            sum += a[i] * b[i];
        }
        return sum;
    }

**Analysis Output**:

* **Vectorization candidate**: Yes
* **Confidence**: 0.75
* **Pattern**: Reduction (sum), element-wise multiply
* **Recommended LMUL**: 1
* **Estimated speedup**: 3-6x
* **Dependencies**: Reduction carry (handled by vector reduction support)
* **Blockers**: None (RVV supports vector reductions)

Scenario 3: Conditional Loop - Low Confidence
---------------------------------------------

**Input Code**::

    void conditional_update(int *data, int n, int threshold) {
        for (int i = 0; i < n; i++) {
            if (data[i] > threshold) {
                data[i] = data[i] * 2;
            }
        }
    }

**Analysis Output**:

* **Vectorization candidate**: Yes
* **Confidence**: 0.60
* **Pattern**: Conditional update, requires masking
* **Recommended LMUL**: 1
* **Estimated speedup**: 2-4x (mask overhead)
* **Dependencies**: None
* **Strategy**: Use masked vector operations
* **Notes**: Efficiency depends on mask density

Scenario 4: Non-Vectorizable Loop
---------------------------------

**Input Code**::

    int fibonacci_sum(int n) {
        int a = 0, b = 1, sum = 0;
        for (int i = 0; i < n; i++) {
            sum += a;
            int temp = a + b;
            a = b;
            b = temp;
        }
        return sum;
    }

**Analysis Output**:

* **Vectorization candidate**: No
* **Confidence**: 0.05
* **Pattern**: Sequential dependency chain
* **Dependencies**: Loop-carried dependencies on a and b
* **Blockers**: Each iteration depends on previous iteration results
* **Reason**: Cannot parallelize due to fundamental data dependencies
* **Suggestions**: Consider algorithmic changes if possible

References
==========

* `RISC-V Vector Extension Specification <https://github.com/riscv/riscv-v-spec>`_
* `LLVM Loop Vectorizer <../Vectorizers.html>`_
* `LLVM Vectorization Plan <../VectorizationPlan.html>`_
* `RISC-V Vector Extension in LLVM <RISCVVectorExtension.html>`_

Glossary
========

.. glossary::

   VLEN
      Vector register length in bits. Determines the size of vector registers in the RISC-V Vector Extension.

   LMUL
      Vector register grouping (Length Multiplier). Allows treating multiple vector registers as a single logical register.

   SEW
      Selected Element Width. The width in bits of elements in vector operations.

   ELEN
      Maximum element width. The largest element size supported by a RISC-V vector implementation.

   Strip-mining
      A loop transformation technique that divides loop iterations into chunks suitable for vectorization.

   Reduction
      An operation that combines multiple values into a single result (e.g., sum, product, min, max).

   Gather/Scatter
      Vector memory operations that access non-contiguous memory locations using an index vector.

   Predication
      Conditional execution using mask registers to selectively enable or disable operations.

   Loop-carried dependency
      A data dependency where one iteration of a loop depends on a previous iteration.

Version History
===============

* **Version 1.0** (2026-01-28): Initial specification for Code Analysis Agent for Vectorization Opportunities

.. note::
   This is a specification document only. It does not implement or perform any actual code analysis within the LLVM codebase. This specification is intended to guide the development of external analysis tools and agents.
