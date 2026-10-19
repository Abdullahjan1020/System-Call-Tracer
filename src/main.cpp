#include <array>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

int main(int argc, char* argv[]) {
    std::map<int, std::string> syscallNames = {
#include "syscall_names.h"
    };

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> [args...]\n";
        return 1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child: "mujhe trace karo" bol ke program chalao
        ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
        execvp(argv[1], &argv[1]);
        perror("execvp");
        return 1;
    }

    // Parent: child exec pe ek baar ruk jata hai (SIGTRAP)
    int status;
    waitpid(pid, &status, 0);
    bool inSyscall = false;
    std::string pendingSyscallName;
    std::array<unsigned long long, 6> pendingArguments{};
    std::map<std::string, int> syscallCalls;
    std::map<std::string, int> syscallErrors;

    while (true) {
        ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            if (inSyscall) {
                ++syscallCalls[pendingSyscallName];
                std::cout << pendingSyscallName << '(';
                for (size_t i = 0; i < pendingArguments.size(); ++i) {
                    if (i != 0) {
                        std::cout << ", ";
                    }
                    std::cout << "0x" << std::hex << pendingArguments[i]
                              << std::dec;
                }
                std::cout << ") = ?" << '\n';
            }
            break;
        }

        if (!inSyscall) {
            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
            const int syscallNumber = static_cast<int>(regs.orig_rax);
            const auto syscallName = syscallNames.find(syscallNumber);
            pendingSyscallName =
                syscallName != syscallNames.end()
                    ? syscallName->second
                    : "unknown_" + std::to_string(syscallNumber);
            pendingArguments = {
                regs.rdi, regs.rsi, regs.rdx, regs.r10, regs.r8, regs.r9};
        } else {
            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
            const long long returnValue = static_cast<long long>(regs.rax);
            ++syscallCalls[pendingSyscallName];

            std::cout << pendingSyscallName << '(';
            for (size_t i = 0; i < pendingArguments.size(); ++i) {
                if (i != 0) {
                    std::cout << ", ";
                }
                std::cout << "0x" << std::hex << pendingArguments[i]
                          << std::dec;
            }
            std::cout << ") = ";
            if (returnValue < 0 && returnValue >= -4095) {
                ++syscallErrors[pendingSyscallName];
                std::cout << "-1 (" << std::strerror(
                    static_cast<int>(-returnValue)) << ")\n";
            } else {
                std::cout << returnValue << '\n';
            }
        }
        inSyscall = !inSyscall;
    }

    int totalCalls = 0;
    int totalErrors = 0;
    std::printf("\n%-15s %5s %8s\n", "syscall", "calls", "errors");
    for (const auto& [name, calls] : syscallCalls) {
        const int errors = syscallErrors[name];
        std::printf("%-15s %5d %8d\n", name.c_str(), calls, errors);
        totalCalls += calls;
        totalErrors += errors;
    }
    std::printf("%-15s %5d %8d\n", "total", totalCalls, totalErrors);
    return 0;
}