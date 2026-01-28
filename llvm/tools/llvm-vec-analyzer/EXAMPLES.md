# LLVM Vectorization Analyzer - Usage Examples

This document provides comprehensive examples of using the llvm-vec-analyzer tool.

## Example 1: Simple Vector Addition

### Source Code (C)
```c
void vector_add(int *a, const int *b, const int *c, int n) {
  for (int i = 0; i < n; i++) {
    a[i] = b[i] + c[i];
  }
}
```

### Compile to LLVM IR
```bash
clang -O1 -emit-llvm -S vector_add.c -o vector_add.ll
```

### Run Analysis
```bash
llvm-vec-analyzer vector_add.ll \
  -output-dir ./analysis_results \
  -vlen 256 \
  -lmul 2 \
  -verbose
```

### Expected Output
```
Analyzing module: vector_add.ll
RISC-V Parameters: VLEN=256, LMUL=2
Output directory: ./analysis_results
Starting analysis...
Analyzing function: vector_add
Analysis complete:
  Found 3 vectorization opportunities
  Found 0 dependency issues
```

### Analysis Report Highlights
- **SimpleLoop**: High confidence (0.8-0.9) for basic loop structure
- **MemoryAccess**: Regular stride-1 memory accesses detected
- **InductionVariable**: Simple induction variable pattern
- **Estimated Speedup**: 75-87% based on VLEN=256

## Example 2: Reduction Operations

### Source Code (C)
```c
int sum_array(const int *arr, int n) {
  int sum = 0;
  for (int i = 0; i < n; i++) {
    sum += arr[i];
  }
  return sum;
}
```

### Run Analysis
```bash
llvm-vec-analyzer sum_array.ll -vlen 512 -lmul 4 -chinese
```

### Key Findings
- **Reduction Pattern**: Confidence score 0.85
- **RISC-V Vector Reduction**: Can use `vredsum` instruction
- **Estimated Speedup**: 60% with proper vector reduction
- **Chinese Report**: Full translation available

## Example 3: Matrix Operations

### Source Code (C)
```c
void matrix_multiply(float *C, const float *A, const float *B, int N) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      float sum = 0.0f;
      for (int k = 0; k < N; k++) {
        sum += A[i*N + k] * B[k*N + j];
      }
      C[i*N + j] = sum;
    }
  }
}
```

### Analysis Insights
- **NestedLoop**: Identifies all three loop levels
- **Recommendation**: Outer loop vectorization or loop reordering
- **Memory Access Pattern**: Strided access on B matrix (gather opportunity)
- **Reduction**: Inner loop accumulation pattern

## Example 4: Complex Control Flow

### Source Code (C)
```c
void conditional_sum(int *out, const int *arr, int n, int threshold) {
  int sum = 0;
  for (int i = 0; i < n; i++) {
    if (arr[i] > threshold) {
      sum += arr[i];
    }
  }
  *out = sum;
}
```

### Analysis Results
```json
{
  "opportunities": [
    {
      "type": "Reduction",
      "confidence_score": 0.65,
      "description": "Reduction with predication",
      "reasons": ["May require masked vector operations"]
    }
  ],
  "issues": [
    {
      "type": "ComplexControlFlow",
      "description": "Loop has complex control flow",
      "details": ["May require predication or loop versioning"]
    }
  ]
}
```

## Example 5: Different RISC-V Configurations

### Configuration 1: Embedded System (Small Vectors)
```bash
llvm-vec-analyzer input.ll -vlen 128 -lmul 1 -output-dir ./embedded_analysis
```
- Suitable for: RV32V embedded systems
- Vector factor: 4x for 32-bit integers
- Conservative estimates

### Configuration 2: High-Performance System (Large Vectors)
```bash
llvm-vec-analyzer input.ll -vlen 1024 -lmul 8 -output-dir ./hpc_analysis
```
- Suitable for: RV64V HPC systems
- Vector factor: 32x for 32-bit integers
- Maximum parallelism

### Configuration 3: Balanced Configuration
```bash
llvm-vec-analyzer input.ll -vlen 256 -lmul 2 -output-dir ./balanced_analysis
```
- Suitable for: General-purpose RISC-V systems
- Vector factor: 8x for 32-bit integers
- Good performance/complexity trade-off

