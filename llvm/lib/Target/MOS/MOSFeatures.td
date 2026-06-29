//===-- MOSFeatures.td - MOS Features ---------------------*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
// MOS Features
//===---------------------------------------------------------------------===//

def FeatureAltRegisterNamesOnly
    : SubtargetFeature<"mos-long-register-names", "LongRegisterNames",
                        "true",
                        "Requires llvm_mos_* prefixes to all registers. "#
                        "Useful if your code has variable names that conflict "#
                        "with llvm-mos register names">;

def FeatureStaticStack :
  SubtargetFeature<
    "static-stack", "StaticStack", "true",
    "Whether to use statically-allocated stack frames if possible.">;

// Experimental: emit native 65816 16-bit accumulator regions (REP/SEP) for i16
// add/sub/and/or/xor. Off by default; no CPU implies it. The ABI and 8-bit
// accumulator at function boundaries are unchanged.
def FeatureNative16BitAccumulator :
  SubtargetFeature<
    "native-16bit-accumulator", "HasNative16BitAccumulator", "true",
    "Emit native 65816 16-bit accumulator regions (REP/SEP) for i16 work.">;
