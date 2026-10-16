#include "synth_process.h"
#include <chrono>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
namespace ChartStudio {
void synthProcess(const std::vector<std::string> &args,
                  const std::function<void()> &check) {
#ifdef _WIN32
    std::wstring command;
    for (const auto &arg : args) {
        auto wide = fs::u8path(arg).wstring();
        command += L"\"";
        size_t slashes = 0;
        for (wchar_t ch : wide) {
            if (ch == L'\\') {
                ++slashes;
                continue;
            }
            command.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
            slashes = 0;
            command += ch;
        }
        command.append(slashes * 2, L'\\');
        command += L"\" ";
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                        &process)) {
        throw std::runtime_error(
            "Cannot start FFmpeg; install it on PATH or use WAV");
    }
    CloseHandle(process.hThread);
    try {
        while (WaitForSingleObject(process.hProcess, 20) == WAIT_TIMEOUT) {
            if (check) {
                check();
            }
        }
        DWORD status = 1;
        GetExitCodeProcess(process.hProcess, &status);
        if (status) {
            throw std::runtime_error("FFmpeg encoding failed");
        }
    } catch (...) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hProcess);
        throw;
    }
    CloseHandle(process.hProcess);
#else
    std::vector<char *> argv;
    argv.reserve(args.size() + 1);
    for (const auto &a : args) {
        argv.push_back(const_cast<char *>(a.c_str()));
    }
    argv.push_back(nullptr);
    pid_t pid = 0;
    if (posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ)) {
        throw std::runtime_error(
            "Cannot start FFmpeg; install it on PATH or use WAV");
    }
    try {
        int status = 0;
        for (;;) {
            auto result = waitpid(pid, &status, WNOHANG);
            if (result == pid) {
                break;
            }
            if (result < 0 && errno != EINTR) {
                throw std::runtime_error("Cannot wait for FFmpeg");
            }
            if (check) {
                check();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        pid = 0;
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            throw std::runtime_error("FFmpeg encoding failed");
        }
    } catch (...) {
        if (pid > 0) {
            kill(pid, SIGKILL);
            while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
            }
        }
        throw;
    }
#endif
}
} // namespace ChartStudio
