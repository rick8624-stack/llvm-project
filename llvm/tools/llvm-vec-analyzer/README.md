# llvm-vec-analyzer - Vectorization Opportunity Analyzer

## Overview

`llvm-vec-analyzer` is a tool for analyzing LLVM IR to identify scalar regions, loop nests, and data structures that are potentially eligible for vectorization, with a specific focus on RISC-V architecture.

## Features

- **Loop Analysis**: Identifies loop structures amenable to vectorization
- **Dependency Checking**: Detects data dependencies that may prevent vectorization
- **Pattern Recognition**: Recognizes common patterns (reductions, scans, gathers/scatters)
- **RISC-V Focus**: Supports RISC-V specific parameters (VLEN, LMUL)
- **Multiple Output Formats**: Generates English, Chinese, and JSON reports
- **Confidence Scoring**: Provides confidence scores for each vectorization opportunity

## Usage

```bash
llvm-vec-analyzer [options] <input LLVM IR file>
```

### Options

- `--output-dir=<directory>` - Output directory for analysis reports (default: vec_analysis_output)
- `--vlen=<bits>` - RISC-V Vector register length in bits (default: 128)
- `--lmul=<value>` - RISC-V Vector LMUL (register grouping) value (default: 1)
- `--chinese` - Enable Chinese translation for outputs (default: true)
- `--verbose` - Enable verbose output (default: false)

### Example

```bash
# Analyze a simple loop
llvm-vec-analyzer input.ll -output-dir ./analysis -vlen 256 -lmul 2

# Analyze with verbose output
llvm-vec-analyzer input.ll -verbose

# Disable Chinese output
llvm-vec-analyzer input.ll -chinese=false
```

## Output Files

The tool generates three report files in the output directory:

1. **analysis_report_en.txt** - English language report
2. **analysis_report_zh.txt** - Chinese language report (if enabled)
3. **analysis_report.json** - Machine-readable JSON report

## Report Contents

### Vectorization Opportunities

Each identified opportunity includes:
- Type (SimpleLoop, NestedLoop, Reduction, etc.)
- Function name
- Description
- Confidence score (0.0 to 1.0)
- Estimated speedup percentage
- Detailed reasons

### Dependency Issues

Each identified issue includes:
- Type (LoopCarriedDependency, ComplexControlFlow, etc.)
- Function name
- Description
- Detailed information

### Interaction Points

The report includes information for downstream agents:
- Code Transformation Agent: Ready regions for transformation
- Validation Agent: Baseline metrics for comparison
- Performance Analysis Agent: Pre-optimization metrics

## Integration with LLVM Pipeline

This tool is designed to work as part of a larger vectorization workflow:

1. **Analysis Phase** (this tool): Identify vectorization opportunities
2. **Transformation Phase**: Apply vectorization transformations
3. **Validation Phase**: Verify correctness
4. **Performance Analysis Phase**: Measure performance improvements

## Examples

### Simple Loop Example

Input IR:
```llvm
define void @simple_loop(ptr %a, ptr %b, i32 %n) {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %loop ]
  %gep.a = getelementptr i32, ptr %a, i32 %i
  %gep.b = getelementptr i32, ptr %b, i32 %i
  %val = load i32, ptr %gep.a
  %add = add i32 %val, 1
  store i32 %add, ptr %gep.b
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, %n
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}
```

Output will identify:
- Simple loop structure suitable for vectorization
- Regular memory access patterns
- Induction variable pattern
- Estimated speedup based on VLEN

## Technical Details

### Supported Pattern Types

- **SimpleLoop**: Simple countable loops
- **NestedLoop**: Nested loop structures
- **Reduction**: Sum, product, min, max reductions
- **Scan**: Prefix-sum patterns
- **Gather/Scatter**: Irregular memory access patterns
- **InductionVariable**: Induction variable patterns
- **MemoryAccess**: Regular memory access patterns

### RISC-V Specific Features

The tool considers RISC-V Vector Extension (RVV) parameters:
- **VLEN**: Vector register length (affects vectorization factor)
- **LMUL**: Register grouping (affects effective vector length)

### Confidence Scoring

Confidence scores are calculated based on:
- Loop structure simplicity
- Memory access patterns
- Control flow complexity
- Trip count predictability
- Dependency analysis results

## Building

The tool is built as part of the standard LLVM build process:

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ../llvm
ninja llvm-vec-analyzer
```

## Testing

Tests are located in `llvm/test/tools/llvm-vec-analyzer/`:

```bash
ninja check-llvm-vec-analyzer
```

## Future Enhancements

- Support for more architecture targets (x86, ARM)
- Enhanced pattern recognition
- Cost modeling integration
- Interactive visualization
- Integration with compiler optimization pipeline
