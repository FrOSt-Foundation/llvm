//===-- DCPU16ISelLowering.cpp - DCPU16 DAG Lowering Implementation  ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the DCPU16TargetLowering class.
//
//===----------------------------------------------------------------------===//

#include "DCPU16.h"
#include "DCPU16ISelLowering.h"
#include "DCPU16MachineFunctionInfo.h"
#include "DCPU16TargetMachine.h"
#include "DCPU16Subtarget.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

DCPU16TargetLowering::DCPU16TargetLowering(DCPU16TargetMachine &tm) :
  TargetLowering(tm),
  Subtarget(*tm.getSubtargetImpl()), TM(tm) {

  TD = tm.getDataLayout();

  // Set up the register classes.
  addRegisterClass(MVT::i16, &DCPU16::GR16RegClass);
  addRegisterClass(MVT::i16, &DCPU16::GEXR16RegClass);

  // Compute derived properties from the register classes
  computeRegisterProperties(Subtarget.getRegisterInfo());

  // Provide all sorts of operation actions

  // Division is inexpensive
  // setIntDivIsCheap(true); // It seems like you have to overload a function now? Not sure.
  // setPow2DivIsCheap(true); // FIXME: No such thing as Pow2DivIsCheap in TargetLowering.h?

  setStackPointerRegisterToSaveRestore(DCPU16::SP);
  setBooleanContents(ZeroOrOneBooleanContent);
  setBooleanVectorContents(ZeroOrOneBooleanContent); // FIXME: Is this correct?

  for (MVT VT : MVT::integer_valuetypes()) {
    setLoadExtAction(ISD::EXTLOAD,  VT, MVT::i1,  Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i1,  Promote);
    setLoadExtAction(ISD::ZEXTLOAD, VT, MVT::i1,  Promote);
    setLoadExtAction(ISD::SEXTLOAD, VT, MVT::i16, Expand);
  }

  setOperationAction(ISD::ROTL,             MVT::i16,   Custom);
  setOperationAction(ISD::ROTR,             MVT::i16,   Custom);
  setOperationAction(ISD::BSWAP,            MVT::i16,   Expand);
  setOperationAction(ISD::GlobalAddress,    MVT::i16,   Custom);
  setOperationAction(ISD::ExternalSymbol,   MVT::i16,   Custom);
  setOperationAction(ISD::BlockAddress,     MVT::i16,   Custom);
  setOperationAction(ISD::BR_JT,            MVT::Other, Expand);
  setOperationAction(ISD::JumpTable,        MVT::i16,   Custom);
  setOperationAction(ISD::BR_CC,            MVT::i16,   Custom);
  setOperationAction(ISD::BRCOND,           MVT::Other, Expand);
  setOperationAction(ISD::SETCC,            MVT::i16,   Expand);
  setOperationAction(ISD::SELECT,           MVT::i16,   Expand);
  setOperationAction(ISD::SELECT_CC,        MVT::i16,   Custom);
  setOperationAction(ISD::SIGN_EXTEND,      MVT::i16,   Custom);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i16, Expand);

  setOperationAction(ISD::CTTZ,             MVT::i16,   Expand);
  setOperationAction(ISD::CTTZ_ZERO_UNDEF,  MVT::i16,   Expand);
  setOperationAction(ISD::CTLZ,             MVT::i16,   Expand);
  setOperationAction(ISD::CTLZ_ZERO_UNDEF,  MVT::i16,   Expand);
  setOperationAction(ISD::CTPOP,            MVT::i16,   Expand);

  setOperationAction(ISD::SHL_PARTS,        MVT::i16,   Expand);
  setOperationAction(ISD::SRL_PARTS,        MVT::i16,   Expand);
  setOperationAction(ISD::SRA_PARTS,        MVT::i16,   Expand);

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1,   Expand);

  // FIXME: Implement efficiently multiplication by a constant
  setOperationAction(ISD::MULHS,            MVT::i16,   Expand);
  setOperationAction(ISD::MULHU,            MVT::i16,   Expand);
  setOperationAction(ISD::SMUL_LOHI,        MVT::i16,   Custom);
  setOperationAction(ISD::UMUL_LOHI,        MVT::i16,   Custom);

  setOperationAction(ISD::UDIVREM,          MVT::i16,   Expand);
  setOperationAction(ISD::SDIVREM,          MVT::i16,   Expand);

  setMinFunctionAlignment(1);
  setPrefFunctionAlignment(1);
}

