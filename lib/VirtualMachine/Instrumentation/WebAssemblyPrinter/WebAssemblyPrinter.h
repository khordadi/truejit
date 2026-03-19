#pragma once

#include "Globals.h"
#include "VirtualMachine/Instrumentation/Subscribers.h"

class WebAssemblyPrinter final : public InstructionSubscriber,
                                 public StackSubscriber,
                                 public MemoryOperationSubscriber {
public:
  WebAssemblyPrinter(std::vector<std::string> &WatFileLines,
                     const std::vector<std::string> &FuncsNames);
  void update(const InstructionSubscriber::Event &Event) override;
  void update(const StackSubscriber::Event &Event) override;
  void update(const MemoryOperationSubscriber::Event &Event) override;
  ~WebAssemblyPrinter() override;

private:
  bool PrinterIsOn = true;
  std::vector<std::string> FuncsNames;
  std::vector<std::string> WatFileLines;
};
