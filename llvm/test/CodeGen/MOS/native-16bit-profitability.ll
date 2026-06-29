; Profitability heuristic for the native 16-bit accumulator experiment
; (+native-16bit-accumulator). The MOSNative16Profitability pre-legalizer pass
; only keeps an i16 op native when it belongs to a data-flow chain of at least
; -mos-native16-min-chain (default 3) ops, so an isolated/short i16 run narrows
; to a byte-split (no REP/SEP) while a long chain forms one native region.
;
; RUN: llc -mcpu=mosw65816 -mattr=+native-16bit-accumulator -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mcpu=mosw65816 -mattr=+native-16bit-accumulator -mos-native16-min-chain=4 -verify-machineinstrs < %s | FileCheck %s --check-prefix=THR4

target datalayout = "e-m:e-p:16:8-p1:8:8-i16:8-i32:8-i64:8-f32:8-f64:8-a:8-Fi8-n8"
target triple = "mos"

; A single i16 op (chain length 1) must NOT enter a native region.
; CHECK-LABEL: lone:
; CHECK-NOT: rep
; CHECK: rts
define i16 @lone(i16 %a, i16 %b) {
  %r = add i16 %a, %b
  ret i16 %r
}

; A 2-op chain is below the default threshold of 3: still no native region.
; CHECK-LABEL: two:
; CHECK-NOT: rep
; CHECK: rts
define i16 @two(i16 %a, i16 %b, i16 %c) {
  %t = add i16 %a, %b
  %r = xor i16 %t, %c
  ret i16 %r
}

; A 3-op chain hits the threshold: exactly one merged REP ... SEP region.
; CHECK-LABEL: three:
; CHECK: rep #32
; CHECK-NOT: rep #32
; CHECK: sep #32
; CHECK-NOT: sep #32
; CHECK: rts
;
; With the threshold raised to 4, the same 3-op chain must narrow (no region).
; THR4-LABEL: three:
; THR4-NOT: rep
; THR4: rts
define i16 @three(i16 %a, i16 %b, i16 %c, i16 %d) {
  %t = add i16 %a, %b
  %u = xor i16 %t, %c
  %r = and i16 %u, %d
  ret i16 %r
}