SDValue DCPU16TargetLowering::LowerOperation(SDValue Op,
                                             SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::GlobalAddress:    return LowerGlobalAddress(Op, DAG);
  case ISD::BlockAddress:     return LowerBlockAddress(Op, DAG);
  case ISD::ExternalSymbol:   return LowerExternalSymbol(Op, DAG);
  case ISD::BR_CC:            return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:        return LowerSELECT_CC(Op, DAG);
  case ISD::SIGN_EXTEND:      return LowerSIGN_EXTEND(Op, DAG);
  case ISD::RETURNADDR:       return LowerRETURNADDR(Op, DAG);
  case ISD::FRAMEADDR:        return LowerFRAMEADDR(Op, DAG);
  case ISD::ROTL:             return LowerROT(Op, DAG, true);
  case ISD::ROTR:             return LowerROT(Op, DAG, false);
  case ISD::SMUL_LOHI:        return LowerMUL_LOHI(Op, DAG, true);
  case ISD::UMUL_LOHI:        return LowerMUL_LOHI(Op, DAG, false);
  case ISD::JumpTable:        return LowerJumpTable(Op, DAG);
  default:
    llvm_unreachable("unimplemented operand");
  }
}

//===----------------------------------------------------------------------===//
//                       DCPU16 Inline Assembly Support
//===----------------------------------------------------------------------===//

/// getConstraintType - Given a constraint letter, return the type of
/// constraint it is for this target.
TargetLowering::ConstraintType
DCPU16TargetLowering::getConstraintType(const std::string &Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return C_RegisterClass;
    default:
      break;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass*>
DCPU16TargetLowering::getRegForInlineAsmConstraint(
  const TargetRegisterInfo *RI,
  const StringRef Constraint, MVT VT) const {

  if (Constraint.size() == 1) {
    // GCC Constraint Letters
    switch (Constraint[0]) {
    default: break;
    case 'r':   // GENERAL_REGS
      return std::make_pair(0U, &DCPU16::GR16RegClass);
    }
  }

  return TargetLowering::getRegForInlineAsmConstraint(RI, Constraint, VT);
}

//===----------------------------------------------------------------------===//
//                      Calling Convention Implementation
//===----------------------------------------------------------------------===//

#include "DCPU16GenCallingConv.inc"

SDValue
DCPU16TargetLowering::LowerFormalArguments(SDValue Chain,
                                           CallingConv::ID CallConv,
                                           bool isVarArg,
                                           const SmallVectorImpl<ISD::InputArg>
                                             &Ins,
                                           const SDLoc &dl,
                                           SelectionDAG &DAG,
                                           SmallVectorImpl<SDValue> &InVals)
                                             const {

  switch (CallConv) {
  default:
    llvm_unreachable("Unsupported calling convention");
  case CallingConv::C:
  case CallingConv::Fast:
    return LowerCCCArguments(Chain, CallConv, isVarArg, Ins, dl, DAG, InVals);
  case CallingConv::DCPU16_INTR: {
    if (Ins.size() != 1 || Ins[0].VT != MVT::i16)
      report_fatal_error("Interrupt handlers take exactly one integer or unsigned argument");

    // Copy to virtual register from A
    MachineRegisterInfo &RegInfo = DAG.getMachineFunction().getRegInfo();
    unsigned VReg =
      RegInfo.createVirtualRegister(&DCPU16::GR16RegClass);
    RegInfo.addLiveIn(DCPU16::A, VReg);
    SDValue ArgValue = DAG.getCopyFromReg(Chain, dl, VReg, MVT::i16);
    InVals.push_back(ArgValue);

    return Chain;
  }
  }
}

SDValue
DCPU16TargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG                     = CLI.DAG;
  SDLoc &dl                             = CLI.DL;
  SmallVector<ISD::OutputArg, 32> &Outs = CLI.Outs;
  SmallVector<SDValue, 32> &OutVals     = CLI.OutVals;
  SmallVector<ISD::InputArg, 32> &Ins   = CLI.Ins;
  SDValue Chain                         = CLI.Chain;
  SDValue Callee                        = CLI.Callee;
  bool &isTailCall                      = CLI.IsTailCall;
  CallingConv::ID CallConv              = CLI.CallConv;
  bool isVarArg                         = CLI.IsVarArg;

  // DCPU16 target does not yet support tail call optimization.
  isTailCall = false;

  switch (CallConv) {
  default:
    llvm_unreachable("Unsupported calling convention");
  case CallingConv::Fast:
  case CallingConv::C:
    return LowerCCCCallTo(Chain, Callee, CallConv, isVarArg, isTailCall,
                          Outs, OutVals, Ins, dl, DAG, InVals);
  case CallingConv::DCPU16_INTR:
    report_fatal_error("ISRs cannot be called directly");
  }
}

