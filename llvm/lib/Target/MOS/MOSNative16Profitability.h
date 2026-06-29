//===-- MOSNative16Profitability.h - native16 chain heuristic ---*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the MOS native 16-bit accumulator profitability pass.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MOS_MOSNATIVE16PROFITABILITY_H
#define LLVM_LIB_TARGET_MOS_MOSNATIVE16PROFITABILITY_H

#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

MachineFunctionPass *createMOSNative16ProfitabilityPass();

} // namespace llvm

#endif // not LLVM_LIB_TARGET_MOS_MOSNATIVE16PROFITABILITY_H
