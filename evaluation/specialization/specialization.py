import os
import sys
import subprocess
import time
from os import abort


def main():
    print(f'{"":>15}\t#IR lines     compile (s)   exec (s)')
    for specialized_arg in [i * 4096 for i in [0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096]]:
        specialized_code = """
#include <stdio.h>
#include <stdlib.h>
#define ITER 10000ul
#define MEM_SIZE 1000000ul

void f(unsigned long arg, int* mem);

int main() {
    int *mem = (int*) malloc(MEM_SIZE * sizeof(int));
    for (unsigned long opt = 0; opt < ITER; opt++)"""

        specialized_code += f"""
        f({specialized_arg}, mem);"""

        specialized_code += """
    return 0;
}"""

        specialized_code += """
void f(unsigned long arg, int* mem) {"""
        specialized_code += f"""
    arg = {specialized_arg};"""
        specialized_code += """
    unsigned long f_idx = 0;"""
        for i in range(10):
            specialized_code += f"""
    for (unsigned long opt = 0; opt < arg; ++opt) mem[f_idx++ % MEM_SIZE] = (int) f_idx; // loop #{i}"""
        specialized_code += """
    return;
}
"""

        # print(specialized_code)

        # print("*********************************************")

        # remove substring f"arg = {specialized_arg};" from specialized_code
        generic_code = specialized_code.replace(f"arg = {specialized_arg};", "")
        # print(specialized_code)
        # print("*********************************************")
        # print(generic_code)

        print(f"arg={specialized_arg}")
        codes = {'generic': generic_code, 'specialized': specialized_code}
        for key, code in codes.items():
            # write the code to a file
            with open(f"{key}.c", "w") as f:
                f.write(code)

            subprocess.run(["clang", "-Wno-unsequenced", "-O3", "-S", "-emit-llvm", "-o", f"{key}.ll", f"{key}.c"], check=True)
            with open(f"{key}.ll", "r") as f:
                ir_lines = f.readlines()

            # compile and run the specialized_code
            start = time.time()
            subprocess.run(["clang", "-Wno-unsequenced", "-O3", "-o", f"{key}", f"{key}.c"], check=True)
            end = time.time()
            compile_time = end - start

            start = time.time()
            subprocess.run([f"./{key}"], check=True)
            end = time.time()

            exec_time = end - start

            print(f'{key:<15}\t{len(ir_lines):<14}{compile_time:<14.2f}{exec_time:<14.4f}')

        # print()


if __name__ == '__main__':
    main()
