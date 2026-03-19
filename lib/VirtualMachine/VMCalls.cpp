#include "VirtualMachine/VMCalls.h"
#include "VirtualMachine/VirtualMachine.h"
#include "VirtualMachineCompilerProtocol.h"

namespace vmcalls {
#define VM ((VirtualMachine *)(RT[State::VMPtr]))
void onJIT(void **RT, u32 FIdx) { VM->onJIT(FIdx); }
void onInterp(void **RT, u32 FIdx) { VM->onInterp(FIdx); }
void onFn(void **RT, u32 FIdx) { VM->onFn(FIdx); }
void onStartUp(void **RT, u32 FIdx) { VM->onStartUp(FIdx); }

void onStackOp(void **RT, u32 Op, u32 Ty, u64 Addr) {
  VM->onStackOp(Op, Ty, Addr);
}

void onInstruction(void **RT, u32 LineNumber) { VM->onInstruction(LineNumber); }

void onMemoryOp(void **RT, u32 Op, u32 Ty, u64 Addr, u64 Offset, u64 Ptr) {
  VM->onMemoryOp(Op, Ty, Addr, Offset, Ptr);
}

void onCall(void **RT, u32 FIdx) { VM->onCall(FIdx); }

void onReturn(void **RT, u32 FIdx) { VM->onReturn(FIdx); }

void onOutOfBoundMemory(void **RT, u32 Line, u32 Ty, u64 Addr, u64 Offset,
                        u64 Ptr) {
  VM->onOutOfBoundMemory(Line, Ty, Addr, Offset, Ptr);
}

void onBranch(void **RT, u32 Line, u32 Cond) { VM->onBranch(Line, Cond); }

void memoryFill(void **RT, u32 Dest, u32 Value, u32 Size) {
  VM->memoryFill(Dest, Value, Size);
}

void memoryCopy(void **RT, u32 Dest, u32 Src, u32 Size) {
  VM->memoryCopy(Dest, Src, Size);
}

u32 memoryGrow(void **RT, u32 Size) { return VM->memoryGrow(Size); }

u32 memorySize(void **RT) { return VM->memorySize(); }

void onString(void **RT, const char *Str) { VM->onString(Str); }

#define WASI_FUNCTION(NAME)                                                    \
  void NAME(void **RT) { VM->NAME(); }
#include "VirtualMachine/WASI.api.def"
#undef WASI_FUNCTION
#undef VM

void noop() {}
} // namespace vmcalls