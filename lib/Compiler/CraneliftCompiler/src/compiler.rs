use cranelift::codegen::entity::Unsigned;
use cranelift::codegen::ir::entities::Value;
use cranelift::codegen::ir::function::DisplayFunction;
use cranelift::codegen::ir::immediates::{Ieee32, Ieee64};
use cranelift::codegen::ir::types::*;
use cranelift::codegen::ir::{Inst, SigRef, UserFuncName, ValueLabel};
use cranelift::codegen::isa::CallConv;
use cranelift::codegen::packed_option::ReservedValue;
use cranelift::codegen::{ir, settings};
use cranelift::frontend::{FunctionBuilder, FunctionBuilderContext, Variable};
use cranelift::module::{default_libcall_names, Linkage, Module, ModuleError, ModuleResult};
use cranelift::object::{ObjectBuilder, ObjectModule, ObjectProduct};
use cranelift::prelude::types::*;
use cranelift::prelude::{
    types, AbiParam, Configurable, EntityRef, FloatCC, InstBuilder, IntCC, JumpTableData, MemFlags,
    Signature, Type,
};
use std::any::Any;
use std::collections::{hash_map, HashMap};
use std::path::{Path, PathBuf};
use wasmparser;
use wasmparser::CompositeInnerType::Func;
use wasmparser::Operator::*;
use wasmparser::Payload::*;
use wasmparser::{
    FuncType, FunctionBody, MemArg, Operator, OperatorsIterator, Parser, TypeRef, ValType,
};
use wasmtime::OptLevel;

const PTR_TYPE: Type = I64;
const PTR_TYPE_SIZE: i32 = 8;
const GLOBAL_VAR_TYPE: Type = I32;
const GLOBAL_VAR_TYPE_SIZE: i32 = 4;
const INDIRECT_TABLE_ITEM_TYPE: Type = I32;
const INDIRECT_TABLE_ITEM_TYPE_SIZE: i32 = 4;

//////////////////////////// translation_utils.rs //////////////////////////////
pub fn f32_translation(x: wasmparser::Ieee32) -> Ieee32 {
    Ieee32::with_bits(x.bits())
}
pub fn f64_translation(x: wasmparser::Ieee64) -> Ieee64 {
    Ieee64::with_bits(x.bits())
}
fn translate_icmp(cc: IntCC, builder: &mut FunctionBuilder, state: &mut State) {
    let (arg0, arg1) = state.pop2();
    let val = builder.ins().icmp(cc, arg0, arg1);
    state.push1(builder.ins().uextend(I32, val));
}
fn translate_fcmp(cc: FloatCC, builder: &mut FunctionBuilder, state: &mut State) {
    let (arg0, arg1) = state.pop2();
    let val = builder.ins().fcmp(cc, arg0, arg1);
    state.push1(builder.ins().uextend(I32, val));
}

fn mem_op_size(opcode: ir::Opcode, ty: Type) -> u8 {
    match opcode {
        ir::Opcode::Istore8 | ir::Opcode::Sload8 | ir::Opcode::Uload8 => 1,
        ir::Opcode::Istore16 | ir::Opcode::Sload16 | ir::Opcode::Uload16 => 2,
        ir::Opcode::Istore32 | ir::Opcode::Sload32 | ir::Opcode::Uload32 => 4,
        ir::Opcode::Store | ir::Opcode::Load => u8::try_from(ty.bytes()).unwrap(),
        _ => panic!("unknown size of mem op for {opcode:?}"),
    }
}
fn translate_load(o: Operator, offset: u64, builder: &mut FunctionBuilder, state: &mut State) {
    let addr = state.pop1();
    let addr = builder.ins().uextend(I64, addr);
    let addr = builder
        .ins()
        .iadd(addr, state.host_pointers.linear_memory_ptr);
    let addr_with_offset = builder.ins().iadd_imm(addr, offset as i64);

    let load_ty = match o {
        I32Load8U { .. } | I32Load8S { .. } | I64Load8U { .. } | I64Load8S { .. } => I8,
        I32Load16U { .. } | I32Load16S { .. } | I64Load16U { .. } | I64Load16S { .. } => I16,
        I32Load { .. } | I64Load32U { .. } | I64Load32S { .. } => I32,
        I64Load { .. } => I64,
        F32Load { .. } => F32,
        F64Load { .. } => F64,
        _ => panic!("unsupported load opcode '{:?}'", o),
    };
    let val = builder
        .ins()
        .load(load_ty, MemFlags::new(), addr_with_offset, 0);
    let dest_ty = match o {
        I32Load8S { .. }
        | I32Load8U { .. }
        | I32Load16S { .. }
        | I32Load16U { .. }
        | I32Load { .. } => I32,
        I64Load8S { .. }
        | I64Load8U { .. }
        | I64Load16S { .. }
        | I64Load16U { .. }
        | I64Load32S { .. }
        | I64Load32U { .. }
        | I64Load { .. } => I64,
        F32Load { .. } => F32,
        F64Load { .. } => F64,
        _ => panic!("unsupported load opcode '{:?}'", o),
    };
    let ext_val = match o {
        I32Load8S { .. }
        | I32Load16S { .. }
        | I64Load8S { .. }
        | I64Load16S { .. }
        | I64Load32S { .. } => builder.ins().sextend(dest_ty, val),
        I32Load8U { .. }
        | I32Load16U { .. }
        | I64Load8U { .. }
        | I64Load16U { .. }
        | I64Load32U { .. } => builder.ins().uextend(dest_ty, val),
        _ => val,
    };
    state.push1(ext_val);
}
fn translate_store(o: Operator, offset: u64, builder: &mut FunctionBuilder, state: &mut State) {
    let (addr, val) = state.pop2();
    let addr = builder.ins().uextend(I64, addr);
    let addr = builder
        .ins()
        .iadd(addr, state.host_pointers.linear_memory_ptr);
    let addr_with_offset = builder.ins().iadd_imm(addr, offset as i64);

    let val = match o {
        I32Store8 { .. } | I64Store8 { .. } => builder.ins().ireduce(I8, val),
        I32Store16 { .. } | I64Store16 { .. } => builder.ins().ireduce(I16, val),
        I64Store32 { .. } => builder.ins().ireduce(I32, val),
        _ => val,
    };
    builder
        .ins()
        .store(MemFlags::new(), val, addr_with_offset, 0);
}

fn from_frame(builder: &mut FunctionBuilder, state: &mut State, types: Vec<ValType>) -> Vec<Value> {
    let mut args = Vec::with_capacity(types.len());
    let mut addr = 0;
    for ty in types {
        let ty = get_type(ty);
        let ptr = builder
            .ins()
            .iadd_imm(state.host_pointers.activation_frame_ptr, addr as i64);
        let val = builder.ins().load(ty, MemFlags::new(), ptr, 0);
        args.push(val);
        addr += ty.bytes() as u32;
    }
    args
}

