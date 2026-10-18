#include <iostream>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

int main(int argc, char* argv[]) {
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
    std::cout << "Child stopped, ready to trace\n";

    ptrace(PTRACE_CONT, pid, nullptr, nullptr);  // abhi bas chalne do
    waitpid(pid, &status, 0);
    return 0;
}