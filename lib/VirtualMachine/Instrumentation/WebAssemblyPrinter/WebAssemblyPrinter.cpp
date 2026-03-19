#include "WebAssemblyPrinter.h"

#include <fmt/color.h>
#include <fmt/format.h>

#ifdef ENABLE_STYLE
#define WithStyle(Sty) Sty,
#else
#define WithStyle(Sty)
#endif

WebAssemblyPrinter::WebAssemblyPrinter(
    std::vector<std::string> &WatFileLines,
    const std::vector<std::string> &FuncsNames)
    : FuncsNames(FuncsNames), WatFileLines(std::move(WatFileLines)) {}

void WebAssemblyPrinter::update(const InstructionSubscriber::Event &Event) {
  const auto [Line] = Event;

  auto HeadSty = fg(fmt::color::light_yellow);
  fmt::print(WithStyle(HeadSty) "{}\n", std::string(65, '-'));
  auto Before = 4;
  auto After = 2;
  auto Start = std::max(0, int(Line) - Before);
  auto End = std::min(int(WatFileLines.size()), int(Line) + After);
  auto Selected = fmt::emphasis::bold | fg(fmt::color::yellow);
  auto Normal = fg(fmt::color::light_yellow);
  for (auto i = Start; i < End; ++i)
    if (i == Line - 1)
      fmt::print(WithStyle(Selected) "{:3} >>>{}\n", i + 1, WatFileLines[i]);
    else
      fmt::print(WithStyle(Normal) "{:3}    {}\n", i + 1, WatFileLines[i]);
  std::cout << std::flush;
}
void WebAssemblyPrinter::update(const StackSubscriber::Event &Event) {
  auto Op = Event.Op;
  auto Ty = Event.Ty;
  auto Val = Event.Val;

  auto OpName = GetName(Op);
  auto TypeName = GetName(Ty);
  fmt::text_style Style = fmt::emphasis::bold;
  switch (Op) {
  case StackOperation::StackPush:
    Style |= fg(fmt::color::spring_green);
    break;
  case StackOperation::StackPop:
    Style |= fg(fmt::color::red);
    break;
  }
  fmt::print(WithStyle(Style) "{:6}[{:16x}] {}\n", OpName, Val, TypeName);
}
void WebAssemblyPrinter::update(const MemoryOperationSubscriber::Event &Event) {
  if (not PrinterIsOn)
    return;
  auto Op = Event.Op;
  auto Ty = Event.Ty;
  auto Base = Event.Base;
  auto Offset = Event.Offset;
  auto Addr = Event.Addr;
  const auto LinearMemory = Event.LinearMemory;
  auto LinearMemorySize = Event.LinearMemorySize;

  auto OpName = GetName(Op);
  auto Size = GetSize(Ty);
  auto TypeName = GetName(Ty);

  u64 Value = 0;
  for (int i = 0; i < Size; ++i)
    Value |= (u64)(*(u8 *)(LinearMemory + Base + Offset + i)) << (i * 8);

  auto MemStart = (u64)(LinearMemoryBaseAddress);
  auto MemEnd = (u64)(LinearMemoryBaseAddress) + LinearMemorySize;

  std::string Str =
      fmt::format("{:6}\n"
                  "value [{:16x}]\n"
                  "connect [{:16x}]\n"
                  "addr  [{:16x}] {:x}+{:x}\n"
                  "end   [{:16x}]\n",
                  OpName, Value, MemStart, Addr, Base, Offset, MemEnd);
  fmt::print(fmt::emphasis::bold | fg(fmt::color::cyan), Str);
  auto dumpMemory = [&]() {
    auto RowSize = 16;

    // print column header
    fmt::print(fg(fmt::color::magenta), "{:12}", "");
    for (size_t i = 0; i < RowSize; i++)
      fmt::print(fg(fmt::color::magenta), "{:2x} ", i);
    std::cout << std::endl;

    int BeforeContext = 1;
    int StartRow = int(Base + Offset) / RowSize;
    StartRow = StartRow - BeforeContext;
    StartRow = std::max(StartRow, int(0));

    int AfterContext = 1;
    int EndRow = int(Base + Offset + Size) / RowSize;
    EndRow = EndRow + AfterContext;
    EndRow = std::min(EndRow, (int)(LinearMemorySize / RowSize));
    for (size_t i = StartRow; i <= EndRow; i++) {
      fmt::print(fg(fmt::color::magenta), "0x{:08x}  ", i * RowSize);
      for (size_t j = 0; j < RowSize; j++) {
        auto Index = i * RowSize + j;
        if (Index >= Base + Offset && Index < Base + Offset + Size) {
          fmt::print(fg(fmt::color::cyan), "{:02x} ", LinearMemory[Index]);
        } else {
          fmt::print("{:02x} ", LinearMemory[Index]);
        }
      }

      // print the ascii code
      std::cout << fmt::format("  ");
      for (size_t j = 0; j < RowSize; j++) {
        auto Index = i * RowSize + j;
        if (Index >= Base + Offset && Index < Base + Offset + Size) {
          auto C = LinearMemory[Index];
          if (std::isprint(C))
            fmt::print(fg(fmt::color::cyan), "{}", C);
          else
            fmt::print(fg(fmt::color::cyan), ".");
        } else {
          auto C = LinearMemory[Index];
          if (std::isprint(C))
            fmt::print("{}", C);
          else
            fmt::print(".");
        }
      }
      std::cout << std::endl;
    }
  };
  dumpMemory();
}
WebAssemblyPrinter::~WebAssemblyPrinter() = default;