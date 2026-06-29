//===-- MOSNative16Profitability.cpp - native16 chain heuristic ----------===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file This file defines the MOS native 16-bit accumulator profitability
/// pass. Native 16-bit accumulator codegen (a REP #$20 ... SEP #$20 region with
/// 16-bit ADC/AND/ORA/EOR/SBC) only pays off when several i16 ops form a *chain*
/// that the late-optimization region merger fuses into a single REP/SEP,
/// amortizing the ~5-cycle / multi-byte mode switch. An isolated i16 op pays a
/// full REP/SEP it can never share, so blanket native16 is a code-size and cycle
/// regression on real fixed-point code (which is dominated by isolated ops and
/// pointer arithmetic).
///
/// This pass runs before the legalizer (on SSA generic MIR). For every i16
/// {G_ADD,G_SUB,G_AND,G_OR,G_XOR} it computes the length of the longest
/// same-block, single-use data-flow chain it belongs to. Ops in a chain of at
/// least -mos-native16-min-chain are rewritten to the matching G_NATIVE16_*
/// target-generic opcode, which passes through the legalizer untouched and
/// selects to the native REP/SEP pseudo. Everything else is left as a plain i16
/// op, which the legalizer narrows to the stock register-resident byte-split --
/// the same path the feature-off build takes. Crucially this default-narrow also
/// catches i16 ops the legalizer itself *generates* (e.g. pointer arithmetic in
/// legalizePtrAdd), which a post-legalizer decision would miss.
///
/// The decision must be made pre-selection: after register allocation even a
/// one-op native region is already smaller than a memory byte-split, so only the
/// legalizer's normal narrowing can recover stock's register-resident split.
//
//===----------------------------------------------------------------------===//

#include "MOSNative16Profitability.h"

#include "MCTargetDesc/MOSMCTargetDesc.h"
#include "MOS.h"
#include "MOSSubtarget.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/CommandLine.h"

#include <functional>

#define DEBUG_TYPE "mos-native16-profitability"

using namespace llvm;

static cl::opt<unsigned> Native16MinChain(
    "mos-native16-min-chain", cl::Hidden, cl::init(3),
    cl::desc("Minimum i16 op chain length to keep a native 16-bit accumulator "
             "region instead of narrowing to a byte-split (MOS)."));

namespace {

// Is this a generic i16 binop eligible for native 16-bit accumulator selection?
static bool isNative16Candidate(const MachineInstr &MI,
                                const MachineRegisterInfo &MRI) {
  switch (MI.getOpcode()) {
  case MOS::G_ADD:
  case MOS::G_SUB:
  case MOS::G_AND:
  case MOS::G_OR:
  case MOS::G_XOR:
    break;
  default:
    return false;
  }
  return MRI.getType(MI.getOperand(0).getReg()) == LLT::scalar(16);
}

// The G_NATIVE16_* opcode for a plain i16 binop kept native.
static unsigned native16Opcode(unsigned Opc) {
  switch (Opc) {
  case MOS::G_ADD:
    return MOS::G_NATIVE16_ADD;
  case MOS::G_SUB:
    return MOS::G_NATIVE16_SUB;
  case MOS::G_AND:
    return MOS::G_NATIVE16_AND;
  case MOS::G_OR:
    return MOS::G_NATIVE16_OR;
  case MOS::G_XOR:
    return MOS::G_NATIVE16_XOR;
  default:
    llvm_unreachable("not an i16 native16 candidate opcode");
  }
}

class MOSNative16Profitability : public MachineFunctionPass {
public:
  static char ID;

  MOSNative16Profitability() : MachineFunctionPass(ID) {
    llvm::initializeMOSNative16ProfitabilityPass(
        *PassRegistry::getPassRegistry());
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::IsSSA);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

bool MOSNative16Profitability::runOnMachineFunction(MachineFunction &MF) {
  const MOSSubtarget &STI = MF.getSubtarget<MOSSubtarget>();
  if (!STI.hasNative16BitAccumulator())
    return false;

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetInstrInfo &TII = *STI.getInstrInfo();

  // Collect every i16 candidate op and the single-use, same-block chain edges
  // between them. An edge X -> U exists when X's result is used exactly once and
  // that use is another candidate in the same block (so the late-opt region
  // merger could actually fuse them: it only deletes the STA/SEP/REP/LDA bridge
  // between physically adjacent regions in one block).
  SmallVector<MachineInstr *, 32> Candidates;
  DenseMap<MachineInstr *, MachineInstr *> Succ;
  DenseMap<MachineInstr *, SmallVector<MachineInstr *, 2>> Preds;

  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (isNative16Candidate(MI, MRI))
        Candidates.push_back(&MI);

  for (MachineInstr *MI : Candidates) {
    Register Def = MI->getOperand(0).getReg();
    if (!MRI.hasOneNonDBGUse(Def))
      continue;
    MachineInstr &Use = *MRI.use_nodbg_instructions(Def).begin();
    if (Use.getParent() != MI->getParent())
      continue;
    if (!isNative16Candidate(Use, MRI))
      continue;
    Succ[MI] = &Use;
    Preds[&Use].push_back(MI);
  }

  // down(X) = ops from X to the chain tail (inclusive); up(X) = ops from the
  // chain head to X (inclusive). The longest chain through X is up+down-1.
  DenseMap<MachineInstr *, unsigned> Down, Up;
  std::function<unsigned(MachineInstr *)> down = [&](MachineInstr *X) {
    auto It = Down.find(X);
    if (It != Down.end())
      return It->second;
    unsigned Len = 1;
    auto S = Succ.find(X);
    if (S != Succ.end())
      Len += down(S->second);
    Down[X] = Len;
    return Len;
  };
  std::function<unsigned(MachineInstr *)> up = [&](MachineInstr *X) {
    auto It = Up.find(X);
    if (It != Up.end())
      return It->second;
    unsigned Len = 1;
    auto P = Preds.find(X);
    if (P != Preds.end())
      for (MachineInstr *Pred : P->second)
        Len = std::max(Len, 1 + up(Pred));
    Up[X] = Len;
    return Len;
  };

  bool Changed = false;
  for (MachineInstr *MI : Candidates) {
    unsigned ChainLen = up(MI) + down(MI) - 1;
    if (ChainLen < Native16MinChain)
      continue; // Leave plain -> legalizer narrows to a byte-split.
    MI->setDesc(TII.get(native16Opcode(MI->getOpcode())));
    Changed = true;
  }
  return Changed;
}

} // namespace

char MOSNative16Profitability::ID = 0;

INITIALIZE_PASS(MOSNative16Profitability, DEBUG_TYPE,
                "MOS Native 16-bit Accumulator Profitability", false, false)

MachineFunctionPass *llvm::createMOSNative16ProfitabilityPass() {
  return new MOSNative16Profitability();
}