fn to_frame(
    builder: &mut FunctionBuilder,
    state: &mut State,
    vals: Vec<Value>,
    types: Vec<ValType>,
) {
    let mut addr = 0;
    for (val, ty) in vals.iter().zip(types) {
        let ptr = builder
            .ins()
            .iadd_imm(state.host_pointers.activation_frame_ptr, addr as i64);
        builder.ins().store(MemFlags::new(), *val, ptr, 0);
        addr += get_type(ty).bytes() as u32;
    }
}

fn from_stack(state: &mut State, types: Vec<ValType>) -> Vec<Value> {
    let mut vals = Vec::with_capacity(types.len());
    for _ in 0..types.len() {
        vals.push(state.pop1());
    }
    vals.reverse();
    vals
}

fn to_stack(state: &mut State, vals: Vec<Value>) {
    for val in vals {
        state.push1(val);
    }
}

//////////////////////////// Compiler State //////////////////////////////
enum ControlFrame {
    Block {
        destination: ir::Block,
        exit_is_branched_to: bool,
    },
    Loop {
        destination: ir::Block,
        header: ir::Block,
    },
}

impl ControlFrame {
    fn set_branched_to_exit(&mut self) {
        match *self {
            Self::Block {
                ref mut exit_is_branched_to,
                ..
            } => *exit_is_branched_to = true,
            Self::Loop { .. } => {}
        }
    }

    pub fn br_destination(&self) -> ir::Block {
        match *self {
            Self::Block { destination, .. } => destination,
            Self::Loop { header, .. } => header,
        }
    }

    fn exit_is_branched_to(&self) -> bool {
        match *self {
            Self::Block {
                exit_is_branched_to,
                ..
            } => exit_is_branched_to,
            Self::Loop { .. } => false,
        }
    }

    fn following_code(&self) -> ir::Block {
        match *self {
            Self::Block { destination, .. } | Self::Loop { destination, .. } => destination,
        }
    }

    fn is_loop(&self) -> bool {
        match *self {
            Self::Block { .. } => false,
            Self::Loop { .. } => true,
        }
    }
}

fn translate_br_if(relative_depth: u32, builder: &mut FunctionBuilder, state: &mut State) {
    let val = state.pop1();

    let i = state.frames.len() - 1 - (relative_depth as usize);
    let br_destination = {
        let frame = &mut state.frames[i];
        // The values returned by the branch are still available for the reachable
        // code that comes after it
        frame.set_branched_to_exit();
        frame.br_destination()
    };
    let next_block = builder.create_block();
    builder
        .ins()
        .brif(val, br_destination, &[], next_block, &[]);

    builder.seal_block(next_block); // The only predecessor is the current block.
    builder.switch_to_block(next_block);
}

pub struct HostPointers {
    vm_ptr: Value,
    activation_frame_ptr: Value,
    globals_ptr: Value,
    linear_memory_ptr: Value,
    jump_table_ptr: Value,
    indirect_table_ptr: Value,
}

pub struct HostCalls {
    memory_fill: Value,
    memory_copy: Value,
    memory_grow: Value,
    memory_size: Value,
}

#[derive(Default, Clone, Debug)]
struct FuncSignature {
    params_type: Vec<ValType>,
    return_type: Vec<ValType>,
}

pub struct State {
    signature: FuncSignature,

    values: Vec<Value>,
    frames: Vec<ControlFrame>,
    reachable: bool,

    host_pointers: HostPointers,
    host_calls: HostCalls,
}

impl State {
    fn new(signature: FuncSignature, host_pointers: HostPointers, host_calls: HostCalls) -> Self {
        Self {
            signature,
            values: Vec::new(),
            frames: Vec::new(),
            reachable: true,
            host_pointers,
            host_calls,
        }
    }

    fn initialize(&mut self, exit_block: ir::Block) {
        self.clear();
        self.push_frame(ControlFrame::Block {
            destination: exit_block,
            exit_is_branched_to: false,
        });
    }

    fn clear(&mut self) {
        debug_assert!(self.values.is_empty());
        debug_assert!(self.frames.is_empty());
    }

    fn push1(&mut self, val: Value) {
        self.values.push(val);
    }

    fn push2(&mut self, val1: Value, val2: Value) {
        self.values.push(val1);
        self.values.push(val2);
    }

    fn pushn(&mut self, vals: &[Value]) {
        self.values.extend_from_slice(vals);
    }

    fn pop1(&mut self) -> Value {
        self.values
            .pop()
            .expect("attempted to pop a value from an empty stack")
    }

    fn peek1(&self) -> Value {
        *self
            .values
            .last()
            .expect("attempted to peek at a value on an empty stack")
    }

    fn pop2(&mut self) -> (Value, Value) {
        let v2 = self.values.pop().unwrap();
        let v1 = self.values.pop().unwrap();
        (v1, v2)
    }

    fn pop3(&mut self) -> (Value, Value, Value) {
        let v3 = self.values.pop().unwrap();
        let v2 = self.values.pop().unwrap();
        let v1 = self.values.pop().unwrap();
        (v1, v2, v3)
    }

    fn pop4(&mut self) -> (Value, Value, Value, Value) {
        let v4 = self.values.pop().unwrap();
        let v3 = self.values.pop().unwrap();
        let v2 = self.values.pop().unwrap();
        let v1 = self.values.pop().unwrap();
        (v1, v2, v3, v4)
    }

    fn pop5(&mut self) -> (Value, Value, Value, Value, Value) {
        let v5 = self.values.pop().unwrap();
        let v4 = self.values.pop().unwrap();
        let v3 = self.values.pop().unwrap();
        let v2 = self.values.pop().unwrap();
        let v1 = self.values.pop().unwrap();
        (v1, v2, v3, v4, v5)
    }

    fn popn(&mut self, n: usize) {
        let new_len = self.values.len() - n;
        self.values.truncate(new_len);
    }

    fn peekn(&self, n: usize) -> &[Value] {
        &self.values[self.values.len() - n..]
    }

    fn peekn_mut(&mut self, n: usize) -> &mut [Value] {
        let len = self.values.len();
        &mut self.values[len - n..]
    }

    fn push_frame(&mut self, frame: ControlFrame) {
        // match frame {
        //     ControlFrame::Block { destination, .. } => println!("[push-block] {:?}", destination),
        //     ControlFrame::Loop {
        //         header,
        //         destination,
        //     } => println!("[push-loop] {:?} -> {:?}", header, destination),
        // };
        self.frames.push(frame);
    }

    fn pop_frame(&mut self) -> ControlFrame {
        let frame = self
            .frames
            .pop()
            .expect("attempted to pop a block from an empty stack");
        // match frame {
        //     ControlFrame::Block { destination, .. } => println!("[pop-block] {:?}", destination),
        //     ControlFrame::Loop {
        //         header,
        //         destination,
        //     } => println!("[pop-loop] {:?} -> {:?}", header, destination),
        // };
        frame
    }
}

//////////////////////////// VM //////////////////////////////
pub enum HostCall {
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
    CounterTable,     // 11
    OnInstruction,
    OnStackOp,
    OnMemoryOp,
    OnCall,
    OnReturn,
    OnOutOfBoundMemory,
    OnBranch,
    OnString,
}

