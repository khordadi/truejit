#!/usr/bin/env python3
import json
import sys
import gdb

function_names = []
gdb.execute('rbreak', to_string=True)
gdb.execute('run', to_string=True)
try:
    while True:
        gdb_output = gdb.execute('continue', to_string=True)
        register_info = gdb.execute('info registers rip', to_string=True)
        breakpoint_address = register_info.split()[1][2:]
        function_name = register_info.split()[3]
        breakpoint_info = gdb.execute('info breakpoints', to_string=True).split('\n')
        matching_breakpoints = [s for s in breakpoint_info if breakpoint_address in s]
        if len(matching_breakpoints) > 1:
            sys.exit(1)
        breakpoint_number = matching_breakpoints[0].split()[0].split('.')[0]
        gdb.execute('disable breakpoints ' + breakpoint_number)
        function_names.append(function_name)
except:
    pass

# write json.dumps(function_names) to stderr
print(json.dumps(function_names), file=sys.stderr)
