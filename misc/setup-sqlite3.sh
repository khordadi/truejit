#!/usr/bin/env bash

cd /tmp
rm -rf sqlite
wget https://www.sqlite.org/2025/sqlite-amalgamation-3490100.zip
unzip *.zip
rm *.zip
mv sqlite* sqlite
cd sqlite
clang shell.c sqlite3.c -lpthread -ldl -lm -o sqlite3
clang -emit-llvm -S -O0 sqlite3.c -o sqlite3.ll
clang -emit-llvm -S -O0 shell.c -o shell.ll
llvm-link sqlite3.ll shell.ll -o sqlite3.ll


/opt/wasi-sdk/bin/clang -D_WASI_EMULATED_GETPID -lwasi-emulated-getpid -D_WASI_EMULATED_SIGNAL -lwasi-emulated-signal -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks shell.c sqlite3.c -lpthread -ldl -lm -o sqlite3


tee ~/gdb.py <<EOF
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
EOF

gdb --batch-silent --command=~/gdb.py --args ./sqlite3 --version

tee in.sql <<EOF
-- Create a table to store user information
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL,
    email TEXT NOT NULL,
    age INTEGER
);

-- Insert some users into the table
INSERT INTO users (username, email, age) VALUES ('john_doe', 'john@example.com', 28);
INSERT INTO users (username, email, age) VALUES ('jane_smith', 'jane@example.com', 32);
INSERT INTO users (username, email, age) VALUES ('alice_jones', 'alice@example.com', 24);

-- Update a user's age
UPDATE users SET age = 29 WHERE id = 1;

-- Query all users
SELECT * FROM users;

-- Delete a user
DELETE FROM users WHERE id = 2;
EOF

./sqlite3 <~/in.sql
