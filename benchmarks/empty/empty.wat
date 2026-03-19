(module
    (func $_start (result i32)
;;    i32.const 15
;;   global.set $g3
   global.get $g3
    )

    (memory 2)
    (export "memory" (memory 0))
    (export "_start" (func $_start))
    (global $g0 (mut i32) (i32.const 0))
    (global $g1 (mut i32) (i32.const 0))
    (global $g2 (mut i32) (i32.const 0))
    (global $g3 (mut i32) (i32.const 15))
)
