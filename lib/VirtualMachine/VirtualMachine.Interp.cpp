// ================== Interpreter ==================

#include "VirtualMachine.h"
#include "Globals.h"
#include "VMCalls.h"

#include <cstdint>
#include <vector>
#include <wabt/cast.h>
#include <wabt/type.h>

using s8 = int8_t;
using u8 = uint8_t;
using s16 = int16_t;
using u16 = uint16_t;
using s32 = int32_t;
using u32 = uint32_t;
using Index = uint32_t;
using s64 = int64_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

struct Value {
  static Value Make(s32);
  static Value Make(u32);
  static Value Make(s64);
  static Value Make(u64);
  static Value Make(f32);
  static Value Make(f64);

  template <typename T> T Get() const;
  template <typename T> void Set(T);

  void SetType(wabt::Type t) { type = t; }
  void CheckType(wabt::Type t) const {
    if (t != type)
      Panic(fmt::format("Type mismatch: expected {}, got {}", t.GetName(),
                        type.GetName()));
  }

  wabt::Type type;

private:
  union {
    u32 i32_;
    u64 i64_;
    f32 f32_;
    f64 f64_;
  };
};

using Values = std::vector<Value>;

inline Value Value::Make(s32 val) {
  Value res;
  res.i32_ = val;
  res.SetType(wabt::Type::I32);
  return res;
}
inline Value Value::Make(u32 val) {
  Value res;
  res.i32_ = val;
  res.SetType(wabt::Type::I32);
  return res;
}
inline Value Value::Make(s64 val) {
  Value res;
  res.i64_ = val;
  res.SetType(wabt::Type::I64);
  return res;
}
inline Value Value::Make(u64 val) {
  Value res;
  res.i64_ = val;
  res.SetType(wabt::Type::I64);
  return res;
}
inline Value Value::Make(f32 val) {
  Value res;
  res.f32_ = val;
  res.SetType(wabt::Type::F32);
  return res;
}
inline Value Value::Make(f64 val) {
  Value res;
  res.f64_ = val;
  res.SetType(wabt::Type::F64);
  return res;
}

template <> inline s8 Value::Get<s8>() const {
  CheckType(wabt::Type::I32);
  return i32_;
}
template <> inline u8 Value::Get<u8>() const {
  CheckType(wabt::Type::I32);
  return i32_;
}
template <> inline s16 Value::Get<s16>() const {
  CheckType(wabt::Type::I32);
  return i32_;
}
template <> inline u16 Value::Get<u16>() const {
  CheckType(wabt::Type::I32);
  return i32_;
}
template <> inline s32 Value::Get<s32>() const {
  CheckType(wabt::Type::I32);
  return i32_;
}
template <> inline u32 Value::Get<u32>() const {
  CheckType(wabt::Type::I32);
  return i32_;
}
template <> inline s64 Value::Get<s64>() const {
  CheckType(wabt::Type::I64);
  return i64_;
}
template <> inline u64 Value::Get<u64>() const {
  CheckType(wabt::Type::I64);
  return i64_;
}
template <> inline f32 Value::Get<f32>() const {
  CheckType(wabt::Type::F32);
  return f32_;
}
template <> inline f64 Value::Get<f64>() const {
  CheckType(wabt::Type::F64);
  return f64_;
}

template <> inline void Value::Set<s32>(s32 val) {
  i32_ = val;
  SetType(wabt::Type::I32);
}
template <> inline void Value::Set<u32>(u32 val) {
  i32_ = val;
  SetType(wabt::Type::I32);
}
template <> inline void Value::Set<s64>(s64 val) {
  i64_ = val;
  SetType(wabt::Type::I64);
}
template <> inline void Value::Set<u64>(u64 val) {
  i64_ = val;
  SetType(wabt::Type::I64);
}
template <> inline void Value::Set<f32>(f32 val) {
  f32_ = val;
  SetType(wabt::Type::F32);
}
template <> inline void Value::Set<f64>(f64 val) {
  f64_ = val;
  SetType(wabt::Type::F64);
}
// clang-format on

// MATH
inline int Clz(unsigned x) { return x ? __builtin_clz(x) : sizeof(x) * 8; }
inline int Clz(unsigned long x) {
  return x ? __builtin_clzl(x) : sizeof(x) * 8;
}
inline int Clz(unsigned long long x) {
  return x ? __builtin_clzll(x) : sizeof(x) * 8;
}

inline int Ctz(unsigned x) { return x ? __builtin_ctz(x) : sizeof(x) * 8; }
inline int Ctz(unsigned long x) {
  return x ? __builtin_ctzl(x) : sizeof(x) * 8;
}
inline int Ctz(unsigned long long x) {
  return x ? __builtin_ctzll(x) : sizeof(x) * 8;
}

inline int Popcount(uint8_t x) { return __builtin_popcount(x); }
inline int Popcount(unsigned x) { return __builtin_popcount(x); }
inline int Popcount(unsigned long x) { return __builtin_popcountl(x); }
inline int Popcount(unsigned long long x) { return __builtin_popcountll(x); }

template <typename T, typename std::enable_if<!std::is_floating_point<T>::value,
                                              int>::type = 0>
bool IsNaN(T val) {
  return false;
}

template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
bool IsNaN(T val) {
  return std::isnan(val);
}

template <typename T, std::enable_if_t<!std::is_floating_point_v<T>, int> = 0>
T CanonNaN(T val) {
  return val;
}

template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
T CanonNaN(T val) {
  if (WABT_UNLIKELY(std::isnan(val))) {
    return std::numeric_limits<f32>::quiet_NaN();
  }
  return val;
}

template <typename T> T ShiftMask(T val) { return val & (sizeof(T) * 8 - 1); }

template <typename T> bool IntEqz(T val) { return val == 0; }
template <typename T> bool Eq(T lhs, T rhs) { return lhs == rhs; }
template <typename T> bool Ne(T lhs, T rhs) { return lhs != rhs; }
template <typename T> bool Lt(T lhs, T rhs) { return lhs < rhs; }
template <typename T> bool Le(T lhs, T rhs) { return lhs <= rhs; }
template <typename T> bool Gt(T lhs, T rhs) { return lhs > rhs; }
template <typename T> bool Ge(T lhs, T rhs) { return lhs >= rhs; }
template <typename T> T IntClz(T val) { return Clz(val); }
template <typename T> T IntCtz(T val) { return Ctz(val); }
template <typename T> T IntPopcnt(T val) { return Popcount(val); }
template <typename T> T IntNot(T val) { return ~val; }
template <typename T> T IntNeg(T val) { return ~val + 1; }
template <typename T> T Add(T lhs, T rhs) { return CanonNaN(lhs + rhs); }
template <typename T> T Sub(T lhs, T rhs) { return CanonNaN(lhs - rhs); }
template <typename T> T IntAnd(T lhs, T rhs) { return lhs & rhs; }
template <typename T> T IntOr(T lhs, T rhs) { return lhs | rhs; }
template <typename T> T IntXor(T lhs, T rhs) { return lhs ^ rhs; }
template <typename T> T IntShl(T lhs, T rhs) { return lhs << ShiftMask(rhs); }
template <typename T> T IntShr(T lhs, T rhs) { return lhs >> ShiftMask(rhs); }
template <typename T> T IntMin(T lhs, T rhs) { return std::min(lhs, rhs); }
template <typename T> T IntMax(T lhs, T rhs) { return std::max(lhs, rhs); }
template <typename T> T IntAndNot(T lhs, T rhs) { return lhs & ~rhs; }
template <typename T> T IntAvgr(T lhs, T rhs) { return (lhs + rhs + 1) / 2; }
template <typename T> T Xchg(T lhs, T rhs) { return rhs; }

