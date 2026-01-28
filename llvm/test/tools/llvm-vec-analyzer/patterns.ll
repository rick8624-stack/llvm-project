; RUN: llvm-vec-analyzer %s -output-dir %t -vlen 128 -lmul 1 | FileCheck %s
; RUN: FileCheck --input-file=%t/analysis_report.json %s --check-prefix=JSON

; Test reduction pattern detection
define i32 @test_reduction(ptr nocapture readonly %arr, i32 %n) {
entry:
  br label %for.body

for.body:
  %i = phi i32 [ 0, %entry ], [ %i.next, %for.body ]
  %sum = phi i32 [ 0, %entry ], [ %sum.next, %for.body ]
  %gep = getelementptr inbounds i32, ptr %arr, i32 %i
  %val = load i32, ptr %gep
  %sum.next = add i32 %sum, %val
  %i.next = add i32 %i, 1
  %cmp = icmp slt i32 %i.next, %n
  br i1 %cmp, label %for.body, label %exit

exit:
  ret i32 %sum.next
}

; Test nested loop detection
define void @test_nested(ptr %a, i32 %m, i32 %n) {
entry:
  br label %outer.loop

outer.loop:
  %i = phi i32 [ 0, %entry ], [ %i.next, %outer.latch ]
  br label %inner.loop

inner.loop:
  %j = phi i32 [ 0, %outer.loop ], [ %j.next, %inner.loop ]
  %idx = add i32 %i, %j
  %gep = getelementptr i32, ptr %a, i32 %idx
  store i32 0, ptr %gep
  %j.next = add i32 %j, 1
  %cmp.j = icmp slt i32 %j.next, %n
  br i1 %cmp.j, label %inner.loop, label %outer.latch

outer.latch:
  %i.next = add i32 %i, 1
  %cmp.i = icmp slt i32 %i.next, %m
  br i1 %cmp.i, label %outer.loop, label %exit

exit:
  ret void
}

; CHECK: Analysis complete
; CHECK: Found {{[0-9]+}} vectorization opportunities

; JSON: "type": "Reduction"
; JSON: "type": "NestedLoop"
; JSON: "confidence_score"