## Example 6: Batch Analysis

### Analyze Multiple Files
```bash
#!/bin/bash
for file in *.ll; do
  echo "Analyzing $file..."
  llvm-vec-analyzer "$file" \
    -output-dir "analysis_$(basename $file .ll)" \
    -vlen 256 -lmul 2
done
```

## Example 7: Integration with Compilation Pipeline

### Step 1: Compile to IR
```bash
clang -O2 -emit-llvm -S mycode.c -o mycode.ll
```

### Step 2: Analyze
```bash
llvm-vec-analyzer mycode.ll -output-dir ./vec_analysis
```

### Step 3: Review Reports
```bash
# View summary
cat ./vec_analysis/analysis_report_en.txt | grep "Total opportunities"

# Extract JSON for automation
python3 << EOF
import json
with open('./vec_analysis/analysis_report.json') as f:
    data = json.load(f)
    high_confidence = [
        opp for opp in data['opportunities'] 
        if opp['confidence_score'] > 0.7
    ]
    print(f"High-confidence opportunities: {len(high_confidence)}")
EOF
```

### Step 4: Apply Transformations (Manual or Automated)
Based on the analysis report, apply appropriate vectorization passes:
```bash
opt -passes='loop-vectorize' mycode.ll -o mycode_vec.ll
```

## Example 8: Report Formats

### English Report (Default)
```
========================================
LLVM Vectorization Opportunity Analysis
========================================

Target Architecture: RISC-V
Vector Length (VLEN): 256 bits
LMUL: 2
...
```

### Chinese Report
```
========================================
LLVM 向量化机会分析报告
========================================

目标架构: RISC-V
向量长度 (VLEN): 256 位
LMUL: 2
...
```

### JSON Report (Machine-Readable)
```json
{
  "metadata": {
    "architecture": "RISC-V",
    "vlen": 256,
    "lmul": 2
  },
  "opportunities": [...],
  "issues": [...],
  "summary": {
    "total_opportunities": 10,
    "total_issues": 2
  }
}
```

## Example 9: Interpreting Confidence Scores

| Score Range | Interpretation | Action |
|-------------|----------------|--------|
| 0.9 - 1.0 | Excellent candidate | Definitely vectorize |
| 0.7 - 0.89 | Good candidate | Vectorize with standard techniques |
| 0.5 - 0.69 | Moderate candidate | May require predication or versioning |
| 0.3 - 0.49 | Challenging | Consider cost-benefit analysis |
| 0.0 - 0.29 | Poor candidate | Likely not profitable |

## Example 10: Common Patterns and Detection

### Pattern: Stride-1 Access
```c
for (i = 0; i < n; i++)
  a[i] = b[i] + c[i];
```
**Detection**: MemoryAccess with high confidence

### Pattern: Reduction
```c
sum = 0;
for (i = 0; i < n; i++)
  sum += a[i];
```
**Detection**: Reduction with 0.85 confidence

### Pattern: Strided Access
```c
for (i = 0; i < n; i += 2)
  a[i] = b[i];
```
**Detection**: MemoryAccess with note about stride

### Pattern: Indirect Access (Gather/Scatter)
```c
for (i = 0; i < n; i++)
  a[i] = b[indices[i]];
```
**Detection**: Gather pattern identified

## Tips and Best Practices

1. **Start with high confidence opportunities** (score > 0.8)
2. **Use JSON output for automation** and tool integration
3. **Compare different VLEN/LMUL configurations** to understand trade-offs
4. **Review dependency issues** before attempting vectorization
5. **Use verbose mode** during development for detailed insights
6. **Validate results** with actual vectorization passes
7. **Consider the interaction points** for holistic optimization strategy

## Troubleshooting

### Issue: No opportunities found
- Check if loops are in simplified form
- Verify IR is optimized (use -O1 or higher)
- Ensure target triple is set correctly

### Issue: Low confidence scores
- Complex control flow may reduce confidence
- Unknown pointer aliasing affects scores
- Function calls in loops reduce vectorizability

### Issue: Report generation fails
- Check write permissions for output directory
- Ensure sufficient disk space
- Verify output directory path is valid