// This is a wrapping absolute value function, so a negative number that is not
// representable as a positive number will be unchanged (e.g. abs(-128) = 128).
//
// Note that std::abs() does not have this behavior (e.g. abs(-128) is UB).
// Similarly, using unary minus is also UB.
template <typename T> T IntAbs(T val) {
  static_assert(std::is_unsigned<T>::value, "T must be unsigned.");
  const auto signbit = T(-1) << (sizeof(T) * 8 - 1);
  return (val & signbit) ? ~val + 1 : val;
}

// Because of the integer promotion rules [1], any value of a type T which is
// smaller than `int` will be converted to an `int`, as long as `int` can hold
// any value of type T.
//
// So type `u16` will be promoted to `int`, since all values can be stored in
// an int. Unfortunately, the product of two `u16` values cannot always be
// stored in an `int` (e.g. 65535 * 65535). This triggers an error in UBSan.
//
// As a result, we make sure to promote the type ahead of time for `u16`. Note
// that this isn't a problem for any other unsigned types.
//
// [1];
// https://en.cppreference.com/w/cpp/language/implicit_conversion#Integral_promotion
template <typename T> struct PromoteMul {
  using type = T;
};
template <> struct PromoteMul<u16> {
  using type = u32;
};

template <typename T> T Mul(T lhs, T rhs) {
  using U = typename PromoteMul<T>::type;
  return CanonNaN(U(lhs) * U(rhs));
}

template <typename T> struct Mask {
  using Type = T;
};
template <> struct Mask<f32> {
  using Type = u32;
};
template <> struct Mask<f64> {
  using Type = u64;
};

template <typename T> typename Mask<T>::Type EqMask(T lhs, T rhs) {
  return lhs == rhs ? -1 : 0;
}
template <typename T> typename Mask<T>::Type NeMask(T lhs, T rhs) {
  return lhs != rhs ? -1 : 0;
}
template <typename T> typename Mask<T>::Type LtMask(T lhs, T rhs) {
  return lhs < rhs ? -1 : 0;
}
template <typename T> typename Mask<T>::Type LeMask(T lhs, T rhs) {
  return lhs <= rhs ? -1 : 0;
}
template <typename T> typename Mask<T>::Type GtMask(T lhs, T rhs) {
  return lhs > rhs ? -1 : 0;
}
template <typename T> typename Mask<T>::Type GeMask(T lhs, T rhs) {
  return lhs >= rhs ? -1 : 0;
}

template <typename T> T IntRotl(T lhs, T rhs) {
  return (lhs << ShiftMask(rhs)) | (lhs >> ShiftMask<T>(0 - rhs));
}

template <typename T> T IntRotr(T lhs, T rhs) {
  return (lhs >> ShiftMask(rhs)) | (lhs << ShiftMask<T>(0 - rhs));
}

// i{32,64}.{div,rem}_s are special-cased because they trap when dividing the
// max signed value by -1. The modulo operation on x86 uses the same
// instruction to generate the quotient and the remainder.
template <typename T,
          typename std::enable_if<std::is_signed<T>::value, int>::type = 0>
bool IsNormalDivRem(T lhs, T rhs) {
  return !(lhs == std::numeric_limits<T>::min() && rhs == -1);
}

template <typename T,
          typename std::enable_if<!std::is_signed<T>::value, int>::type = 0>
bool IsNormalDivRem(T lhs, T rhs) {
  return true;
}

template <typename T> T IntDiv(T lhs, T rhs) {
  if (rhs == 0)
    Panic("integer divide by zero");
  if (IsNormalDivRem(lhs, rhs))
    return lhs / rhs;
  else
    Panic("integer overflow");
}

template <typename T> T IntRem(T lhs, T rhs) {
  if (rhs == 0) {
    Panic("integer divide by zero");
  }
  if (IsNormalDivRem(lhs, rhs)) {
    return lhs % rhs;
  } else {
    return 0;
  }
}

template <typename T> T FloatAbs(T val) { return std::abs(val); }

template <typename T> T FloatCopysign(T lhs, T rhs) {
  return std::copysign(lhs, rhs);
}

template <typename T> T FloatNeg(T val) { return -val; }
template <typename T> T FloatCeil(T val) { return CanonNaN(std::ceil(val)); }
template <typename T> T FloatFloor(T val) { return CanonNaN(std::floor(val)); }
template <typename T> T FloatTrunc(T val) { return CanonNaN(std::trunc(val)); }
template <typename T> T FloatNearest(T val) {
  return CanonNaN(std::nearbyint(val));
}
template <typename T> T FloatSqrt(T val) { return CanonNaN(std::sqrt(val)); }

template <typename T> T FloatDiv(T lhs, T rhs) {
  // IEE754 specifies what should happen when dividing a float by zero, but
  // C/C++ says it is undefined behavior.
  if (WABT_UNLIKELY(rhs == 0)) {
    return std::isnan(lhs) || lhs == 0
               ? std::numeric_limits<T>::quiet_NaN()
               : ((std::signbit(lhs) ^ std::signbit(rhs))
                      ? -std::numeric_limits<T>::infinity()
                      : std::numeric_limits<T>::infinity());
  }
  return CanonNaN(lhs / rhs);
}

template <typename T> T FloatMin(T lhs, T rhs) {
  if (WABT_UNLIKELY(std::isnan(lhs) || std::isnan(rhs))) {
    return std::numeric_limits<T>::quiet_NaN();
  } else if (WABT_UNLIKELY(lhs == 0 && rhs == 0)) {
    return std::signbit(lhs) ? lhs : rhs;
  } else {
    return std::min(lhs, rhs);
  }
}

template <typename T> T FloatPMin(T lhs, T rhs) { return std::min(lhs, rhs); }

template <typename T> T FloatMax(T lhs, T rhs) {
  if (WABT_UNLIKELY(std::isnan(lhs) || std::isnan(rhs))) {
    return std::numeric_limits<T>::quiet_NaN();
  } else if (WABT_UNLIKELY(lhs == 0 && rhs == 0)) {
    return std::signbit(lhs) ? rhs : lhs;
  } else {
    return std::max(lhs, rhs);
  }
}

template <typename T> T FloatPMax(T lhs, T rhs) { return std::max(lhs, rhs); }

