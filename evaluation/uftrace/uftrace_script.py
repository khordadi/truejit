import sys
import json

functions = []
functions_set = set()


def uftrace_begin(ctx):
    pass


def uftrace_entry(function):
    name = function["name"]
    if name not in functions_set:
        functions_set.add(name)
        timestamp = function["timestamp"]
        functions.append((timestamp, name))


def uftrace_end():
    global functions
    functions = [(timestamp, name) for (timestamp, name) in functions if
                 not name.endswith(".cpp") and not name.endswith(".c")]
    functions = json.dumps(functions)
    print(functions, file=sys.stderr)