/// LowerCCCArguments - transform physical registers into virtual registers and
/// generate load operations for arguments places on the stack.
// FIXME: struct return stuff
// FIXME: varargs
SDValue
DCPU16TargetLowering::LowerCCCArguments(SDValue Chain,
                                        CallingConv::ID CallConv,
                                        bool isVarArg,
                                        const SmallVectorImpl<ISD::InputArg>
                                          &Ins,
                                        const SDLoc &dl,
                                        SelectionDAG &DAG,
                                        SmallVectorImpl<SDValue> &InVals)
                                          const {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(), ArgLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_DCPU16);

  assert(!isVarArg && "Varargs not supported yet");

  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];
    if (VA.isRegLoc()) {
      // Arguments passed in registers
      EVT RegVT = VA.getLocVT();
      switch (RegVT.getSimpleVT().SimpleTy) {
      default:
        {
#ifndef NDEBUG
          errs() << "LowerFormalArguments Unhandled argument type: "
               << RegVT.getSimpleVT().SimpleTy << "\n";
#endif
          llvm_unreachable(0);
        }
      case MVT::i16:
        unsigned VReg =
          RegInfo.createVirtualRegister(&DCPU16::GR16RegClass);
        RegInfo.addLiveIn(VA.getLocReg(), VReg);
        SDValue ArgValue = DAG.getCopyFromReg(Chain, dl, VReg, RegVT);

        // If this is an 8-bit value, it is really passed promoted to 16
        // bits. Insert an assert[sz]ext to capture this, then truncate to the
        // right size.
        if (VA.getLocInfo() == CCValAssign::SExt)
          ArgValue = DAG.getNode(ISD::AssertSext, dl, RegVT, ArgValue,
                                 DAG.getValueType(VA.getValVT()));
        else if (VA.getLocInfo() == CCValAssign::ZExt)
          ArgValue = DAG.getNode(ISD::AssertZext, dl, RegVT, ArgValue,
                                 DAG.getValueType(VA.getValVT()));

        if (VA.getLocInfo() != CCValAssign::Full)
          ArgValue = DAG.getNode(ISD::TRUNCATE, dl, VA.getValVT(), ArgValue);

        InVals.push_back(ArgValue);
      }
    } else {
      // Sanity check
      assert(VA.isMemLoc());
      // Load the argument to a virtual register
      unsigned ObjSize = VA.getLocVT().getSizeInBits()/16;
      if (ObjSize != 1) {
        errs() << "LowerFormalArguments Unhandled argument type: "
             << EVT(VA.getLocVT()).getEVTString()
             << "\n";
      }
      // Create the frame index object for this incoming parameter...
      int FI = MFI.CreateFixedObject(ObjSize, VA.getLocMemOffset(), true);

      // Create the SelectionDAG nodes corresponding to a load
      //from this parameter
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i16);
      InVals.push_back(DAG.getLoad(VA.getLocVT(), dl, Chain, FIN,
                                   MachinePointerInfo::getFixedStack(DAG.getMachineFunction(), FI)));
    }
  }

  return Chain;
}

