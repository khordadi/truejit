#pragma once

#include <string>

///// state vector /////
enum State {
  VMPtr,            // 0
  Globals,          // 1
  LinearMemory,     // 2
  LinearMemorySize, // 3
  MemoryFill,       // 4
  MemoryCopy,       // 5
  MemoryGrow,       // 6
  MemorySize,       // 7
  ActiveFrame,      // 8
  JumpTable,        // 9
  IndirectTable,    // 10
  OnFn,             // 11 (new: for LRU cache)
  OnInstruction,
  OnStackOp,
  OnMemoryOp,
  OnCall,
  OnReturn,
  OnOutOfBoundMemory,
  OnBranch,
  OnString,

  Size
};

///// stack and memory operations /////
enum MemoryOperation {
  MemStore,
  MemLoad,
};

enum StackOperation {
  StackPop,
  StackPush,
};

inline std::string GetName(const MemoryOperation Op) {
  switch (Op) {
  case MemStore:
    return "store";
  case MemLoad:
    return "load";
  default:
    Panic("Unknown memory operation");
  }
}
inline std::string GetName(const StackOperation Op) {
  switch (Op) {
  case StackPop:
    return "pop";
  case StackPush:
    return "push";
  default:
    Panic("Unknown stack operation");
  }
}

///// types /////
enum ValueType {
  U8,
  I8,
  U16,
  I16,
  U32,
  I32,
  U64,
  I64,
  F32,
  F64,
};

inline std::string GetName(const ValueType Ty) {
  switch (Ty) {
  case U8:
    return "u8";
  case I8:
    return "i8";
  case U16:
    return "u16";
  case I16:
    return "i16";
  case U32:
    return "u32";
  case I32:
    return "i32";
  case U64:
    return "u64";
  case I64:
    return "i64";
  case F32:
    return "f32";
  case F64:
    return "f64";
  default:
    Panic("Unknown type");
  }
}
inline u32 GetSize(const ValueType Ty) {
  switch (Ty) {
  case U8:
  case I8:
    return 1;
  case U16:
  case I16:
    return 2;
  case U32:
  case I32:
  case F32:
    return 4;
  case U64:
  case I64:
  case F64:
    return 8;
  default:
    Panic("Unknown type");
  }
}
