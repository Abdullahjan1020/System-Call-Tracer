// mini strace using ptrace() - Topic 03 System-Call Tracer (x86-64 Linux)
// Trace output goes to stderr (unbuffered, so it never gets mixed up with
// the child's output). References: man 2 ptrace, man 2 waitpid.

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <map>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

// syscall number -> name table (generated from unistd_64.h)
std::map<int, std::string> syscallNames = {
#include "syscall_names.h"
};

// Number se naam nikalta hai; agar table mein na ho to "unknown_N"
std::string getName(long nr) {
    std::map<int, std::string>::iterator it = syscallNames.find((int)nr);
    if (it != syscallNames.end()) return it->second;
    return "unknown_" + std::to_string(nr);
}

// Summary table: har syscall ki calls aur errors ginti
void printSummary(std::map<std::string, int>& calls,
                  std::map<std::string, int>& errors,
                  int total, int totalErrors) {
    fprintf(stderr, "\n%-16s %6s %8s\n", "syscall", "calls", "errors");
    for (std::map<std::string, int>::iterator it = calls.begin(); it != calls.end(); ++it) {
        fprintf(stderr, "%-16s %6d %8d\n", it->first.c_str(), it->second, errors[it->first]);
    }
    fprintf(stderr, "%-16s %6d %8d\n", "total", total, totalErrors);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Child: "mujhe trace karo" bol ke program chalao
        ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
        execvp(argv[1], &argv[1]);
        perror("execvp");
        _exit(1);
    }

    // Parent: child exec pe ek baar ruk jata hai
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        // execvp fail hua (command nahi mili), trace karne ko kuch nahi
        return 1;
    }

    // Syscall stops pe signal number SIGTRAP|0x80 aayega, taake
    // syscall stop aur asli signal stop mein 100% farak ho sake
    ptrace(PTRACE_SETOPTIONS, pid, nullptr, PTRACE_O_TRACESYSGOOD);

    std::map<std::string, int> calls;
    std::map<std::string, int> errors;
    int total = 0;
    int totalErrors = 0;

    bool inSyscall = false;            // true = abhi entry ho chuki, exit ka intezaar
    std::string pendingName;           // entry pe save, exit pe print
    unsigned long long args[6] = {0, 0, 0, 0, 0, 0};
    int sigToDeliver = 0;              // child ko wapas dene wala signal

    while (true) {
        // Agla syscall entry/exit tak chalao; signal ho to child ko pass karo
        ptrace(PTRACE_SYSCALL, pid, nullptr, (void*)(long)sigToDeliver);
        sigToDeliver = 0;

        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            if (inSyscall) {
                // exit_group kabhi return nahi hota
                fprintf(stderr, ") = ?\n");
            }
            fprintf(stderr, "+++ exited with %d +++\n", WEXITSTATUS(status));
            break;
        }
        if (WIFSIGNALED(status)) {
            if (inSyscall) fprintf(stderr, ") = ?\n");
            fprintf(stderr, "+++ killed by %s +++\n", strsignal(WTERMSIG(status)));
            break;
        }
        if (!WIFSTOPPED(status)) continue;

        int sig = WSTOPSIG(status);

        if (sig == (SIGTRAP | 0x80)) {
            // ---- Asli syscall stop ----
            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, nullptr, &regs);

            if (!inSyscall) {
                // ENTRY: naam aur arguments save karo, abhi print nahi
                inSyscall = true;
                pendingName = getName((long)regs.orig_rax);
                args[0] = regs.rdi;
                args[1] = regs.rsi;
                args[2] = regs.rdx;
                args[3] = regs.r10;
                args[4] = regs.r8;
                args[5] = regs.r9;
                // Line ka pehla hissa abhi print, return value exit pe
                fprintf(stderr, "%s(0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx",
                        pendingName.c_str(), args[0], args[1], args[2],
                        args[3], args[4], args[5]);
            } else {
                // EXIT: return value rax mein
                inSyscall = false;
                long long ret = (long long)regs.rax;
                calls[pendingName]++;
                total++;

                if (ret < 0 && ret > -4096) {
                    // Error: -ret hi errno hai
                    errors[pendingName]++;
                    totalErrors++;
                    fprintf(stderr, ") = -1 (%s)\n", strerror((int)-ret));
                } else if (pendingName == "mmap" || pendingName == "brk" ||
                           pendingName == "mremap") {
                    // Pointer wali calls hex mein
                    fprintf(stderr, ") = 0x%llx\n", (unsigned long long)ret);
                } else {
                    fprintf(stderr, ") = %lld\n", ret);
                }
            }
        } else {
            // ---- Signal stop (syscall nahi) ----
            // inSyscall ko toggle NAHI karna. Signal child ko wapas pass karo.
            fprintf(stderr, "--- %s ---\n", strsignal(sig));
            sigToDeliver = sig;
        }
    }

    // exit_group ka exit stop kabhi nahi aata, lekin wo ek call to hui hai
    if (inSyscall) {
        calls[pendingName]++;
        total++;
    }

    printSummary(calls, errors, total, totalErrors);
    return 0;
}