// get the int value of the state
pub fn ptr_idx(state: HostCall) -> i32 {
    match state {
        HostCall::VMPtr => 0,
        HostCall::Globals => 1,
        HostCall::LinearMemory => 2,
        HostCall::LinearMemorySize => 3,
        HostCall::MemoryFill => 4,
        HostCall::MemoryCopy => 5,
        HostCall::MemoryGrow => 6,
        HostCall::MemorySize => 7,
        HostCall::ActiveFrame => 8,
        HostCall::JumpTable => 9,
        HostCall::IndirectTable => 10,
        HostCall::CounterTable => 11,
        HostCall::OnInstruction => 12,
        HostCall::OnStackOp => 13,
        HostCall::OnMemoryOp => 14,
        HostCall::OnCall => 15,
        HostCall::OnReturn => 16,
        HostCall::OnOutOfBoundMemory => 17,
        HostCall::OnBranch => 18,
        HostCall::OnString => 19,
    }
}

fn get_type(ty: ValType) -> Type {
    match ty {
        ValType::I32 => types::I32,
        ValType::I64 => types::I64,
        ValType::F32 => types::F32,
        ValType::F64 => types::F64,
        ValType::V128 => panic!("V128 is not supported"),
        ValType::Ref(_) => panic!("Ref is not supported"),
    }
}

pub fn wat2wasm(wat: &Path) -> PathBuf {
    let wasm = wat.with_extension("wasm");
    let output = std::process::Command::new("wat2wasm")
        .arg("-o")
        .arg(&wasm)
        .arg(wat)
        .output()
        .unwrap();
    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        eprintln!("{}", stderr);
        panic!("wat2wasm failed");
    }
    wasm
}

#[derive(Debug)]
pub struct Compiler<'a> {
    types: Vec<FuncType>,
    fn_bodies: Vec<FunctionBody<'a>>,
    fn_types: Vec<usize>,
    num_imported_funcs: u32,
}

impl<'a> Compiler<'a> {
    pub fn new(application: String) -> Self {
        let mut types = Vec::new();
        let mut fn_bodies = Vec::new();
        let mut fn_types = Vec::new();

        let application = application.as_str();
        let mut path = Path::new(application);
        if path.extension().unwrap() == "wat" {
            wat2wasm(path);
        }
        let binding = path.with_extension("wasm");
        path = binding.as_path();
        let vec = std::fs::read(path).unwrap();
        let len = vec.len();
        let buf = unsafe { std::slice::from_raw_parts(vec.as_ptr(), len) };
        std::mem::forget(vec);

        let mut num_imported_funcs = 0;

        for payload in Parser::default().parse_all(buf) {
            match payload.unwrap() {
                ImportSection(imports) => {
                    for entry in imports {
                        let import = entry.unwrap();
                        match import.ty {
                            TypeRef::Func(index) => {
                                num_imported_funcs += 1;
                                fn_types.push(index as usize);
                            }
                            _ => continue,
                        };
                    }
                }
                TypeSection(types_sec) => {
                    let num_types = usize::try_from(types_sec.count()).unwrap();
                    types.reserve(num_types);

                    for ty in types_sec {
                        let ty = ty.unwrap().into_types();
                        assert_eq!(ty.len(), 1);
                        for ty in ty {
                            if let Func(ty) = ty.composite_type.inner {
                                types.push(ty);
                            } else {
                                panic!("Unexpected type");
                            }
                        }
                    }
                }
                FunctionSection(functions) => {
                    let num_func = usize::try_from(functions.count()).unwrap();
                    fn_bodies.reserve(num_func);
                    fn_types.reserve(num_func);
                    for ty_idx in functions {
                        let ty_idx = ty_idx.unwrap() as usize;
                        fn_types.push(ty_idx);
                    }
                }
                CodeSectionEntry(sec) => {
                    fn_bodies.push(sec);
                }
                _ => {}
            }
        }

        // print num of types
        // println!("[types] {:?}", types.len());
        // print num of functions
        // println!("[functions-bodies] {:?}", fn_bodies.len());
        // print num of function types
        // println!("[function-types] {:?}", fn_types.len());
        // print num of imported functions
        // println!("[imported functions] {:?}", num_imported_funcs);

        Compiler {
            types,
            fn_bodies,
            fn_types,
            num_imported_funcs,
        }
    }