SDValue
DCPU16TargetLowering::LowerReturn(SDValue Chain,
                                  CallingConv::ID CallConv, bool isVarArg,
                                  const SmallVectorImpl<ISD::OutputArg> &Outs,
                                  const SmallVectorImpl<SDValue> &OutVals,
                                  const SDLoc &dl, SelectionDAG &DAG) const {

  // CCValAssign - represent the assignment of the return value to a location
  SmallVector<CCValAssign, 16> RVLocs;

  // ISRs cannot return any value.
  if (CallConv == CallingConv::DCPU16_INTR && !Outs.empty())
    report_fatal_error("Interrupt handlers cannot return any value");

  // CCState - Info about the registers and stack slot.
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
     RVLocs, *DAG.getContext());

  // Analize return values.
  CCInfo.AnalyzeReturn(Outs, RetCC_DCPU16);

  SDValue Flag;

  // Copy the result values into the output registers.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    Chain = DAG.getCopyToReg(Chain, dl, VA.getLocReg(),
                             OutVals[i], Flag);

    // Guarantee that all emitted copies are stuck together,
    // avoiding something bad.
    Flag = Chain.getValue(1);
  }

  unsigned Opc = (CallConv == CallingConv::DCPU16_INTR ?
                  DCPU16ISD::RETI_FLAG : DCPU16ISD::RET_FLAG);

  if (Flag.getNode())
    return DAG.getNode(Opc, dl, MVT::Other, Chain, Flag);

  // Return Void
  return DAG.getNode(Opc, dl, MVT::Other, Chain);
}

