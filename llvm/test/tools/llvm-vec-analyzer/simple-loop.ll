; RUN: llvm-vec-analyzer %s -output-dir %t -vlen 128 -lmul 1 -chinese | FileCheck %s
; RUN: cat %t/analysis_report_en.txt | FileCheck %s --check-prefix=ENGLISH
; RUN: cat %t/analysis_report_zh.txt | FileCheck %s --check-prefix=CHINESE
; RUN: cat %t/analysis_report.json | FileCheck %s --check-prefix=JSON

; Simple vectorizable loop test
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

; CHECK: Analysis complete

; ENGLISH: VECTORIZATION OPPORTUNITIES
; ENGLISH: SimpleLoop
; ENGLISH: Function: simple_loop
; ENGLISH: DEPENDENCY ISSUES

; CHINESE: 向量化机会
; CHINESE: 简单循环
; CHINESE: 函数: simple_loop
; CHINESE: 依赖问题

; JSON: "type": "SimpleLoop"
; JSON: "function": "simple_loop"