    fn get_body(&self, func: u32) -> &FunctionBody<'a> {
        self.fn_bodies
            .get((func - self.num_imported_funcs) as usize)
            .unwrap()
    }

    fn get_fn_type(&self, func: u32) -> &FuncType {
        let fn_type_idx = self.fn_types[func as usize];
        self.types.get(fn_type_idx).unwrap()
    }

    pub fn compile(&self, func: u32, opt_level: u32, disable_hardware_features: bool) -> Vec<u8> {
        let opt_level = if opt_level == 0 { "none" } else { "speed" };

        let enable_hardware_features = !disable_hardware_features;
        let isa_builder = cranelift::native::builder_with_options(enable_hardware_features)
            .unwrap_or_else(|msg| {
                panic!("host machine is not supported: {msg}");
            });

        let mut flag_builder = settings::builder();
        flag_builder.set("opt_level", opt_level).unwrap();
        flag_builder.set("use_colocated_libcalls", "false").unwrap();
        flag_builder.set("is_pic", "false").unwrap(); // FIXME set back to true once the x64 backend supports it.
        flag_builder.set("enable_verifier", "false").unwrap();

        let isa = isa_builder
            .finish(settings::Flags::new(flag_builder))
            .unwrap();
        let mut cl_module =
            ObjectModule::new(ObjectBuilder::new(isa, "main", default_libcall_names()).unwrap());

        let mut ctx = cl_module.make_context();
        let mut func_ctx = FunctionBuilderContext::new();

        let mut sig = cl_module.make_signature();
        sig.params.push(AbiParam::new(types::I64)); // *vm
                                                    // sig.params.push(AbiParam::new(I32)); // fn_idx

        let cl_func = cl_module
            .declare_function(func.to_string().as_str(), Linkage::Export, &sig)
            .unwrap();

        ctx.func.signature = sig;
        ctx.func.name = UserFuncName::user(0, cl_func.as_u32());

        let mut builder = FunctionBuilder::new(&mut ctx.func, &mut func_ctx);
        let entry_block = builder.create_block();
        builder.append_block_params_for_function_params(entry_block);
        builder.switch_to_block(entry_block);
        builder.seal_block(entry_block); // Declare all predecessors known.

        builder.ensure_inserted_block();

        let ty = self.get_fn_type(func);
        let mut sig = FuncSignature::default();
        for param in ty.params() {
            sig.params_type.push(*param);
        }
        for result in ty.results() {
            sig.return_type.push(*result);
        }

        ////////////////////////////// host calls //////////////////////////////

        let vm_ptr = builder.block_params(entry_block)[0];

        let activation_frame_ptr = builder.ins().load(
            PTR_TYPE,
            MemFlags::new(),
            vm_ptr,
            ptr_idx(HostCall::ActiveFrame) * PTR_TYPE_SIZE,
        );

        let globals_ptr = builder.ins().load(
            PTR_TYPE,
            MemFlags::new(),
            vm_ptr,
            ptr_idx(HostCall::Globals) * PTR_TYPE_SIZE,
        );

        let linear_memory_ptr = builder.ins().load(
            PTR_TYPE,
            MemFlags::new(),
            vm_ptr,
            ptr_idx(HostCall::LinearMemory) * PTR_TYPE_SIZE,
        );

        let jump_table_ptr = builder.ins().load(
            PTR_TYPE,
            MemFlags::new(),
            vm_ptr,
            ptr_idx(HostCall::JumpTable) * PTR_TYPE_SIZE,
        );

        let indirect_table_ptr = builder.ins().load(
            PTR_TYPE,
            MemFlags::new(),
            vm_ptr,
            ptr_idx(HostCall::IndirectTable) * PTR_TYPE_SIZE,
        );

        let host_pointers = HostPointers {
            vm_ptr,
            activation_frame_ptr,
            globals_ptr,
            linear_memory_ptr,
            jump_table_ptr,
            indirect_table_ptr,
        };

        let memory_size_ptr = builder.ins().load(
            PTR_TYPE,
            MemFlags::new(),
            vm_ptr,
            ptr_idx(HostCall::MemorySize) * PTR_TYPE_SIZE,
        );

        // MemoryFill,       // 4
        let memory_fill_ptr = builder.ins().load(
            PTR_TYPE,
            MemFlags::new(),
            vm_ptr,
            ptr_idx(HostCall::MemoryFill) * PTR_TYPE_SIZE,
        );
        //     MemoryCopy,       // 5
        let memory_copy_ptr = builder.ins().load(
            PTR_TYPE,
            MemFlags::new(),
            vm_ptr,
            ptr_idx(HostCall::MemoryCopy) * PTR_TYPE_SIZE,
        );
        //     MemoryGrow,       // 6
        let memory_grow_ptr = builder.ins().load(
            PTR_TYPE,
            MemFlags::new(),
            vm_ptr,
            ptr_idx(HostCall::MemoryGrow) * PTR_TYPE_SIZE,
        );

        let host_calls = HostCalls {
            memory_fill: memory_fill_ptr,
            memory_copy: memory_copy_ptr,
            memory_grow: memory_grow_ptr,
            memory_size: memory_size_ptr,
        };

        let mut state = &mut State::new(sig.clone(), host_pointers, host_calls);

        // Set up the translation state with a single pushed control block representing the whole
        // function and its return values.
        let exit_block = builder.create_block();
        builder.append_block_params_for_function_returns(exit_block);
        state.initialize(exit_block);

        let params = from_frame(&mut builder, &mut state, sig.params_type.clone());

        let mut local_cnt = 0;
        for (ty, param) in sig.params_type.iter().zip(params.iter()) {
            let name = Variable::new(local_cnt);
            builder.declare_var(name, get_type(*ty));
            builder.def_var(name, *param);
            local_cnt += 1;
        }
        // print num of params
        // println!("[params] {:?}", local_cnt);
        ////////////////////////////
        // TODO: not sure if this is the right way to declare locals
        let locals = self.get_body(func).get_locals_reader().unwrap().into_iter();

        for local in locals {
            let (cnt, ty) = local.unwrap();
            for _ in 0..cnt {
                let name = Variable::new(local_cnt);
                builder.declare_var(name, get_type(ty));
                local_cnt += 1;
            }
        }
        // println!("[params+locals] {:?}", local_cnt);
        ////////////////////////////

        // println!("[fn-body]");
        let body = self
            .get_body(func)
            .get_operators_reader()
            .unwrap()
            .into_iter();
        for o in body {
            let o = o.unwrap();
            // let processing = if state.reachable {
            //     "visiting"
            // } else {
            //     "skipping"
            // };
            // println!("[{}] {:?}", processing, o);

            if !state.reachable {
                match o {
                    Operator::Loop { blockty: _ } | Operator::Block { blockty: _ } => {
                        state.push_frame(ControlFrame::Block {
                            destination: ir::Block::reserved_value(),
                            exit_is_branched_to: false,
                        });
                    }
                    Operator::End => {
                        let frame = state.pop_frame();

                        let reachable_anyway = match frame {
                            // If it is a loop we also have to seal the body loop block
                            ControlFrame::Loop { header, .. } => {
                                builder.seal_block(header);
                                // And loops can't have branches to the end.
                                false
                            }
                            // All other control constructs are already handled.
                            _ => false,
                        };

                        if frame.exit_is_branched_to() || reachable_anyway {
                            builder.switch_to_block(frame.following_code());
                            builder.seal_block(frame.following_code());

                            // And add the return values of the block but only if the next block is reachable
                            // (which corresponds to testing if the stack depth is 1)
                            state.reachable = true;
                        }
                    }
                    _ => {
                        // We don't translate because this is unreachable code
                    }
                }
                continue;
            }

            match o {
                /****************************** Nullary Operators ************************************/
                I32Const { value } => {
                    state.push1(builder.ins().iconst(I32, i64::from(value.unsigned())))
                }
                I64Const { value } => {
                    state.push1(builder.ins().iconst(I64, value));
                }
                F32Const { value } => {
                    state.push1(builder.ins().f32const(f32_translation(value)));
                }
                F64Const { value } => {
                    state.push1(builder.ins().f64const(f64_translation(value)));
                }
                /********************************** Locals ****************************************
                 *  `get_local` and `set_local` are treated as non-SSA variables and will completely
                 *  disappear in the Cranelift Code
                 ***********************************************************************************/
                Operator::LocalGet { local_index } => {
                    let val = builder.use_var(Variable::from_u32(local_index));
                    state.push1(val);
                }
                Operator::LocalSet { local_index } => {
                    let mut val = state.pop1();
                    builder.def_var(Variable::from_u32(local_index), val);
                }
                Operator::LocalTee { local_index } => {
                    let mut val = state.peek1();
                    builder.def_var(Variable::from_u32(local_index), val);
                }
                /********************************* Stack misc ***************************************
                 *  `drop`, `nop`, `unreachable` and `select`.
                 ***********************************************************************************/
                Operator::Drop => {
                    state.pop1();
                }
                Operator::Select => {
                    let (mut arg1, mut arg2, cond) = state.pop3();
                    state.push1(builder.ins().select(cond, arg1, arg2));
                }
                Operator::Nop => {
                    // We do nothing
                }
                Operator::Unreachable => {
                    // ret
                    builder.ins().return_(&[]);
                    // do nothing, because in theory in the execution we shouldn't reach here.
                    state.reachable = false;
                }
                /******************************* Load instructions ***********************************
                 * Wasm specifies an integer alignment flag but we drop it in Cranelift.
                 * The memory base address is provided by the environment.
                 ************************************************************************************/
                I32Load8U { memarg }
                | I32Load8S { memarg }
                | I64Load8U { memarg }
                | I64Load8S { memarg }
                | I32Load16U { memarg }
                | I32Load16S { memarg }
                | I64Load16U { memarg }
                | I64Load16S { memarg }
                | I32Load { memarg }
                | I64Load32U { memarg }
                | I64Load32S { memarg }
                | I64Load { memarg }
                | F32Load { memarg }
                | F64Load { memarg } => {
                    translate_load(o, memarg.offset, &mut builder, &mut state);
                }
                I32Store { memarg }
                | I64Store { memarg }
                | F32Store { memarg }
                | F64Store { memarg }
                | I32Store8 { memarg }
                | I64Store8 { memarg }
                | I32Store16 { memarg }
                | I64Store16 { memarg }
                | I64Store32 { memarg } => {
                    translate_store(o, memarg.offset, &mut builder, &mut state);
                }
                /******************************* Unary Operators *************************************/
                Operator::I32Clz | Operator::I64Clz => {
                    let arg = state.pop1();
                    state.push1(builder.ins().clz(arg));
                }
                Operator::I32Ctz | Operator::I64Ctz => {
                    let arg = state.pop1();
                    state.push1(builder.ins().ctz(arg));
                }
                Operator::I32Popcnt | Operator::I64Popcnt => {
                    let arg = state.pop1();
                    state.push1(builder.ins().popcnt(arg));
                }
                Operator::I64ExtendI32S => {
                    let val = state.pop1();
                    state.push1(builder.ins().sextend(I64, val));
                }
                Operator::I64ExtendI32U => {
                    let val = state.pop1();
                    state.push1(builder.ins().uextend(I64, val));
                }
                Operator::I32WrapI64 => {
                    let val = state.pop1();
                    state.push1(builder.ins().ireduce(I32, val));
                }
                Operator::F32Sqrt | Operator::F64Sqrt => {
                    let arg = state.pop1();
                    state.push1(builder.ins().sqrt(arg));
                }
                Operator::F32Ceil | Operator::F64Ceil => {
                    let arg = state.pop1();
                    state.push1(builder.ins().ceil(arg));
                }
                Operator::F32Floor | Operator::F64Floor => {
                    let arg = state.pop1();
                    state.push1(builder.ins().floor(arg));
                }
                Operator::F32Trunc | Operator::F64Trunc => {
                    let arg = state.pop1();
                    state.push1(builder.ins().trunc(arg));
                }
                Operator::F32Nearest | Operator::F64Nearest => {
                    let arg = state.pop1();
                    state.push1(builder.ins().nearest(arg));
                }
                Operator::F32Abs | Operator::F64Abs => {
                    let val = state.pop1();
                    state.push1(builder.ins().fabs(val));
                }
                Operator::F32Neg | Operator::F64Neg => {
                    let arg = state.pop1();
                    state.push1(builder.ins().fneg(arg));
                }
                Operator::F64ConvertI64U | Operator::F64ConvertI32U => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_from_uint(F64, val));
                }
                Operator::F64ConvertI64S | Operator::F64ConvertI32S => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_from_sint(F64, val));
                }
                Operator::F32ConvertI64S | Operator::F32ConvertI32S => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_from_sint(F32, val));
                }
                Operator::F32ConvertI64U | Operator::F32ConvertI32U => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_from_uint(F32, val));
                }
                Operator::F64PromoteF32 => {
                    let val = state.pop1();
                    state.push1(builder.ins().fpromote(F64, val));
                }
                Operator::F32DemoteF64 => {
                    let val = state.pop1();
                    state.push1(builder.ins().fdemote(F32, val));
                }
                Operator::I64TruncF64S | Operator::I64TruncF32S => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_to_sint(I64, val));
                }
                Operator::I32TruncF64S | Operator::I32TruncF32S => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_to_sint(I32, val));
                }
                Operator::I64TruncF64U | Operator::I64TruncF32U => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_to_uint(I64, val));
                }
                Operator::I32TruncF64U | Operator::I32TruncF32U => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_to_uint(I32, val));
                }
                Operator::I64TruncSatF64S | Operator::I64TruncSatF32S => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_to_sint_sat(I64, val));
                }
                Operator::I32TruncSatF64S | Operator::I32TruncSatF32S => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_to_sint_sat(I32, val));
                }
                Operator::I64TruncSatF64U | Operator::I64TruncSatF32U => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_to_uint_sat(I64, val));
                }
                Operator::I32TruncSatF64U | Operator::I32TruncSatF32U => {
                    let val = state.pop1();
                    state.push1(builder.ins().fcvt_to_uint_sat(I32, val));
                }
                Operator::F32ReinterpretI32 => {
                    let val = state.pop1();
                    state.push1(builder.ins().bitcast(F32, MemFlags::new(), val));
                }
                Operator::F64ReinterpretI64 => {
                    let val = state.pop1();
                    state.push1(builder.ins().bitcast(F64, MemFlags::new(), val));
                }
                Operator::I32ReinterpretF32 => {
                    let val = state.pop1();
                    state.push1(builder.ins().bitcast(I32, MemFlags::new(), val));
                }
                Operator::I64ReinterpretF64 => {
                    let val = state.pop1();
                    state.push1(builder.ins().bitcast(I64, MemFlags::new(), val));
                }
                Operator::I32Extend8S => {
                    let val = state.pop1();
                    state.push1(builder.ins().ireduce(I8, val));
                    let val = state.pop1();
                    state.push1(builder.ins().sextend(I32, val));
                }
                Operator::I32Extend16S => {
                    let val = state.pop1();
                    state.push1(builder.ins().ireduce(I16, val));
                    let val = state.pop1();
                    state.push1(builder.ins().sextend(I32, val));
                }
                Operator::I64Extend8S => {
                    let val = state.pop1();
                    state.push1(builder.ins().ireduce(I8, val));
                    let val = state.pop1();
                    state.push1(builder.ins().sextend(I64, val));
                }
                Operator::I64Extend16S => {
                    let val = state.pop1();
                    state.push1(builder.ins().ireduce(I16, val));
                    let val = state.pop1();
                    state.push1(builder.ins().sextend(I64, val));
                }
                Operator::I64Extend32S => {
                    let val = state.pop1();
                    state.push1(builder.ins().ireduce(I32, val));
                    let val = state.pop1();
                    state.push1(builder.ins().sextend(I64, val));
                }
                /****************************** Binary Operators ************************************/
                Operator::I32Add | Operator::I64Add => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().iadd(arg1, arg2));
                }
                Operator::I32And | Operator::I64And => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().band(arg1, arg2));
                }
                Operator::I32Or | Operator::I64Or => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().bor(arg1, arg2));
                }
                Operator::I32Xor | Operator::I64Xor => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().bxor(arg1, arg2));
                }
                Operator::I32Shl | Operator::I64Shl => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().ishl(arg1, arg2));
                }
                Operator::I32ShrS | Operator::I64ShrS => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().sshr(arg1, arg2));
                }
                Operator::I32ShrU | Operator::I64ShrU => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().ushr(arg1, arg2));
                }
                Operator::I32Rotl | Operator::I64Rotl => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().rotl(arg1, arg2));
                }
                Operator::I32Rotr | Operator::I64Rotr => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().rotr(arg1, arg2));
                }
                Operator::F32Add | Operator::F64Add => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().fadd(arg1, arg2));
                }
                Operator::I32Sub | Operator::I64Sub => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().isub(arg1, arg2));
                }
                Operator::F32Sub | Operator::F64Sub => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().fsub(arg1, arg2));
                }
                Operator::I32Mul | Operator::I64Mul => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().imul(arg1, arg2));
                }
                Operator::F32Mul | Operator::F64Mul => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().fmul(arg1, arg2));
                }
                Operator::F32Div | Operator::F64Div => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().fdiv(arg1, arg2));
                }
                Operator::I32DivS | Operator::I64DivS => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().sdiv(arg1, arg2));
                }
                Operator::I32DivU | Operator::I64DivU => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().udiv(arg1, arg2));
                }
                Operator::I32RemS | Operator::I64RemS => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().srem(arg1, arg2));
                }
                Operator::I32RemU | Operator::I64RemU => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().urem(arg1, arg2));
                }
                Operator::F32Min | Operator::F64Min => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().fmin(arg1, arg2));
                }
                Operator::F32Max | Operator::F64Max => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().fmax(arg1, arg2));
                }
                Operator::F32Copysign | Operator::F64Copysign => {
                    let (arg1, arg2) = state.pop2();
                    state.push1(builder.ins().fcopysign(arg1, arg2));
                }
                /**************************** Comparison Operators **********************************/
                Operator::I32LtS | Operator::I64LtS => {
                    translate_icmp(IntCC::SignedLessThan, &mut builder, &mut state)
                }
                Operator::I32LtU | Operator::I64LtU => {
                    translate_icmp(IntCC::UnsignedLessThan, &mut builder, &mut state)
                }
                Operator::I32LeS | Operator::I64LeS => {
                    translate_icmp(IntCC::SignedLessThanOrEqual, &mut builder, &mut state)
                }
                Operator::I32LeU | Operator::I64LeU => {
                    translate_icmp(IntCC::UnsignedLessThanOrEqual, &mut builder, &mut state)
                }
                Operator::I32GtS | Operator::I64GtS => {
                    translate_icmp(IntCC::SignedGreaterThan, &mut builder, &mut state)
                }
                Operator::I32GtU | Operator::I64GtU => {
                    translate_icmp(IntCC::UnsignedGreaterThan, &mut builder, &mut state)
                }
                Operator::I32GeS | Operator::I64GeS => {
                    translate_icmp(IntCC::SignedGreaterThanOrEqual, &mut builder, &mut state)
                }
                Operator::I32GeU | Operator::I64GeU => {
                    translate_icmp(IntCC::UnsignedGreaterThanOrEqual, &mut builder, &mut state)
                }
                Operator::I32Eqz | Operator::I64Eqz => {
                    let arg = state.pop1();
                    let val = builder.ins().icmp_imm(IntCC::Equal, arg, 0);
                    state.push1(builder.ins().uextend(I32, val));
                }
                Operator::I32Eq | Operator::I64Eq => {
                    translate_icmp(IntCC::Equal, &mut builder, &mut state)
                }
                Operator::F32Eq | Operator::F64Eq => {
                    translate_fcmp(FloatCC::Equal, &mut builder, &mut state)
                }
                Operator::I32Ne | Operator::I64Ne => {
                    translate_icmp(IntCC::NotEqual, &mut builder, &mut state)
                }
                Operator::F32Ne | Operator::F64Ne => {
                    translate_fcmp(FloatCC::NotEqual, &mut builder, &mut state)
                }
                Operator::F32Gt | Operator::F64Gt => {
                    translate_fcmp(FloatCC::GreaterThan, &mut builder, &mut state)
                }
                Operator::F32Ge | Operator::F64Ge => {
                    translate_fcmp(FloatCC::GreaterThanOrEqual, &mut builder, &mut state)
                }
                Operator::F32Lt | Operator::F64Lt => {
                    translate_fcmp(FloatCC::LessThan, &mut builder, &mut state)
                }
                Operator::F32Le | Operator::F64Le => {
                    translate_fcmp(FloatCC::LessThanOrEqual, &mut builder, &mut state)
                }
                /***************************** Control flow blocks **********************************
                 *  When starting a control flow block, we create a new `Block` that will hold the code
                 *  after the block, and we push a frame on the control stack. Depending on the type
                 *  of block, we create a new `Block` for the body of the block with an associated
                 *  jump instruction.
                 *
                 *  The `End` instruction pops the last control frame from the control stack, seals
                 *  the destination block (since `br` instructions targeting it only appear inside the
                 *  block and have already been translated) and modify the value stack to use the
                 *  possible `Block`'s arguments values.
                 ***********************************************************************************/
                Operator::Block { .. } => {
                    let next = builder.create_block();
                    state.push_frame(ControlFrame::Block {
                        destination: next,
                        exit_is_branched_to: false,
                    });
                }
                Operator::Loop { .. } => {
                    let loop_body = builder.create_block();
                    let next = builder.create_block();
                    builder.ins().jump(loop_body, &[]);
                    state.push_frame(ControlFrame::Loop {
                        header: loop_body,
                        destination: next,
                    });
                    builder.switch_to_block(loop_body);
                }
                Operator::End => {
                    let frame = state.pop_frame();
                    if state.frames.len() == 0 {
                        // TODO: added by me
                        do_return(&mut builder, &mut state);
                        // end of parsing the body
                    } else {
                        let next_block = match frame {
                            ControlFrame::Block { destination, .. }
                            | ControlFrame::Loop { destination, .. } => destination,
                        };

                        builder.ins().jump(next_block, &[]);
                        builder.switch_to_block(next_block);
                        builder.seal_block(next_block);

                        // If it is a loop we also have to seal the body loop block
                        if let ControlFrame::Loop { header, .. } = frame {
                            builder.seal_block(header)
                        }
                    }
                }
                /********************************** Globals ****************************************
                 *  `get_global` and `set_global` are handled by the environment.
                 ***********************************************************************************/
                Operator::GlobalGet { global_index } => {
                    let val = builder.ins().load(
                        GLOBAL_VAR_TYPE,
                        MemFlags::new(),
                        state.host_pointers.globals_ptr,
                        GLOBAL_VAR_TYPE_SIZE * global_index as i32,
                    );
                    state.push1(val);
                }
                Operator::GlobalSet { global_index } => {
                    let val = state.pop1();
                    builder.ins().store(
                        MemFlags::new(),
                        val,
                        state.host_pointers.globals_ptr,
                        GLOBAL_VAR_TYPE_SIZE * global_index as i32,
                    );
                }
                /**************************** Branch instructions *********************************
                 * The branch instructions all have as arguments a target nesting level, which
                 * corresponds to how many control stack frames do we have to pop to get the
                 * destination `Block`.
                 *
                 * Once the destination `Block` is found, we sometimes have to declare a certain depth
                 * of the stack unreachable, because some branch instructions are terminator.
                 *
                 * The `br_table` case is much more complicated because Cranelift's `br_table` instruction
                 * does not support jump arguments like all the other branch instructions. That is why, in
                 * the case where we would use jump arguments for every other branch instruction, we
                 * need to split the critical edges leaving the `br_tables` by creating one `Block` per
                 * table destination; the `br_table` will point to these newly created `Blocks` and these
                 * `Block`s contain only a jump instruction pointing to the final destination, this time with
                 * jump arguments.
                 *
                 * This system is also implemented in Cranelift's SSA construction algorithm, because
                 * `use_var` located in a destination `Block` of a `br_table` might trigger the addition
                 * of jump arguments in each predecessor branch instruction, one of which might be a
                 * `br_table`.
                 ***********************************************************************************/
                Operator::Br { relative_depth } => {
                    let i = state.frames.len() - 1 - (relative_depth as usize);
                    let br_destination = {
                        let frame = &mut state.frames[i];
                        // We signal that all the code that follows until the next End is unreachable
                        frame.set_branched_to_exit();
                        frame.br_destination()
                    };
                    builder.ins().jump(br_destination, &[]);
                    state.reachable = false;
                }
                Operator::BrIf { relative_depth } => {
                    translate_br_if(relative_depth, &mut builder, state)
                }
                Operator::BrTable { targets } => {
                    let default = targets.default();
                    let mut min_depth = default;
                    for depth in targets.targets() {
                        let depth = depth.unwrap();
                        if depth < min_depth {
                            min_depth = depth;
                        }
                    }
                    // let jump_args_count = {
                    //     let i = state.frames.len() - 1 - (min_depth as usize);
                    //     let min_depth_frame = &state.frames[i];
                    //     if min_depth_frame.is_loop() {
                    //         min_depth_frame.num_param_values()
                    //     } else {
                    //         min_depth_frame.num_return_values()
                    //     }
                    // };
                    // TODO: it's hardcoded to 0 for now
                    let jump_args_count = 0;

                    let val = state.pop1();
                    let mut data = Vec::with_capacity(targets.len() as usize);
                    if jump_args_count == 0 {
                        // No jump arguments
                        for depth in targets.targets() {
                            let depth = depth.unwrap();
                            let block = {
                                let i = state.frames.len() - 1 - (depth as usize);
                                let frame = &mut state.frames[i];
                                frame.set_branched_to_exit();
                                frame.br_destination()
                            };
                            data.push(builder.func.dfg.block_call(block, &[]));
                        }
                        let block = {
                            let i = state.frames.len() - 1 - (default as usize);
                            let frame = &mut state.frames[i];
                            frame.set_branched_to_exit();
                            frame.br_destination()
                        };
                        let block = builder.func.dfg.block_call(block, &[]);
                        let jt = builder.create_jump_table(JumpTableData::new(block, &data));
                        builder.ins().br_table(val, jt);
                    } else {
                        // Here we have jump arguments, but Cranelift's br_table doesn't support them
                        // We then proceed to split the edges going out of the br_table
                        let mut dest_block_sequence = vec![];
                        let mut dest_block_map = HashMap::new();
                        for depth in targets.targets() {
                            let depth = depth.unwrap();
                            let branch_block = match dest_block_map.entry(depth as usize) {
                                hash_map::Entry::Occupied(entry) => *entry.get(),
                                hash_map::Entry::Vacant(entry) => {
                                    let block = builder.create_block();
                                    dest_block_sequence.push((depth as usize, block));
                                    *entry.insert(block)
                                }
                            };
                            data.push(builder.func.dfg.block_call(branch_block, &[]));
                        }
                        let default_branch_block = match dest_block_map.entry(default as usize) {
                            hash_map::Entry::Occupied(entry) => *entry.get(),
                            hash_map::Entry::Vacant(entry) => {
                                let block = builder.create_block();
                                dest_block_sequence.push((default as usize, block));
                                *entry.insert(block)
                            }
                        };
                        let default_branch_block =
                            builder.func.dfg.block_call(default_branch_block, &[]);
                        let jt = builder
                            .create_jump_table(JumpTableData::new(default_branch_block, &data));
                        builder.ins().br_table(val, jt);
                        for (depth, dest_block) in dest_block_sequence {
                            builder.switch_to_block(dest_block);
                            builder.seal_block(dest_block);
                            let real_dest_block = {
                                let i = state.frames.len() - 1 - depth;
                                let frame = &mut state.frames[i];
                                frame.set_branched_to_exit();
                                frame.br_destination()
                            };
                            builder.ins().jump(real_dest_block, &[]);
                        }
                        // state.popn(return_count);
                    }
                    state.reachable = false;
                }
                Operator::Return => {
                    do_return(&mut builder, &mut state);
                    state.reachable = false;
                }
                /************************************ Calls ****************************************/
                Operator::Call { function_index } => {
                    // call through jump table
                    let func_idx = function_index as usize;
                    let func_ptr = builder.ins().load(
                        PTR_TYPE,
                        MemFlags::new(),
                        state.host_pointers.jump_table_ptr,
                        func_idx as i32 * PTR_TYPE_SIZE,
                    );

                    let mut sig = ir::Signature::new(CallConv::SystemV);
                    sig.params.push(AbiParam::new(I64));
                    sig.params.push(AbiParam::new(I32));
                    let sig_ref = builder.import_signature(sig.clone());

                    // create const function_index
                    let func_idx = builder.ins().iconst(I32, function_index as i64);
                    let callee_ty = self.get_fn_type(function_index);
                    let mut callee_sig = FuncSignature::default();
                    for param in callee_ty.params() {
                        callee_sig.params_type.push(*param);
                    }
                    for result in callee_ty.results() {
                        callee_sig.return_type.push(*result);
                    }
                    // print num of callee params
                    // println!("[callee-params] {:?}", callee_sig.params_type.len());

                    let params = from_stack(&mut state, callee_sig.params_type.clone());

                    to_frame(
                        &mut builder,
                        &mut state,
                        params,
                        callee_sig.params_type.clone(),
                    );

                    // create a callee from func_ptr and call it with empty arg and return
                    builder.ins().call_indirect(
                        sig_ref,
                        func_ptr,
                        &[state.host_pointers.vm_ptr, func_idx],
                    );

                    let return_val =
                        from_frame(&mut builder, &mut state, callee_sig.return_type.clone());

                    state.pushn(return_val.as_ref());
                }
                Operator::CallIndirect {
                    type_index,
                    ..
                } => {
                    let indirect_idx = state.pop1(); // 32

                    let indirect_idx = builder
                        .ins()
                        .imul_imm(indirect_idx, INDIRECT_TABLE_ITEM_TYPE_SIZE as i64); // 32
                    let indirect_idx = builder.ins().uextend(I64, indirect_idx); // 64
                    let indirect_idx = builder
                        .ins()
                        .iadd(state.host_pointers.indirect_table_ptr, indirect_idx); // 64

                    let callee_idx_32 = builder.ins().load(
                        INDIRECT_TABLE_ITEM_TYPE,
                        MemFlags::new(),
                        indirect_idx,
                        0,
                    );

                    // let callee_idx_32 = builder.ins().load(I32, MemFlags::new(), callee_idx_ptr, 0);
                    // to 64
                    // let callee_idx = builder.ins().uextend(I64, callee_idx);

                    //////////////////////////// duplicate ////////////////////////////
                    // func_idx = state.host_calls.jump_table_ptr + (callee_idx * PTR_TYPE_SIZE)
                    let func_idx = builder.ins().imul_imm(callee_idx_32, PTR_TYPE_SIZE as i64);
                    let func_idx_64 = builder.ins().uextend(I64, func_idx);
                    let func_idx = builder
                        .ins()
                        .iadd(state.host_pointers.jump_table_ptr, func_idx_64);
                    // call through jump table
                    // let func_idx = function_index as usize;
                    let func_ptr = builder.ins().load(PTR_TYPE, MemFlags::new(), func_idx, 0);

                    let mut sig = ir::Signature::new(CallConv::SystemV);
                    sig.params.push(AbiParam::new(I64));
                    sig.params.push(AbiParam::new(I32));
                    let sig_ref = builder.import_signature(sig.clone());

                    // create const function_index
                    let callee_ty = &self.types[type_index as usize];
                    let mut callee_sig = FuncSignature::default();
                    for param in callee_ty.params() {
                        callee_sig.params_type.push(*param);
                    }
                    for result in callee_ty.results() {
                        callee_sig.return_type.push(*result);
                    }
                    // print num of callee params
                    // println!("[callee-sig] {:?}", callee_sig);

                    let params = from_stack(&mut state, callee_sig.params_type.clone());

                    to_frame(
                        &mut builder,
                        &mut state,
                        params,
                        callee_sig.params_type.clone(),
                    );

                    // create a callee from func_ptr and call it with empty arg and return
                    builder.ins().call_indirect(
                        sig_ref,
                        func_ptr,
                        &[state.host_pointers.vm_ptr, callee_idx_32],
                    );

                    let return_val =
                        from_frame(&mut builder, &mut state, callee_sig.return_type.clone());

                    state.pushn(return_val.as_ref());
                }

                /********************************** Memory instructions **********************************
                 *  The memory instructions are all handled by the environment.
                 *****************************************************************************************/
                Operator::MemorySize { .. } => {
                    // i32(ptr)
                    let mut sig = ir::Signature::new(CallConv::SystemV);
                    sig.params.push(AbiParam::new(I64));
                    sig.returns.push(AbiParam::new(I32));
                    let sig_ref = builder.import_signature(sig.clone());

                    // create a callee from func_ptr and call it with empty arg and return
                    let call = builder.ins().call_indirect(
                        sig_ref,
                        state.host_calls.memory_size,
                        &[vm_ptr],
                    );
                    // Store the return values into `values_vec`.
                    let results = builder.func.dfg.inst_results(call);
                    state.pushn(results);
                }
                Operator::MemoryGrow { .. } => {
                    // u32(ptr, u32)
                    let mut sig = ir::Signature::new(CallConv::SystemV);
                    sig.params.push(AbiParam::new(I64));
                    sig.params.push(AbiParam::new(I32));
                    sig.returns.push(AbiParam::new(I32));
                    let sig_ref = builder.import_signature(sig.clone());

                    let delta = state.pop1();

                    // create a callee from func_ptr and call it with empty arg and return
                    let call = builder.ins().call_indirect(
                        sig_ref,
                        state.host_calls.memory_grow,
                        &[vm_ptr, delta],
                    );
                    // Store the return values into `values_vec`.
                    let results = builder.func.dfg.inst_results(call);
                    state.pushn(results);
                }
                Operator::MemoryCopy { .. } => {
                    // void(ptr, u32, u32, u32)
                    let mut sig = ir::Signature::new(CallConv::SystemV);
                    sig.params.push(AbiParam::new(I64));
                    sig.params.push(AbiParam::new(I32));
                    sig.params.push(AbiParam::new(I32));
                    sig.params.push(AbiParam::new(I32));
                    let sig_ref = builder.import_signature(sig.clone());

                    let len = state.pop1();
                    let src = state.pop1();
                    let dest = state.pop1();

                    // create a callee from func_ptr and call it with empty arg and return
                    builder.ins().call_indirect(
                        sig_ref,
                        state.host_calls.memory_copy,
                        &[vm_ptr, dest, src, len],
                    );
                }
                // mem_fill
                // void(ptr, u32, u32, u32)
                Operator::MemoryFill { .. } => {
                    let mut sig = ir::Signature::new(CallConv::SystemV);
                    sig.params.push(AbiParam::new(I64));
                    sig.params.push(AbiParam::new(I32));
                    sig.params.push(AbiParam::new(I32));
                    sig.params.push(AbiParam::new(I32));
                    let sig_ref = builder.import_signature(sig.clone());

                    let size = state.pop1();
                    let value = state.pop1();
                    let dest = state.pop1();

                    // create a callee from func_ptr and call it with empty arg and return
                    builder.ins().call_indirect(
                        sig_ref,
                        state.host_calls.memory_fill,
                        &[vm_ptr, dest, value, size],
                    );
                }
                _ => {
                    // println!("[todo] {:?}", o);
                }
            }
        }

        // print current block

        let current_block = builder.current_block().unwrap();
        // println!("[current-block] {:?}", current_block);

        // println!("---------------------------------");
        // println!("{}", builder.func.display());
        // println!("---------------------------------");

        // builder.seal_all_blocks();
        builder.finalize();

        ////////////// generating object file //////////////
        cl_module.define_function(cl_func, &mut ctx).unwrap();
        cl_module.clear_context(&mut ctx);

        // Perform linking.
        let obj = cl_module.finish();

        // Generate ELF object file.
        let obj = Compiler::emit(obj);

        // Compiler::inspect(&obj);

        obj
    }

    fn emit(func: ObjectProduct) -> Vec<u8> {
        func.emit().unwrap()
    }

    fn write_to_file(obj: &Vec<u8>, file: &str) {
        std::fs::write(file, &obj).unwrap();
    }

    fn objdump(file: &str) {
        let status = std::process::Command::new("objdump")
            .arg("-Mintel")
            .arg("-d")
            .arg(file)
            .status()
            .unwrap();
    }

    fn inspect(obj: &Vec<u8>) {
        let file = "/tmp/output.o";
        Compiler::write_to_file(obj, file);
        Compiler::objdump(file);
        std::fs::remove_file(file).unwrap();
    }
}

fn do_return(builder: &mut FunctionBuilder, state: &mut State) {
    let return_type = state.signature.return_type.clone();
    let ret = from_stack(state, return_type.clone());
    to_frame(builder, state, ret, return_type);
    builder.ins().return_(&[]);
}