/// LowerCCCCallTo - functions arguments are copied from virtual regs to
/// (physical regs)/(stack frame), CALLSEQ_START and CALLSEQ_END are emitted.
/// TODO: sret.
SDValue
DCPU16TargetLowering::LowerCCCCallTo(SDValue Chain, SDValue Callee,
                                     CallingConv::ID CallConv, bool isVarArg,
                                     bool isTailCall,
                                     const SmallVectorImpl<ISD::OutputArg>
                                       &Outs,
                                     const SmallVectorImpl<SDValue> &OutVals,
                                     const SmallVectorImpl<ISD::InputArg> &Ins,
                                     const SDLoc &dl, SelectionDAG &DAG,
                                     SmallVectorImpl<SDValue> &InVals) const {
  // Analyze operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
     ArgLocs, *DAG.getContext());

  CCInfo.AnalyzeCallOperands(Outs, CC_DCPU16);

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = CCInfo.getNextStackOffset();

  auto PtrVT = getPointerTy(DAG.getDataLayout());
  Chain = DAG.getCALLSEQ_START(Chain, DAG.getConstant(NumBytes, dl, PtrVT, true), dl);

  SmallVector<std::pair<unsigned, SDValue>, 4> RegsToPass;
  SmallVector<SDValue, 12> MemOpChains;
  SDValue StackPtr;

  // Walk the register/memloc assignments, inserting copies/loads.
  for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
    CCValAssign &VA = ArgLocs[i];

    SDValue Arg = OutVals[i];

    // Promote the value if needed.
    switch (VA.getLocInfo()) {
      default: llvm_unreachable("Unknown loc info!");
      case CCValAssign::Full: break;
      case CCValAssign::SExt:
        Arg = DAG.getNode(ISD::SIGN_EXTEND, dl, VA.getLocVT(), Arg);
        break;
      case CCValAssign::ZExt:
        Arg = DAG.getNode(ISD::ZERO_EXTEND, dl, VA.getLocVT(), Arg);
        break;
      case CCValAssign::AExt:
        Arg = DAG.getNode(ISD::ANY_EXTEND, dl, VA.getLocVT(), Arg);
        break;
    }

    // Arguments that can be passed on register must be kept at RegsToPass
    // vector
    if (VA.isRegLoc()) {
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
    } else {
      assert(VA.isMemLoc());

      if (StackPtr.getNode() == 0)
        StackPtr = DAG.getCopyFromReg(Chain, dl, DCPU16::SP, PtrVT);

      SDValue PtrOff = DAG.getNode(ISD::ADD, dl, PtrVT, StackPtr,
                                   DAG.getIntPtrConstant(VA.getLocMemOffset(), dl));


      MemOpChains.push_back(DAG.getStore(Chain, dl, Arg, PtrOff,
                                         MachinePointerInfo()));
    }
  }

  // Transform all store nodes into one single node because all store nodes are
  // independent of each other.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, MemOpChains);

  // Build a sequence of copy-to-reg nodes chained together with token chain and
  // flag operands which copy the outgoing args into registers.  The InFlag is
  // necessary since all emitted instructions must be stuck together.
  SDValue InFlag;
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i) {
    Chain = DAG.getCopyToReg(Chain, dl, RegsToPass[i].first,
                             RegsToPass[i].second, InFlag);
    InFlag = Chain.getValue(1);
  }

  // If the callee is a GlobalAddress node (quite common, every direct call is)
  // turn it into a TargetGlobalAddress node so that legalize doesn't hack it.
  // Likewise ExternalSymbol -> TargetExternalSymbol.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), dl, MVT::i16);
  else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i16);

  // Returns a chain & a flag for retval copy to use.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (unsigned i = 0, e = RegsToPass.size(); i != e; ++i)
    Ops.push_back(DAG.getRegister(RegsToPass[i].first,
                                  RegsToPass[i].second.getValueType()));

  // Add a register mask operand representing the call-preserved registers.
  const TargetRegisterInfo *TRI = getTargetMachine().getSubtargetImpl(*DAG.getMachineFunction().getFunction())->getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(DAG.getMachineFunction(), CallConv);
  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  if (InFlag.getNode())
    Ops.push_back(InFlag);

  Chain = DAG.getNode(DCPU16ISD::CALL, dl, NodeTys, Ops);
  InFlag = Chain.getValue(1);

  // Create the CALLSEQ_END node.
  Chain = DAG.getCALLSEQ_END(Chain,
                             DAG.getConstant(NumBytes, dl, PtrVT, true),
                             DAG.getConstant(0, dl, PtrVT, true),
                             InFlag, dl);
  InFlag = Chain.getValue(1);

  // Handle result values, copying them out of physregs into vregs that we
  // return.
  return LowerCallResult(Chain, InFlag, CallConv, isVarArg, Ins, dl,
                         DAG, InVals);
}

/// LowerCallResult - Lower the result values of a call into the
/// appropriate copies out of appropriate physical registers.
///
SDValue
DCPU16TargetLowering::LowerCallResult(SDValue Chain, SDValue InFlag,
                                      CallingConv::ID CallConv, bool isVarArg,
                                      const SmallVectorImpl<ISD::InputArg> &Ins,
                                      const SDLoc &dl, SelectionDAG &DAG,
                                      SmallVectorImpl<SDValue> &InVals) const {

  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, isVarArg, DAG.getMachineFunction(),
     RVLocs, *DAG.getContext());

  CCInfo.AnalyzeCallResult(Ins, RetCC_DCPU16);

  // Copy all of the result registers out of their specified physreg.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    Chain = DAG.getCopyFromReg(Chain, dl, RVLocs[i].getLocReg(),
                               RVLocs[i].getValVT(), InFlag).getValue(1);
    InFlag = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }

  return Chain;
}

SDValue DCPU16TargetLowering::LowerGlobalAddress(SDValue Op,
                                                 SelectionDAG &DAG) const {
  const GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();
  int64_t Offset = cast<GlobalAddressSDNode>(Op)->getOffset();

  auto PtrVT = getPointerTy(DAG.getDataLayout());

  // Create the TargetGlobalAddress node, folding in the constant offset.
  SDValue Result = DAG.getTargetGlobalAddress(GV, SDLoc(Op),
                                              PtrVT, Offset);
  return DAG.getNode(DCPU16ISD::Wrapper, SDLoc(Op),
                     PtrVT, Result);
}

