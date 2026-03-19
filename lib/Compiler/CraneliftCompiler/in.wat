(module
    (import "wasi_snapshot_preview1" "args_get" (func $__imported_wasi_snapshot_preview1_args_get (type 0)))
    (func $_start (result i32)
;;        memory.size
        i32.const 1
        memory.grow
        memory.grow
        memory.grow
        memory.grow
;;        drop
    )

    (func $f (param i32 i32 i32 i32) (result i32) ;; assume always return signle i32
        local.get 0
        local.get 1
        i32.add
        local.get 2
        i32.add
        local.get 3
        i32.add
        return
    )

    (memory 2)
    (export "memory" (memory 0))
    (export "_start" (func $_start))
    (global $g0 (mut i32) (i32.const 0))
    (global $g1 (mut i32) (i32.const 0))
    (global $g2 (mut i32) (i32.const 0))
    (global $g3 (mut i32) (i32.const 15))
)
