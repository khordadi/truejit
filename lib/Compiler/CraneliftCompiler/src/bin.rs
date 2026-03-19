use crate::compiler::{wat2wasm, Compiler};

mod compiler;
fn main() {
    let wat = "in.wat";
    let wat = std::path::Path::new(wat);

    let application = wat2wasm(wat).to_str().unwrap().to_string();
    let compiler = Compiler::new(application);

    let vec = compiler.compile(1, 1, false);
    println!("{:?}", vec);
    println!()
}