SDValue DCPU16TargetLowering::LowerExternalSymbol(SDValue Op,
                                                  SelectionDAG &DAG) const {
  SDLoc dl = SDLoc(Op);
  auto PtrVT = getPointerTy(DAG.getDataLayout());

  const char *Sym = cast<ExternalSymbolSDNode>(Op)->getSymbol();
  SDValue Result = DAG.getTargetExternalSymbol(Sym, PtrVT);

  return DAG.getNode(DCPU16ISD::Wrapper, dl, PtrVT, Result);
}

SDValue DCPU16TargetLowering::LowerBlockAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  SDLoc dl = SDLoc(Op);
  auto PtrVT = getPointerTy(DAG.getDataLayout());

  const BlockAddress *BA = cast<BlockAddressSDNode>(Op)->getBlockAddress();
  SDValue Result = DAG.getBlockAddress(BA, PtrVT, /*isTarget=*/true);

  return DAG.getNode(DCPU16ISD::Wrapper, dl, PtrVT, Result);
}

static bool NeedsAdditionalEqualityCC(ISD::CondCode CC,
                                      ISD::CondCode *simpleCC,
                                      ISD::CondCode *reverseCC) {
  if (simpleCC)
    *simpleCC = CC;
  if (reverseCC)
    *reverseCC = CC;
  switch (CC) {
  default: break;
  case ISD::SETEQ:
  case ISD::SETNE:
  case ISD::SETLT:
  case ISD::SETULT:
  case ISD::SETUGT:
  case ISD::SETGT:
    return false;
  case ISD::SETUGE:
    if (simpleCC)
      *simpleCC = ISD::SETUGT;
    if (reverseCC)
      *reverseCC = ISD::SETULT;
    return true;
  case ISD::SETULE:
    if (simpleCC)
      *simpleCC = ISD::SETULT;
    if (reverseCC)
      *reverseCC = ISD::SETUGT;
    return true;
  case ISD::SETGE:
    if (simpleCC)
      *simpleCC = ISD::SETGT;
    if (reverseCC)
      *reverseCC = ISD::SETLT;
    return true;
  case ISD::SETLE:
    if (simpleCC)
      *simpleCC = ISD::SETLT;
    if (reverseCC)
      *reverseCC = ISD::SETGT;
    return true;
  }
  llvm_unreachable("Unsupported or invalid integer condition!");
  return false;
}

static DCPU16CC::CondCodes GetSimpleCC(ISD::CondCode CC) {
  switch (CC) {
  default: llvm_unreachable("Invalid integer condition!");
  case ISD::SETGE:
  case ISD::SETLE:
  case ISD::SETUGE:
  case ISD::SETULE:
    llvm_unreachable("CC requires additional equality test - not simple!");
  case ISD::SETEQ:
    return DCPU16CC::COND_E;
  case ISD::SETNE:
    return DCPU16CC::COND_NE;
  case ISD::SETULT:
    return DCPU16CC::COND_L;
  case ISD::SETUGT:
    return DCPU16CC::COND_G;
  case ISD::SETLT:
    return DCPU16CC::COND_U;
  case ISD::SETGT:
    return DCPU16CC::COND_A;
  }
}

SDValue DCPU16TargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS   = Op.getOperand(2);
  SDValue RHS   = Op.getOperand(3);
  SDValue Dest  = Op.getOperand(4);
  SDLoc   dl    = SDLoc(Op);

  ISD::CondCode nonEqualCC;
  if (NeedsAdditionalEqualityCC(CC, &nonEqualCC, NULL)) {
    SDValue eqCC = DAG.getConstant(DCPU16CC::COND_E, dl, MVT::i16);
    Chain = DAG.getNode(DCPU16ISD::BR_CC, dl, Op.getValueType(),
                        Chain, eqCC, LHS, RHS, Dest);
  }

  DCPU16CC::CondCodes simpleCC = GetSimpleCC(nonEqualCC);
  return DAG.getNode(DCPU16ISD::BR_CC, dl, Op.getValueType(),
                     Chain, DAG.getConstant(simpleCC, dl, MVT::i16), LHS, RHS, Dest);
}

