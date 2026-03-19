import os
from dataclasses import dataclass


@dataclass
class VirtualMachine:
    name: str
    command: list[str]


HOME = os.path.expanduser("~")
virtual_machines = [
    VirtualMachine("truejit", ["/truejit/build/tools/vm-without-compiler"]),
    VirtualMachine(
        "wasmtime-cranelift",
        [f"{HOME}/.wasmtime/bin/wasmtime", "run", "-C", "compiler=cranelift", "-C", "parallel-compilation=no", "-C", "cache=no"],
    ),
    VirtualMachine("wasmer-single-pass", [f"{HOME}/.wasmer/bin/wasmer", "run", "--singlepass"]),
    VirtualMachine("wasmer-cranelift", [f"{HOME}/.wasmer/bin/wasmer", "run", "--cranelift"]),
    VirtualMachine("wasmer-llvm", [f"{HOME}/.wasmer/bin/wasmer", "run", "--llvm"]),
    VirtualMachine("wazero", ["/usr/local/bin/wazero", "run"]),
    VirtualMachine("wavm", ["/usr/bin/wavm", "run"]),
    VirtualMachine("graalwasm", ["/usr/local/bin/graalwasm"]),
]