template <typename R, typename T> bool CanConvert(T val) { return true; }
template <> inline bool CanConvert<s32, f32>(f32 val) {
  return val >= -2147483648.f && val < 2147483648.f;
}
template <> inline bool CanConvert<s32, f64>(f64 val) {
  return val > -2147483649. && val < 2147483648.;
}
template <> inline bool CanConvert<u32, f32>(f32 val) {
  return val > -1.f && val < 4294967296.f;
}
template <> inline bool CanConvert<u32, f64>(f64 val) {
  return val > -1. && val < 4294967296.;
}
template <> inline bool CanConvert<s64, f32>(f32 val) {
  return val >= -9223372036854775808.f && val < 9223372036854775808.f;
}
template <> inline bool CanConvert<s64, f64>(f64 val) {
  return val >= -9223372036854775808. && val < 9223372036854775808.;
}
template <> inline bool CanConvert<u64, f32>(f32 val) {
  return val > -1.f && val < 18446744073709551616.f;
}
template <> inline bool CanConvert<u64, f64>(f64 val) {
  return val > -1. && val < 18446744073709551616.;
}

template <typename R, typename T> R Convert(T val) {
  assert((CanConvert<R, T>(val)));
  return static_cast<R>(val);
}

template <> inline f32 Convert(f64 val) {
  // The WebAssembly rounding mode means that these values (which are > F32_MAX)
  // should be rounded to F32_MAX and not set to infinity. Unfortunately, UBSAN
  // complains that the value is not representable as a float, so we'll special
  // case them.
  const f64 kMin = 3.4028234663852886e38;
  const f64 kMax = 3.4028235677973366e38;
  if (WABT_LIKELY(val >= -kMin && val <= kMin)) {
    return val;
  } else if (WABT_UNLIKELY(val > kMin && val < kMax)) {
    return std::numeric_limits<f32>::max();
  } else if (WABT_UNLIKELY(val > -kMax && val < -kMin)) {
    return -std::numeric_limits<f32>::max();
  } else if (WABT_UNLIKELY(std::isnan(val))) {
    return std::numeric_limits<f32>::quiet_NaN();
  } else {
    return std::copysign(std::numeric_limits<f32>::infinity(), val);
  }
}

template <> inline f32 Convert(u64 val) {
  return wabt_convert_uint64_to_float(val);
}

template <> inline f64 Convert(u64 val) {
  return wabt_convert_uint64_to_double(val);
}

template <> inline f32 Convert(s64 val) {
  return wabt_convert_int64_to_float(val);
}

template <> inline f64 Convert(s64 val) {
  return wabt_convert_int64_to_double(val);
}

template <typename T, int N> T IntExtend(T val) {
  // Hacker's delight 2.6 - sign extension
  auto bit = T{1} << N;
  auto mask = (bit << 1) - 1;
  return ((val & mask) ^ bit) - bit;
}

template <typename R, typename T> R IntTruncSat(T val) {
  if (WABT_UNLIKELY(std::isnan(val))) {
    return 0;
  } else if (WABT_UNLIKELY(!CanConvert<R>(val))) {
    return std::signbit(val) ? std::numeric_limits<R>::min()
                             : std::numeric_limits<R>::max();
  } else {
    return static_cast<R>(val);
  }
}

template <typename T> struct SatPromote;
template <> struct SatPromote<s8> {
  using type = s32;
};
template <> struct SatPromote<s16> {
  using type = s32;
};
template <> struct SatPromote<u8> {
  using type = s32;
};
template <> struct SatPromote<u16> {
  using type = s32;
};

template <typename R, typename T> R Saturate(T val) {
  static_assert(sizeof(R) < sizeof(T), "Incorrect types for Saturate");
  const T min = std::numeric_limits<R>::min();
  const T max = std::numeric_limits<R>::max();
  return val > max ? max : val < min ? min : val;
}

template <typename T, typename U = typename SatPromote<T>::type>
T IntAddSat(T lhs, T rhs) {
  return Saturate<T, U>(lhs + rhs);
}

template <typename T, typename U = typename SatPromote<T>::type>
T IntSubSat(T lhs, T rhs) {
  return Saturate<T, U>(lhs - rhs);
}

template <typename T> T SaturatingRoundingQMul(T lhs, T rhs) {
  constexpr int size_in_bits = sizeof(T) * 8;
  int round_const = 1 << (size_in_bits - 2);
  int64_t product = lhs * rhs;
  product += round_const;
  product >>= (size_in_bits - 1);
  return Saturate<T, int64_t>(product);
}
// MATH

Value Pop(Values &Stack) {
  auto V = Stack.back();
  Stack.pop_back();
  return V;
}
template <typename T> T Pop(Values &Stack) { return Pop(Stack).Get<T>(); }

void Push(Values &Stack, Value V) { Stack.push_back(V); }
template <typename T> void Push(Values &Stack, T V) {
  Push(Stack, Value::Make(V));
}
template <> void Push<bool>(Values &Stack, const bool V) {
  Push(Stack, Value::Make(static_cast<u32>(V ? 1 : 0)));
}

void PrintMemory(const char *LinearMemory, const u32 LinearMemorySize,
                 const u32 Offset, const u32 Size) {}

template <typename LoadTy>
LoadTy *Load(Values &Stack, char *LinearMemory, const u32 Offset) {
  const auto Addr = Pop<u32>(Stack);
  // PrintMemory(LinearMemory, Addr + Offset, sizeof(LoadTy));
  return reinterpret_cast<LoadTy *>(LinearMemory + Addr + Offset);
}

template <typename PushTy, typename LoadTy>
void DoLoad(Values &Stack, char *LinearMemory, const u32 Offset) {
  auto Ptr = Load<LoadTy>(Stack, LinearMemory, Offset);
  Push<PushTy>(Stack, *Ptr);
}

template <typename PopTy, typename StoreTy>
void DoStore(Values &Stack, char *LinearMemory, const u32 Offset) {
  auto Val = Pop<PopTy>(Stack);
  const auto Addr = Pop<u32>(Stack);
  auto Ptr = reinterpret_cast<StoreTy *>(LinearMemory + Addr + Offset);
  *Ptr = Val;
}

template <typename OutTy, typename InTy> using UnopFunc = OutTy(InTy);
template <typename OutTy, typename InTy> using BinopFunc = OutTy(InTy, InTy);

template <typename OutTy, typename InTy>
void DoUnop(Values &Stack, UnopFunc<OutTy, InTy> Fn) {
  Push<OutTy>(Stack, Fn(Pop<InTy>(Stack)));
}
template <typename OutTy, typename InTy>
void DoBinop(Values &Stack, BinopFunc<OutTy, InTy> Fn) {
  auto RHS = Pop<InTy>(Stack);
  auto LHS = Pop<InTy>(Stack);
  Push<OutTy>(Stack, Fn(LHS, RHS));
}

template <typename OutTy, typename InTy> void DoConvert(Values &Stack) {
  Push<OutTy>(Stack, Convert<OutTy>(Pop<InTy>(Stack)));
}

template <typename OutTy, typename InTy> void DoReinterpret(Values &Stack) {
  Push(Stack, wabt::Bitcast<OutTy>(Pop<InTy>(Stack)));
}