SDValue DCPU16TargetLowering::LowerSELECT_CC(SDValue Op,
                                             SelectionDAG &DAG) const {
  SDValue LHS    = Op.getOperand(0);
  SDValue RHS    = Op.getOperand(1);
  SDValue TrueV  = Op.getOperand(2);
  SDValue FalseV = Op.getOperand(3);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(4))->get();
  SDLoc dl       = SDLoc(Op);

  ISD::CondCode reverseCC;
  if (NeedsAdditionalEqualityCC(CC, NULL, &reverseCC)) {
    // This makes sure we only need one SELECT_CC node.
    std::swap(TrueV, FalseV);
  }
  DCPU16CC::CondCodes simpleCC = GetSimpleCC(reverseCC);

  SDVTList VTs = DAG.getVTList(Op.getValueType());
  SmallVector<SDValue, 5> Ops;
  Ops.push_back(DAG.getConstant(simpleCC, dl, MVT::i16));
  Ops.push_back(LHS);
  Ops.push_back(RHS);
  Ops.push_back(TrueV);
  Ops.push_back(FalseV);

  return DAG.getNode(DCPU16ISD::SELECT_CC, dl, VTs, Ops);
}

SDValue DCPU16TargetLowering::LowerSIGN_EXTEND(SDValue Op,
                                               SelectionDAG &DAG) const {
  SDValue Val = Op.getOperand(0);
  EVT VT      = Op.getValueType();
  SDLoc dl    = SDLoc(Op);

  assert(VT == MVT::i16 && "Only support i16 for now!");

  return DAG.getNode(ISD::SIGN_EXTEND_INREG, dl, VT,
                     DAG.getNode(ISD::ANY_EXTEND, dl, VT, Val),
                     DAG.getValueType(Val.getValueType()));
}

SDValue
DCPU16TargetLowering::getReturnAddressFrameIndex(SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  DCPU16MachineFunctionInfo *FuncInfo = MF.getInfo<DCPU16MachineFunctionInfo>();
  int ReturnAddrIndex = FuncInfo->getRAIndex();

  if (ReturnAddrIndex == 0) {
    // Set up a frame object for the return address.
    ReturnAddrIndex = MF.getFrameInfo().CreateFixedObject(1, -1, true);
    FuncInfo->setRAIndex(ReturnAddrIndex);
  }

  return DAG.getFrameIndex(ReturnAddrIndex, getPointerTy(DAG.getDataLayout()));
}

SDValue DCPU16TargetLowering::LowerRETURNADDR(SDValue Op,
                                              SelectionDAG &DAG) const {
  MachineFrameInfo &MFI = DAG.getMachineFunction().getFrameInfo();
  MFI.setReturnAddressIsTaken(true);

  unsigned Depth = cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue();
  SDLoc dl = SDLoc(Op);
  auto PtrVT = getPointerTy(DAG.getDataLayout());

  if (Depth > 0) {
    SDValue FrameAddr = LowerFRAMEADDR(Op, DAG);
    SDValue Offset =
      DAG.getConstant(TD->getPointerSize(), dl, MVT::i16);
    return DAG.getLoad(PtrVT, dl, DAG.getEntryNode(),
                       DAG.getNode(ISD::ADD, dl, PtrVT,
                                   FrameAddr, Offset),
                       MachinePointerInfo());
  }

  // Just load the return address.
  SDValue RetAddrFI = getReturnAddressFrameIndex(DAG);
  return DAG.getLoad(PtrVT, dl, DAG.getEntryNode(),
                     RetAddrFI, MachinePointerInfo());
}

