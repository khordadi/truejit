#pragma once

#include "Globals.h"

namespace vmcalls {
void onJIT(void **RT, u32 FIdx);
void onInterp(void **RT, u32 FIdx);
void onFn(void **RT, u32 FIdx);
void onStartUp(void **RT, u32 FIdx);

void onStackOp(void **RT, u32 Op, u32 Ty, u64 Addr);
void onInstruction(void **RT, u32 LineNumber);
void onMemoryOp(void **RT, u32 Op, u32 Ty, u64 Addr, u64 Offset, u64 Ptr);
void onCall(void **RT, u32 FIdx);
void onReturn(void **RT, u32 FIdx);
void onOutOfBoundMemory(void **RT, u32 Line, u32 Ty, u64 Addr, u64 Offset,
                        u64 Ptr);
void onBranch(void **RT, u32 Line, u32 Cond);

void memoryFill(void **RT, u32 Dest, u32 Value, u32 Size);
void memoryCopy(void **RT, u32 Dest, u32 Src, u32 Size);
u32 memoryGrow(void **RT, u32 Size);
u32 memorySize(void **RT);

void onString(void **RT, const char *Str);

#define WASI_FUNCTION(NAME) void NAME(void **RT);
#include "WASI.api.def"
#undef WASI_FUNCTION

void noop();
} // namespace vmcalls