static uint8_t Indent = 0;

void PrintFrame(std::vector<char> &ActivationFrame) {
  auto Sty = fg(fmt::color::pink);
  fmt::print("{{ ");
  for (char &Hex : ActivationFrame)
    fmt::print(Sty, "{:02x} ", Hex);
  fmt::print("}}");
  std::cout << std::endl;
}

void VirtualMachine::interpret(const u32 FunctionIndex) {
  // fmt::println("[interp] {}", FunctionIndex);
  const auto FName = M->funcs[FunctionIndex]->name;
  assert(not FName.starts_with("$__imported_wasi_snapshot_preview1") and
         "This is a wasi function!");

  // {
  //   print(fg(fmt::color::green), "{}>>> {}", std::string(Indent++, ' '),
  //   FName);
  //   // print arguments types
  //   fmt::print(" (");
  //   auto &Params = M->funcs[FunctionIndex]->decl.sig.param_types;
  //   for (auto I = 0; I < Params.size(); ++I) {
  //     auto Ty = Params[I];
  //     switch (Ty) {
  //     case wabt::Type::I32:
  //       print(fg(fmt::color::cyan), "i32");
  //       break;
  //     case wabt::Type::I64:
  //       print(fg(fmt::color::cyan), "i64");
  //       break;
  //     case wabt::Type::F32:
  //       print(fg(fmt::color::cyan), "f32");
  //       break;
  //     case wabt::Type::F64:
  //       print(fg(fmt::color::cyan), "f64");
  //       break;
  //     default:
  //       REPORT_ERROR_AND_EXIT("Unsupported type");
  //     }
  //     if (I + 1 < Params.size())
  //       fmt::print(", ");
  //   }
  //   fmt::print(")");
  //   std::cout << std::endl;
  // }

  auto F = M->funcs[FunctionIndex];

  Values Stack;

  auto PrintStack = [&] {
    std::cout << "[";
    for (auto I = 0; I < Stack.size(); ++I) {
      auto V = Stack[I];
      auto TyStyle = fg(fmt::color::cyan);
      auto ValStyle = fg(fmt::color::yellow);
      std::string TyStr;
      std::string ValStr;

      switch (V.type) {

      case wabt::Type::I32:
        TyStr = "i32";
        ValStr = std::to_string(V.Get<u32>());
        break;
      case wabt::Type::I64:
        TyStr = "i64";
        ValStr = std::to_string(V.Get<u64>());
        break;
      case wabt::Type::F32:
        TyStr = "f32";
        ValStr = std::to_string(V.Get<f32>());
        break;
      case wabt::Type::F64:
        TyStr = "f64";
        ValStr = std::to_string(V.Get<f64>());
        break;
      default:
        Panic("Unsupported type");
      }

      fmt::print(TyStyle, "{}", TyStr);
      fmt::print(":");
      fmt::print(ValStyle, "{}", ValStr);
      if (I + 1 < Stack.size()) {
        if (I == F->decl.sig.param_types.size() - 1 or
            I == F->local_types.size() + F->decl.sig.param_types.size() - 1)
          fmt::print(" | ");
        else
          fmt::print(", ");
      }
    }
    fmt::print("]");
    std::cout << std::endl;
  };

  // deserialize values from ActivationFrame to ValueStack (in the beginning of
  // callee)
  auto FrameToParams = [&]() {
    const auto &ParamTys = F->decl.sig.param_types;
    auto Params = Values(ParamTys.size());
    auto Offset = 0;
    for (auto i = 0; i < ParamTys.size(); ++i) {
      switch (ParamTys[i]) {
      case wabt::Type::I32:
        Params[i].Set(
            *reinterpret_cast<u32 *>(ActivationFrame.data() + Offset));
        Params[i].SetType(wabt::Type::I32);
        Offset += 4;
        break;
      case wabt::Type::I64:
        Params[i].Set(
            *reinterpret_cast<u64 *>(ActivationFrame.data() + Offset));
        Params[i].SetType(wabt::Type::I64);
        Offset += 8;
        break;
      case wabt::Type::F32:
        Params[i].Set(
            *reinterpret_cast<f32 *>(ActivationFrame.data() + Offset));
        Params[i].SetType(wabt::Type::F32);
        Offset += 4;
        break;
      case wabt::Type::F64:
        Params[i].Set(
            *reinterpret_cast<f64 *>(ActivationFrame.data() + Offset));
        Params[i].SetType(wabt::Type::F64);
        Offset += 8;
        break;
      default:
        Panic("Unsupported param type");
      }
    }
    return Params;
  };
  Stack = FrameToParams();

  auto GetLocals = [&]() {
    Values Locals;
    for (const auto Ty : F->local_types)
      switch (Ty) {
      case wabt::Type::I32:
        Locals.emplace_back(Value::Make(u32{0}));
        break;
      case wabt::Type::I64:
        Locals.emplace_back(Value::Make(u64{0}));
        break;
      case wabt::Type::F32:
        Locals.emplace_back(Value::Make(f32{0}));
        break;
      case wabt::Type::F64:
        Locals.emplace_back(Value::Make(f64{0}));
        break;
      default:
        Panic("Unsupported local type");
      }
    return Locals;
  };
  auto Locals = GetLocals();
  Stack.insert(Stack.end(), Locals.begin(), Locals.end());

  // serializes values from ValueStack to ActivationFrame (before call)
  auto ParamsToFrame = [&](const wabt::Func *Callee) {
    Values Params;
    for (auto i = 0; i < Callee->decl.sig.param_types.size(); i++)
      Params.push_back(Pop(Stack));
    // reverse the order of Params
    std::ranges::reverse(Params);

    const auto &ParamTys = Callee->decl.sig.param_types;
    auto Offset = 0;
    for (auto i = 0; i < Params.size(); ++i) {
      switch (ParamTys[i]) {
      case wabt::Type::I32:
        *reinterpret_cast<u32 *>(ActivationFrame.data() + Offset) =
            Params[i].Get<u32>();
        Offset += 4;
        break;
      case wabt::Type::I64:
        *reinterpret_cast<u64 *>(ActivationFrame.data() + Offset) =
            Params[i].Get<u64>();
        Offset += 8;
        break;
      case wabt::Type::F32:
        *reinterpret_cast<f32 *>(ActivationFrame.data() + Offset) =
            Params[i].Get<f32>();
        Offset += 4;
        break;
      case wabt::Type::F64:
        *reinterpret_cast<f64 *>(ActivationFrame.data() + Offset) =
            Params[i].Get<f64>();
        Offset += 8;
        break;
      default:
        Panic("Unsupported param type");
      }
    }
  };

  // deserializes values from ActivationFrame to ValueStack (after call)
  auto FrameToReturn = [&](const wabt::Func *Callee) {
    auto &ReturnTys = Callee->decl.sig.result_types;
    if (ReturnTys.empty())
      return Values{};
    assert(ReturnTys.size() == 1 and "Only one return value is supported");
    auto Return = Values(1);
    auto Offset = 0;
    switch (ReturnTys[0]) {
    case wabt::Type::I32:
      Return[0].Set(*reinterpret_cast<u32 *>(ActivationFrame.data() + Offset));
      Return[0].SetType(wabt::Type::I32);
      break;
    case wabt::Type::I64:
      Return[0].Set(*reinterpret_cast<u64 *>(ActivationFrame.data() + Offset));
      Return[0].SetType(wabt::Type::I64);
      break;
    case wabt::Type::F32:
      Return[0].Set(*reinterpret_cast<f32 *>(ActivationFrame.data() + Offset));
      Return[0].SetType(wabt::Type::F32);
      break;
    case wabt::Type::F64:
      Return[0].Set(*reinterpret_cast<f64 *>(ActivationFrame.data() + Offset));
      Return[0].SetType(wabt::Type::F64);
      break;
    default:
      Panic("Unsupported return type");
    }
    return Return;
  };

  std::optional<uint32_t> BranchingTarget = std::nullopt;
  std::function<void(const wabt::Expr &)> Interp = [&](const wabt::Expr &E) {
#ifdef PRINT_INTERP
    PrintStack();
#endif

    u32 Breakpoint = 5625;
    if (E.loc.line == Breakpoint) {
      if (true) {
      }
    }

    auto PrintLine = [&] {
      constexpr auto LineNumSty = fg(fmt::color::cyan);
      fmt::print(LineNumSty, "{:3}", E.loc.line);
      constexpr auto LineSty = fg(fmt::color::gold);
      fmt::print(LineSty, "{}", MLines[E.loc.line - 1]);
      std::cout << std::endl;
    };
#ifdef PRINT_INTERP
    PrintLine();
#endif

    switch (E.type()) {
    case wabt::ExprType::Const: {
      switch (const auto CE = wabt::cast<wabt::ConstExpr>(&E);
              CE->const_.type()) {
      case wabt::Type::I32:
        Push(Stack, Value::Make(CE->const_.u32()));
        break;
      case wabt::Type::I64:
        Push(Stack, Value::Make(CE->const_.u64()));
        break;
      case wabt::Type::F32:
        Push(Stack, Value::Make(std::bit_cast<f32>(CE->const_.f32_bits())));
        break;
      case wabt::Type::F64:
        Push(Stack, Value::Make(std::bit_cast<f64>(CE->const_.f64_bits())));
        break;
      default:
        Panic("Unsupported const type");
      }
      break;
    }
    case wabt::ExprType::LocalGet: {
      auto LGE = wabt::cast<wabt::LocalGetExpr>(&E);
      const auto Idx = LGE->var.index();
      Push(Stack, Stack[Idx]);
      break;
    }
    case wabt::ExprType::LocalSet: {
      auto LSE = wabt::cast<wabt::LocalSetExpr>(&E);
      const auto Idx = LSE->var.index();
      Stack[Idx] = Pop(Stack);
      break;
    }
    case wabt::ExprType::LocalTee: {
      auto LTE = wabt::cast<wabt::LocalTeeExpr>(&E);
      const auto Idx = LTE->var.index();
      Stack[Idx] = Stack.back();
      break;
    }

      // assume that all global variables are u32
    case wabt::ExprType::GlobalGet: {
      auto GGE = wabt::cast<wabt::GlobalGetExpr>(&E);
      const auto Idx = GGE->var.index();
      Push(Stack, Value::Make(u32{Globals[Idx]}));
      break;
    }
    case wabt::ExprType::GlobalSet: {
      auto GSE = wabt::cast<wabt::GlobalSetExpr>(&E);
      const auto Idx = GSE->var.index();
      Globals[Idx] = Pop<u32>(Stack);
      break;
    }

    case wabt::ExprType::Store: {
      const auto SE = wabt::cast<wabt::StoreExpr>(&E);
      auto Align = SE->align; // unused
      auto Offset = SE->offset;

      switch (SE->opcode) {
      case wabt::Opcode::I32Store:
        DoStore<u32, u32>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Store:
        DoStore<u64, u64>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::F32Store:
        DoStore<f32, f32>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::F64Store:
        DoStore<f64, f64>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I32Store8:
        DoStore<u32, u8>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I32Store16:
        DoStore<u32, u16>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Store8:
        DoStore<u64, u8>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Store16:
        DoStore<u64, u16>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Store32:
        DoStore<u64, u32>(Stack, LinearMemory, Offset);
        break;
      default:
        Panic("Unsupported store opcode");
      }
      break;
    }

    case wabt::ExprType::Load: {
      const auto LE = wabt::cast<wabt::LoadExpr>(&E);
      auto Align = LE->align; // unused
      auto Offset = LE->offset;

      switch (LE->opcode) {
      case wabt::Opcode::I32Load:
        DoLoad<u32, u32>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Load:
        DoLoad<u64, u64>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::F32Load:
        DoLoad<f32, f32>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::F64Load:
        DoLoad<f64, f64>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I32Load8S:
        DoLoad<u32, s8>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I32Load8U:
        DoLoad<u32, u8>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I32Load16S:
        DoLoad<u32, s16>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I32Load16U:
        DoLoad<u32, u16>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Load8S:
        DoLoad<u64, s8>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Load8U:
        DoLoad<u64, u8>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Load16S:
        DoLoad<u64, s16>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Load16U:
        DoLoad<u64, u16>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Load32S:
        DoLoad<u64, s32>(Stack, LinearMemory, Offset);
        break;
      case wabt::Opcode::I64Load32U:
        DoLoad<u64, u32>(Stack, LinearMemory, Offset);
        break;
      default:
        Panic("Unsupported load opcode");
      }
      break;
    }

    case wabt::ExprType::MemoryFill: {
      auto MF = wabt::cast<wabt::MemoryFillExpr>(&E);
      auto Size = Pop<u32>(Stack);
      auto Value = Pop<u32>(Stack);
      auto Dest = Pop<u32>(Stack);
      memoryFill(Dest, Value, Size);
      break;
    }
    case wabt::ExprType::MemoryCopy: {
      auto MC = wabt::cast<wabt::MemoryCopyExpr>(&E);
      auto Size = Pop<u32>(Stack);
      auto Src = Pop<u32>(Stack);
      auto Dst = Pop<u32>(Stack);
      memoryCopy(Dst, Src, Size);
      break;
    }
    case wabt::ExprType::MemoryGrow: {
      auto MG = wabt::cast<wabt::MemoryGrowExpr>(&E);
      auto Pages = Pop<u32>(Stack);
      auto Result = memoryGrow(Pages);
      Push(Stack, Result);
      break;
    }
    case wabt::ExprType::MemorySize: {
      auto Result = memorySize();
      Push(Stack, Result);
      break;
    }

    case wabt::ExprType::Block: {
      auto BE = wabt::cast<wabt::BlockExpr>(&E);
      for (auto &InnerE : BE->block.exprs) {
        Interp(InnerE);
        if (BranchingTarget.has_value()) {
          if (BranchingTarget.value() == 0)
            BranchingTarget = std::nullopt;
          else
            BranchingTarget.value()--;
          break;
        }
      }
      break;
    }

    case wabt::ExprType::Loop: {
      const auto LE = wabt::cast<wabt::LoopExpr>(&E);
      bool Looping;
      do {
        Looping = false;
        for (auto &InnerE : LE->block.exprs) {
          Interp(InnerE);
          if (BranchingTarget.has_value()) {    // branch requested
            if (BranchingTarget.value() == 0) { // target is this loop
              BranchingTarget = std::nullopt;   // reset branch target
              Looping = true;                   // loop again
            } else {                            // target is outer block/loop
              BranchingTarget.value()--;        // decrement target
              Looping = false;                  // stop looping
            }
            break;
          }
        }
      } while (Looping);
      break;
    }

    case wabt::ExprType::Br: {
      const auto BRE = wabt::cast<wabt::BrExpr>(&E);
      BranchingTarget = BRE->var.index();
      break;
    }

    case wabt::ExprType::BrIf: {
      const auto BIE = wabt::cast<wabt::BrIfExpr>(&E);
      if (Pop<u32>(Stack))
        BranchingTarget = BIE->var.index();
      break;
    }

    case wabt::ExprType::BrTable: {
      const auto BTE = wabt::cast<wabt::BrTableExpr>(&E);
      if (const auto Target = Pop<u32>(Stack); Target >= BTE->targets.size())
        BranchingTarget = BTE->default_target.index();
      else
        BranchingTarget = BTE->targets[Target].index();
      break;
    }

    case wabt::ExprType::If: {
      const auto IE = wabt::cast<wabt::IfExpr>(&E);
      if (Pop<u32>(Stack)) {
        for (auto &InnerE : IE->true_.exprs) {
          Interp(InnerE);
          if (BranchingTarget.has_value()) {
            if (BranchingTarget.value() == 0)
              BranchingTarget = std::nullopt;
            else
              BranchingTarget.value()--;
            break;
          }
        }
      } else {
        for (auto &InnerE : IE->false_) {
          Interp(InnerE);
          if (BranchingTarget.has_value()) {
            if (BranchingTarget.value() == 0)
              BranchingTarget = std::nullopt;
            else
              BranchingTarget.value()--;
            break;
          }
        }
      }
      break;
    }

    case wabt::ExprType::Select: {
      const auto SE = wabt::cast<wabt::SelectExpr>(&E);
      const auto Cond = Pop<u32>(Stack);
      const auto False = Pop(Stack);
      const auto True = Pop(Stack);
      Push(Stack, Cond ? True : False);
      break;
    }

    case wabt::ExprType::Binary: {
      switch (const auto BE = wabt::cast<wabt::BinaryExpr>(&E); BE->opcode) {
      case wabt::Opcode::I32Add:
        DoBinop(Stack, Add<u32>);
        break;
      case wabt::Opcode::I32Sub:
        DoBinop(Stack, Sub<u32>);
        break;
      case wabt::Opcode::I32Mul:
        DoBinop(Stack, Mul<u32>);
        break;
      case wabt::Opcode::I32DivU:
        DoBinop(Stack, IntDiv<u32>);
        break;
      case wabt::Opcode::I32DivS:
        DoBinop(Stack, IntDiv<s32>);
        break;
      case wabt::Opcode::I32RemU:
        DoBinop(Stack, IntRem<u32>);
        break;
      case wabt::Opcode::I32RemS:
        DoBinop(Stack, IntRem<s32>);
        break;
      case wabt::Opcode::I32And:
        DoBinop(Stack, IntAnd<u32>);
        break;
      case wabt::Opcode::I32Or:
        DoBinop(Stack, IntOr<u32>);
        break;
      case wabt::Opcode::I32Xor:
        DoBinop(Stack, IntXor<u32>);
        break;
      case wabt::Opcode::I32Shl:
        DoBinop(Stack, IntShl<u32>);
        break;
      case wabt::Opcode::I32ShrU:
        DoBinop(Stack, IntShr<u32>);
        break;
      case wabt::Opcode::I32ShrS:
        DoBinop(Stack, IntShr<s32>);
        break;
      case wabt::Opcode::I32Rotl:
        DoBinop(Stack, IntRotl<u32>);
        break;
      case wabt::Opcode::I32Rotr:
        DoBinop(Stack, IntRotr<u32>);
        break;
      case wabt::Opcode::I64Add:
        DoBinop(Stack, Add<u64>);
        break;
      case wabt::Opcode::I64Sub:
        DoBinop(Stack, Sub<u64>);
        break;
      case wabt::Opcode::I64Mul:
        DoBinop(Stack, Mul<u64>);
        break;
      case wabt::Opcode::I64DivS:
        DoBinop(Stack, IntDiv<s64>);
        break;
      case wabt::Opcode::I64DivU:
        DoBinop(Stack, IntDiv<u64>);
        break;
      case wabt::Opcode::I64RemS:
        DoBinop(Stack, IntRem<s64>);
        break;
      case wabt::Opcode::I64RemU:
        DoBinop(Stack, IntRem<u64>);
        break;
      case wabt::Opcode::I64And:
        DoBinop(Stack, IntAnd<u64>);
        break;
      case wabt::Opcode::I64Or:
        DoBinop(Stack, IntOr<u64>);
        break;
      case wabt::Opcode::I64Xor:
        DoBinop(Stack, IntXor<u64>);
        break;
      case wabt::Opcode::I64Shl:
        DoBinop(Stack, IntShl<u64>);
        break;
      case wabt::Opcode::I64ShrU:
        DoBinop(Stack, IntShr<u64>);
        break;
      case wabt::Opcode::I64ShrS:
        DoBinop(Stack, IntShr<s64>);
        break;
      case wabt::Opcode::I64Rotl:
        DoBinop(Stack, IntRotl<u64>);
        break;
      case wabt::Opcode::I64Rotr:
        DoBinop(Stack, IntRotr<u64>);
        break;
      case wabt::Opcode::F32Add:
        DoBinop(Stack, Add<f32>);
        break;
      case wabt::Opcode::F32Sub:
        DoBinop(Stack, Sub<f32>);
        break;
      case wabt::Opcode::F32Div:
        DoBinop(Stack, FloatDiv<f32>);
        break;
      case wabt::Opcode::F32Mul:
        DoBinop(Stack, Mul<f32>);
        break;
      case wabt::Opcode::F32Min:
        DoBinop(Stack, FloatMin<f32>);
        break;
      case wabt::Opcode::F32Max:
        DoBinop(Stack, FloatMax<f32>);
        break;
      case wabt::Opcode::F64Div:
        DoBinop(Stack, FloatDiv<f64>);
        break;
      case wabt::Opcode::F64Mul:
        DoBinop(Stack, Mul<f64>);
        break;
      case wabt::Opcode::F64Add:
        DoBinop(Stack, Add<f64>);
        break;
      case wabt::Opcode::F64Sub:
        DoBinop(Stack, Sub<f64>);
        break;
      case wabt::Opcode::F64Min:
        DoBinop(Stack, FloatMin<f64>);
        break;
      case wabt::Opcode::F64Max:
        DoBinop(Stack, FloatMax<f64>);
        break;
      case wabt::Opcode::F64Copysign:
        DoBinop(Stack, FloatCopysign<f64>);
        break;
      case wabt::Opcode::F32Copysign:
        DoBinop(Stack, FloatCopysign<f32>);
        break;
      default:
        Panic("Unsupported binary opcode");
        // clang-format on
      }
      break;
    }

    case wabt::ExprType::Compare: {
      switch (const auto CE = wabt::cast<wabt::CompareExpr>(&E); CE->opcode) {

      case wabt::Opcode::I32Eq:
        DoBinop(Stack, Eq<u32>);
        break;
      case wabt::Opcode::I32LtU:
        DoBinop(Stack, Lt<u32>);
        break;
      case wabt::Opcode::I32Ne:
        DoBinop(Stack, Ne<u32>);
        break;
      case wabt::Opcode::I32GtU:
        DoBinop(Stack, Gt<u32>);
        break;
      case wabt::Opcode::I32LeU:
        DoBinop(Stack, Le<u32>);
        break;
      case wabt::Opcode::I32GeU:
        DoBinop(Stack, Ge<u32>);
        break;
      case wabt::Opcode::I32GtS:
        DoBinop(Stack, Gt<s32>);
        break;
      case wabt::Opcode::I32LeS:
        DoBinop(Stack, Le<s32>);
        break;
      case wabt::Opcode::I32GeS:
        DoBinop(Stack, Ge<s32>);
        break;
      case wabt::Opcode::I32LtS:
        DoBinop(Stack, Lt<s32>);
        break;
      case wabt::Opcode::I64Eq:
        DoBinop(Stack, Eq<u64>);
        break;
      case wabt::Opcode::I64Ne:
        DoBinop(Stack, Ne<u64>);
        break;
      case wabt::Opcode::I64LtU:
        DoBinop(Stack, Lt<u64>);
        break;
      case wabt::Opcode::I64LeU:
        DoBinop(Stack, Le<u64>);
        break;
      case wabt::Opcode::I64GtU:
        DoBinop(Stack, Gt<u64>);
        break;
      case wabt::Opcode::I64GeU:
        DoBinop(Stack, Ge<u64>);
        break;
      case wabt::Opcode::I64LtS:
        DoBinop(Stack, Lt<s64>);
        break;
      case wabt::Opcode::I64LeS:
        DoBinop(Stack, Le<s64>);
        break;
      case wabt::Opcode::I64GtS:
        DoBinop(Stack, Gt<s64>);
        break;
      case wabt::Opcode::I64GeS:
        DoBinop(Stack, Ge<s64>);
        break;
      case wabt::Opcode::F32Eq:
        DoBinop(Stack, Eq<f32>);
        break;
      case wabt::Opcode::F32Ne:
        DoBinop(Stack, Ne<f32>);
        break;
      case wabt::Opcode::F32Lt:
        DoBinop(Stack, Lt<f32>);
        break;
      case wabt::Opcode::F32Le:
        DoBinop(Stack, Le<f32>);
        break;
      case wabt::Opcode::F32Gt:
        DoBinop(Stack, Gt<f32>);
        break;
      case wabt::Opcode::F32Ge:
        DoBinop(Stack, Ge<f32>);
        break;
      case wabt::Opcode::F64Lt:
        DoBinop(Stack, Lt<f64>);
        break;
      case wabt::Opcode::F64Le:
        DoBinop(Stack, Le<f64>);
        break;
      case wabt::Opcode::F64Gt:
        DoBinop(Stack, Gt<f64>);
        break;
      case wabt::Opcode::F64Ge:
        DoBinop(Stack, Ge<f64>);
        break;
      case wabt::Opcode::F64Eq:
        DoBinop(Stack, Eq<f64>);
        break;
      case wabt::Opcode::F64Ne:
        DoBinop(Stack, Ne<f64>);
        break;
      default:
        Panic("Unsupported compare opcode");
        // clang-format on
      }
      break;
    }

    case wabt::ExprType::Convert: {
      switch (const auto CE = wabt::cast<wabt::ConvertExpr>(&E); CE->opcode) {

      case wabt::Opcode::I32Eqz:
        DoUnop(Stack, IntEqz<u32>);
        break;
      case wabt::Opcode::I64Eqz:
        DoUnop(Stack, IntEqz<u64>);
        break;
      case wabt::Opcode::I32WrapI64:
        DoConvert<u32, u64>(Stack);
        break;
      case wabt::Opcode::I64ExtendI32S:
        DoConvert<s64, s32>(Stack);
        break;
      case wabt::Opcode::I64ExtendI32U:
        DoConvert<u64, u32>(Stack);
        break;
      case wabt::Opcode::I32TruncF32S:
        DoConvert<s32, f32>(Stack);
        break;
      case wabt::Opcode::I32TruncF32U:
        DoConvert<u32, f32>(Stack);
        break;
      case wabt::Opcode::I32TruncF64S:
        DoConvert<s32, f64>(Stack);
        break;
      case wabt::Opcode::I32TruncF64U:
        DoConvert<u32, f64>(Stack);
        break;
      case wabt::Opcode::I64TruncF32S:
        DoConvert<s64, f32>(Stack);
        break;
      case wabt::Opcode::I64TruncF32U:
        DoConvert<u64, f32>(Stack);
        break;
      case wabt::Opcode::I64TruncF64S:
        DoConvert<s64, f64>(Stack);
        break;
      case wabt::Opcode::I64TruncF64U:
        DoConvert<u64, f64>(Stack);
        break;
      case wabt::Opcode::F32ConvertI32S:
        DoConvert<f32, s32>(Stack);
        break;
      case wabt::Opcode::F32ConvertI32U:
        DoConvert<f32, u32>(Stack);
        break;
      case wabt::Opcode::F32ConvertI64S:
        DoConvert<f32, s64>(Stack);
        break;
      case wabt::Opcode::F32ConvertI64U:
        DoConvert<f32, u64>(Stack);
        break;
      case wabt::Opcode::F64ConvertI32S:
        DoConvert<f64, s32>(Stack);
        break;
      case wabt::Opcode::F64ConvertI32U:
        DoConvert<f64, u32>(Stack);
        break;
      case wabt::Opcode::F64ConvertI64S:
        DoConvert<f64, s64>(Stack);
        break;
      case wabt::Opcode::F64ConvertI64U:
        DoConvert<f64, u64>(Stack);
        break;
      case wabt::Opcode::F32DemoteF64:
        DoConvert<f32, f64>(Stack);
        break;
      case wabt::Opcode::F64PromoteF32:
        DoConvert<f64, f32>(Stack);
        break;
      case wabt::Opcode::I32ReinterpretF32:
        DoReinterpret<u32, f32>(Stack);
        break;
      case wabt::Opcode::I64ReinterpretF64:
        DoReinterpret<u64, f64>(Stack);
        break;
      case wabt::Opcode::F32ReinterpretI32:
        DoReinterpret<f32, u32>(Stack);
        break;
      case wabt::Opcode::F64ReinterpretI64:
        DoReinterpret<f64, u64>(Stack);
        break;
      default:
        Panic("Unsupported Convert opcode");
        // clang-format on
      }
      break;
    }
    case wabt::ExprType::Unary: {
      auto UE = wabt::cast<wabt::UnaryExpr>(&E);
      switch (UE->opcode) {
      case wabt::Opcode::I32Clz:
        DoUnop(Stack, IntClz<u32>);
        break;
      case wabt::Opcode::I64Clz:
        DoUnop(Stack, IntClz<u64>);
        break;
      case wabt::Opcode::I32Ctz:
        DoUnop(Stack, IntCtz<u32>);
        break;
      case wabt::Opcode::I64Ctz:
        DoUnop(Stack, IntCtz<u64>);
        break;
      case wabt::Opcode::I32Popcnt:
        DoUnop(Stack, IntPopcnt<u32>);
        break;
      case wabt::Opcode::I64Popcnt:
        DoUnop(Stack, IntPopcnt<u64>);
        break;
      case wabt::Opcode::I32Extend8S:
        DoUnop(Stack, IntExtend<u32, 7>);
        break;
      case wabt::Opcode::I64Extend8S:
        DoUnop(Stack, IntExtend<u64, 7>);
        break;
      case wabt::Opcode::I32Extend16S:
        DoUnop(Stack, IntExtend<u32, 15>);
        break;
      case wabt::Opcode::I64Extend16S:
        DoUnop(Stack, IntExtend<u64, 15>);
        break;
      case wabt::Opcode::I64Extend32S:
        DoUnop(Stack, IntExtend<u64, 31>);
        break;
      case wabt::Opcode::F32Sqrt:
        DoUnop(Stack, FloatSqrt<f32>);
        break;
      case wabt::Opcode::F64Sqrt:
        DoUnop(Stack, FloatSqrt<f64>);
        break;
      case wabt::Opcode::F32Nearest:
        DoUnop(Stack, FloatNearest<f32>);
        break;
      case wabt::Opcode::F64Nearest:
        DoUnop(Stack, FloatNearest<f64>);
        break;
      case wabt::Opcode::F32Abs:
        DoUnop(Stack, FloatAbs<f32>);
        break;
      case wabt::Opcode::F64Abs:
        DoUnop(Stack, FloatAbs<f64>);
        break;
      case wabt::Opcode::F32Ceil:
        DoUnop(Stack, FloatCeil<f32>);
        break;
      case wabt::Opcode::F64Ceil:
        DoUnop(Stack, FloatCeil<f64>);
        break;
      case wabt::Opcode::F32Floor:
        DoUnop(Stack, FloatFloor<f32>);
        break;
      case wabt::Opcode::F64Floor:
        DoUnop(Stack, FloatFloor<f64>);
        break;
      case wabt::Opcode::F32Trunc:
        DoUnop(Stack, FloatTrunc<f32>);
        break;
      case wabt::Opcode::F64Trunc:
        DoUnop(Stack, FloatTrunc<f64>);
        break;
      case wabt::Opcode::F32Neg:
        DoUnop(Stack, FloatNeg<f32>);
        break;
      case wabt::Opcode::F64Neg:
        DoUnop(Stack, FloatNeg<f64>);
        break;
      default:
        Panic("Unsupported unary opcode");
        // clang-format on
      }
      break;
    }

    case wabt::ExprType::Drop: {
      Pop(Stack);
      break;
    }

    case wabt::ExprType::Call:
    case wabt::ExprType::CallIndirect: {
      u32 FIdx = 0;
      wabt::Func *Callee = nullptr;
      if (E.type() == wabt::ExprType::Call) {
        const auto CallExpr = wabt::cast<wabt::CallExpr>(&E);
        FIdx = CallExpr->var.index();
        Callee = M->funcs[FIdx];
      } else if (E.type() == wabt::ExprType::CallIndirect) {
        auto CalleeIndexInFunctionTable = Pop<u32>(Stack);
        FIdx = IndirectJumpTable[CalleeIndexInFunctionTable];
        Callee = M->funcs[FIdx];
      }
      assert(Callee != nullptr and "Callee is nullptr");

      ParamsToFrame(Callee);

      // print calling FIdx
      // fmt::println("[interpreter] calling {}", FIdx);

      // static int Indent = 0;
      // fmt::println("{}>>> {}", std::string(Indent++, ' '), Callee->name);
      // fmt::println("[interp|{}-call] {} -> {}",
      //              E.type() == wabt::ExprType::Call ? "direct" : "indirect",
      //              FunctionIndex, FIdx);
      Mon->set(FIdx);
      VirtualCall(FIdx);
      // Indent--;
      // fmt::println("{}<<< {}", std::string(--Indent, ' '), Callee->name);

      // if (Callee->name.starts_with("$__imported_wasi_snapshot_preview1")) {
      //   // WASI function
      //
      // } else {
      //   if (JumpTable[FIdx] == (void *)vmcalls::onDispatch)
      //     onDispatch(FIdx);
      //   else
      //     onJIT(FIdx);
      // }

      auto Result = FrameToReturn(Callee);
      for (auto &R : Result)
        Push(Stack, R);

      break;
    }

    case wabt::ExprType::Return: {
      BranchingTarget = std::numeric_limits<unsigned int>::max();
      break;
    }

    default:
      Panic(fmt::format("Unsupported opcode: {}",
                        wabt::GetExprTypeName(E.type())));
    }
  };

  for (auto &E : F->exprs) {
    Interp(E);
    if (BranchingTarget.has_value())
      break;
  }
  // if (BranchingTarget.has_value() and BranchingTarget.value() != 0 and
  // BranchingTarget.value() != std::numeric_limits<unsigned int>::max())
  // REPORT_ERROR_AND_EXIT("Invalid branching target");
  // PrintStack();

  // serializes values from ValueStack to ActivationFrame (before return)
  auto ReturnToFrame = [&] {
    assert(F->decl.sig.result_types.size() <= 1);
    if (not F->decl.sig.result_types.empty()) {
      const auto ReturnTy = F->decl.sig.result_types.front();

      switch (ReturnTy) {
      case wabt::Type::I32:
        *reinterpret_cast<u32 *>(ActivationFrame.data() + 0) = Pop<u32>(Stack);
        break;
      case wabt::Type::I64:
        *reinterpret_cast<u64 *>(ActivationFrame.data() + 0) = Pop<u64>(Stack);
        break;
      case wabt::Type::F32:
        *reinterpret_cast<f32 *>(ActivationFrame.data() + 0) = Pop<f32>(Stack);
        break;
      case wabt::Type::F64:
        *reinterpret_cast<f64 *>(ActivationFrame.data() + 0) = Pop<f64>(Stack);
        break;
      default:
        Panic("Unsupported return type");
      }
    }
  };
  ReturnToFrame();

  // {
  //   print(fg(fmt::color::red), "{}<<< {}", std::string(--Indent, ' '),
  //   FName); std::cout << std::endl;
  // }
}