SDValue DCPU16TargetLowering::LowerFRAMEADDR(SDValue Op,
                                             SelectionDAG &DAG) const {
  MachineFrameInfo &MFI = DAG.getMachineFunction().getFrameInfo();
  MFI.setFrameAddressIsTaken(true);

  EVT VT = Op.getValueType();
  SDLoc dl = SDLoc(Op);  // FIXME probably not meaningful
  unsigned Depth = cast<ConstantSDNode>(Op.getOperand(0))->getZExtValue();
  SDValue FrameAddr = DAG.getCopyFromReg(DAG.getEntryNode(), dl,
                                         DCPU16::J, VT);
  while (Depth--)
    FrameAddr = DAG.getLoad(VT, dl, DAG.getEntryNode(), FrameAddr,
                            MachinePointerInfo());
  return FrameAddr;
}

SDValue DCPU16TargetLowering::LowerROT(SDValue Op,
                                       SelectionDAG &DAG,
                                       bool IsLeft) const {
  EVT VT = Op.getValueType();
  SDLoc dl = SDLoc(Op);

  SDValue LHS    = Op.getOperand(0);
  SDValue RHS    = Op.getOperand(1);

  unsigned Opc = IsLeft ? ISD::SHL : ISD::SRL;
  SDVTList VTs = DAG.getVTList(VT, MVT::Glue);
  SDValue Ops[] = {LHS, RHS};
  SDValue ShiftNode = DAG.getNode(Opc, dl, VTs, Ops);
  SDValue Ex = DAG.getCopyFromReg(DAG.getEntryNode(), dl, DCPU16::EX, VT,
                                  ShiftNode.getValue(1));

  SDVTList VTs2 = DAG.getVTList(VT);
  SDValue Ops2[] = {ShiftNode, Ex};
  return DAG.getNode(ISD::OR, dl, VTs2, Ops2);
}

SDValue DCPU16TargetLowering::LowerMUL_LOHI(SDValue Op,
                                            SelectionDAG &DAG,
                                            bool Signed) const {
  SDValue Chain = DAG.getEntryNode();
  EVT VT = Op.getValueType();
  SDLoc dl = SDLoc(Op);

  SDValue LHS    = Op.getOperand(0);
  SDValue RHS    = Op.getOperand(1);

  SDVTList VTs = DAG.getVTList(VT, MVT::Other, MVT::Glue);
  SDValue Ops[] = {Chain, LHS, RHS};
  SDValue Lo = DAG.getNode(Signed ? DCPU16ISD::SMUL : DCPU16ISD::UMUL,
                           dl, VTs, Ops);
  SDValue Hi = DAG.getCopyFromReg(Lo.getValue(1), dl, DCPU16::EX, VT, Lo.getValue(2));

  SDValue Ops2[] = {Lo, Hi};
  return DAG.getMergeValues(Ops2, dl);
}

SDValue DCPU16TargetLowering::LowerJumpTable(SDValue Op,
                                             SelectionDAG &DAG) const {
  JumpTableSDNode *JT = cast<JumpTableSDNode>(Op);
  SDLoc dl = SDLoc(Op);

  SDValue TJT = DAG.getTargetJumpTable(JT->getIndex(), MVT::i16);
  return DAG.getNode(DCPU16ISD::Wrapper, dl, MVT::i16, TJT);
}

const char *DCPU16TargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  default: return NULL;
  case DCPU16ISD::RET_FLAG:           return "DCPU16ISD::RET_FLAG";
  case DCPU16ISD::RETI_FLAG:          return "DCPU16ISD::RETI_FLAG";
  case DCPU16ISD::CALL:               return "DCPU16ISD::CALL";
  case DCPU16ISD::Wrapper:            return "DCPU16ISD::Wrapper";
  case DCPU16ISD::BR_CC:              return "DCPU16ISD::BR_CC";
  case DCPU16ISD::SELECT_CC:          return "DCPU16ISD::SELECT_CC";
  case DCPU16ISD::SMUL:               return "DCPU16ISD::SMUL";
  case DCPU16ISD::UMUL:               return "DCPU16ISD::UMUL";
  }
}

bool DCPU16TargetLowering::isIntDivCheap(EVT VT, AttributeSet Attr) const {
  return true;
}