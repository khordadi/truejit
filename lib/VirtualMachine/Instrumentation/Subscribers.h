#pragma once
#include "VirtualMachineCompilerProtocol.h"
#include <wabt/opcode.h>

class BranchSubscriber {
public:
  using Event = struct {
    u32 Line;
    u32 Cond;
  };
  virtual void update(const Event &Event) = 0;
  virtual ~BranchSubscriber() = default;
};

class CallSubscriber {
public:
  using Event = struct {
    u32 FIdx;
  };
  virtual void update(const Event &Event) = 0;
  virtual ~CallSubscriber() = default;
};

class ReturnSubscriber {
public:
  using Event = struct {
    u32 FIdx;
  };
  virtual void update(const Event &Event) = 0;
  virtual ~ReturnSubscriber() = default;
};

class InstructionSubscriber {
public:
  using Event = struct {
    u32 Line;
  };
  virtual void update(const Event &Event) = 0;
  virtual ~InstructionSubscriber() = default;
};

class StackSubscriber {
public:
  using Event = struct {
    StackOperation Op;
    ValueType Ty;
    u64 Val;
  };
  virtual void update(const Event &Event) = 0;
  virtual ~StackSubscriber() = default;
};

class MemoryOperationSubscriber {
public:
  using Event = struct {
    MemoryOperation Op;
    ValueType Ty;
    u64 Base;
    u64 Offset;
    u64 Addr;
    const char *LinearMemory;
    u64 LinearMemorySize;
  };
  virtual void update(const Event &Event) = 0;
  virtual ~MemoryOperationSubscriber() = default;
};