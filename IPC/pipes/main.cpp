#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

int main() {
    int pipefd[2]{};

    if (pipe(pipefd) == -1) {
        std::cerr << "pipe failed: " << std::strerror(errno) << '\n';
        return 1;
    }

    pid_t pid{fork()};

    if (pid == -1) {
        std::cerr << "fork failed: " << std::strerror(errno) << '\n';
        return 1;
    }

    if (pid == 0) {
        // Child process: read from the pipe.
        close(pipefd[1]); // Close unused write end.

        char buffer[128]{};
        ssize_t bytesRead{read(pipefd[0], buffer, sizeof(buffer) - 1)};

        if (bytesRead == -1) {
            std::cerr << "child read failed: " << std::strerror(errno) << '\n';
            close(pipefd[0]);
            return 1;
        }

        std::cout << "Child received: " << buffer << '\n';

        close(pipefd[0]);
        return 0;
    }

    // Parent process: write to the pipe.
    close(pipefd[0]); // Close unused read end.

    std::string message{"Hello from the parent process via a Unix pipe."};

    ssize_t bytesWritten{write(pipefd[1], message.c_str(), message.size())};

    if (bytesWritten == -1) {
        std::cerr << "parent write failed: " << std::strerror(errno) << '\n';
        close(pipefd[1]);
        return 1;
    }

    close(pipefd[1]); // Signals EOF to the child.

    int status{};
    waitpid(pid, &status, 0);

    std::cout << "Parent: child exited.\n";

    return 0;
}