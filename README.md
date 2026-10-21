# Topic 03: System-Call Tracer (mini strace)

CS-330/333 Operating Systems, Semester Project (NUST)
Members: Abdullah Jan (Reg. No: ________), <Partner name> (Reg. No: ________)

## What it does
Runs a command under `ptrace()` and prints every system call with its name,
six raw argument registers (hex) and return value, in the style of `strace`.
At the end it prints a summary table of calls and errors per syscall.
Trace output goes to **stderr**; the traced program's own output is untouched.

## Requirements
- Linux x86-64 (tested on Kali Linux VM), g++ with C++17
- No root needed (tracer only traces its own child)

## Build
```bash
make
```
(or: `g++ -Wall -Wextra -std=c++17 -o mytrace src/main.cpp`)

If the folder is a VM shared folder and the binary will not run, build elsewhere:
`g++ -Wall -Wextra -std=c++17 -o ~/mytrace src/main.cpp`

## Run
```bash
./mytrace <command> [args...]
./mytrace ls /tmp
./mytrace ls /nonexistent > out.txt 2>&1      # save the trace
```

## Sample output
```
openat(0xffffff9c, 0x7f..., 0x80000, 0x0, ...) = 3
access(0x7f..., 0x4, ...) = -1 (No such file or directory)
exit_group(0x0, ...) = ?
+++ exited with 0 +++

syscall           calls   errors
access                1        1
...
total                30        1
```

## Test inputs (outputs saved in tests/)
| Command | Purpose | Output file |
|---|---|---|
| `./mytrace true` | minimal program (30 syscalls) | tests/true_output.txt |
| `./mytrace ls /nonexistent` | failing syscalls, exit code 2 | tests/ls_error_output.txt |
| `./mytrace sleep 1` | blocking syscall (clock_nanosleep) | tests/sleep_output.txt |
| `./mytrace ./tests/crash` | child killed by SIGSEGV (signal handling) | tests/crash_output.txt |
| `./mytrace ls -R /usr/include` | large trace (2973 syscalls) | tests/big_output.txt |
| `./mytrace nosuchcmd` | exec failure | tests/badcmd_output.txt |
| `./mytrace` | no arguments, usage message | tests/noargs_output.txt |

`tests/crash.cpp` is built with `g++ -o tests/crash tests/crash.cpp`.

## How it works (short)
1. `fork()`; child calls `PTRACE_TRACEME` then `execvp()`.
2. Parent sets `PTRACE_O_TRACESYSGOOD`, then loops on `PTRACE_SYSCALL` + `waitpid()`.
3. Each syscall causes two stops (entry, exit). Entry: read `orig_rax` (number) and
   `rdi, rsi, rdx, r10, r8, r9` (arguments) with `PTRACE_GETREGS`. Exit: read `rax`
   (return value; values in -4095..-1 are errors, `-rax` is errno).
4. Stops with signal number `SIGTRAP|0x80` are syscall stops. Any other stop is a
   signal; it is printed and passed to the child (not swallowed).
5. Syscall names come from `src/syscall_names.h`, generated from
   `/usr/include/x86_64-linux-gnu/asm/unistd_64.h`.

## Known limitations
- x86-64 only; arguments are printed as raw hex (no string/struct decoding).
- Follows only the main process (no `-f` for forks/threads).
- Ctrl+C reaches both tracer and child, so no summary is printed.
- `exit_group` never returns, so it is printed with `= ?`.

## Who wrote what
- Abdullah Jan: ________ (e.g. tracer loop, register reading, summary table)
- <Partner name>: ________ (e.g. tests, signal handling, report/slides)

## References
- `man 2 ptrace`, `man 2 waitpid`, `man 2 execve`
- Silberschatz, Galvin, Gagne: Operating System Concepts, system-call chapter
  (system calls / types of system calls section)
- Linux `unistd_64.h` (syscall numbers)