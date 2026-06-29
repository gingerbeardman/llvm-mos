; Phase 1 red test for the native 16-bit accumulator experiment.
;
; A chain of i16 operations should be selected as a single native 16-bit
; accumulator region on the 65816: one `rep #$20` to enter M=0, native 16-bit
; ADC/EOR/AND/ORA, one `sep #$20` to restore M=1, and the unchanged A-low/X-high
; return ABI. The 65816 16-bit-immediate (_Immediate16, MLow=1) opcodes for the
; CC1 family already exist; this feature wires selection + REP/SEP insertion.
;
; This test is RED until the feature is implemented: today the backend expands
; i16 byte-at-a-time and never emits REP/SEP.
;
; RUN: llc -mcpu=mosw65816 -mattr=+native-16bit-accumulator -verify-machineinstrs < %s | FileCheck %s

target datalayout = "e-m:e-p:16:8-p1:8:8-i16:8-i32:8-i64:8-f32:8-f64:8-a:8-Fi8-n8"
target triple = "mos"

define i16 @chain(i16 %a, i16 %b, i16 %c) {
entry:
  %t0 = add i16 %a, %b
  %t1 = xor i16 %t0, %c
  %t2 = and i16 %t1, 32767      ; 0x7fff
  %t3 = or  i16 %t2, 256        ; 0x0100
  ret i16 %t3
}

; The whole chain is one merged native region: a single REP at the head, the
; value threaded through the 16-bit accumulator across all four ops with NO
; intervening SEP/REP (the CHECK-NOT lines), and a single SEP before the 8-bit
; A/X return.
; CHECK-LABEL: chain:
; CHECK:       rep #{{(32|\$20)}}
; CHECK:       adc
; CHECK-NOT:   sep
; CHECK:       eor
; CHECK-NOT:   sep
; CHECK:       and
; CHECK-NOT:   sep
; CHECK:       ora
; CHECK:       sep #{{(32|\$20)}}
; CHECK:       rts
