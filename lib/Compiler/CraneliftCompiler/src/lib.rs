mod compiler;

use crate::compiler::Compiler;
use std::ffi::{c_char, CStr};

#[repr(C)]
pub struct Vector {
    data: *mut u8,
    size: usize,
}

impl From<Vec<u8>> for Vector {
    fn from(vec: Vec<u8>) -> Self {
        let size = vec.len();
        let data = vec.as_ptr() as *mut u8;
        std::mem::forget(vec);
        Vector { data, size }
    }
}
#[no_mangle]
pub fn compiler_constructor(application: *const c_char) -> *mut Compiler<'static> {
    let application = unsafe { CStr::from_ptr(application) }
        .to_string_lossy()
        .into_owned();
    let compiler = Compiler::new(application);
    Box::into_raw(Box::new(compiler))
}

#[no_mangle]
pub fn compile(
    compiler: *mut Compiler,
    func: u32,
    opt_level: u32,
    disable_hardware_features: bool,
) -> *mut Vector {
    let compiler = unsafe { &mut *compiler };
    let vector = compiler.compile(func, opt_level, disable_hardware_features);
    Box::into_raw(Box::new(Vector::from(vector)))
}

#[no_mangle]
pub fn compiler_destructor(compiler: *mut Compiler) {
    unsafe {
        let _ = Box::from_raw(compiler);
    };
}
