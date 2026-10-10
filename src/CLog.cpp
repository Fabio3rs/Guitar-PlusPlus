/*****************************************************************************************************
 *		GUITAR++
 *		PROGRAMADO POR FÁBIO
 *		BMS - Brazilian Modding Studio -
 *http://brmodstudio.forumeiros.com
 *****************************************************************************************************/
#include "CLog.h"
#include <cstring>

#ifdef _WIN32
auto InstallExceptionCatcher(void (*cb)(const char *buffer)) -> int;
#endif

auto CLog::log() -> CLog & {
    static CLog Log("Guitar++.log");
#ifdef _WIN32
    static const int installed = InstallExceptionCatcher([](const char *buffer) {
        // A crash can interrupt a thread holding writeMutex. Use a separate,
        // best-effort file rather than entering the normal logger again.
        if (FILE *file = std::fopen("Guitar++.crash.log", "ab")) {
            std::fwrite(buffer, 1, std::strlen(buffer), file);
            std::fclose(file);
        } else {
            std::fputs(buffer, stderr);
        }
    });
    (void)installed;
#endif
    return Log;
}

CLog::CLog(const std::string &NameOfFile)
    : FileName(NameOfFile), Finished(false) {
    // Preserve the existing unbuffered file behavior for loading diagnostics.
    LogFile.rdbuf()->pubsetbuf(nullptr, 0);
    LogFile.open(NameOfFile, std::ios::out | std::ios::trunc);
    writeLocked(std::string("* Guitar++ compilation date/time: ") +
                __DATE__ + " " + __TIME__ + "\n* Log started at: " +
                GetDateAndTime() + "\n");
}

CLog::~CLog() noexcept {
    try {
        FinishLog();
    } catch (...) {
        // Never throw from shutdown while another exception is being handled.
    }
}

auto CLog::GetDateAndTime() -> std::string {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now) != 0) return "unknown time";
#else
    if (!localtime_r(&now, &local)) return "unknown time";
#endif
    char text[64]{};
    if (std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local) == 0)
        return "unknown time";
    return text;
}

void CLog::writeLocked(const std::string &text) {
    if (!useStderr) {
        if (LogFile.good()) {
            LogFile.write(text.data(), static_cast<std::streamsize>(text.size()));
            if (LogFile.good()) return;
        }
        useStderr = true;
        std::fprintf(stderr, "Cannot write log '%s'; using stderr\n", FileName.c_str());
    }
    std::fwrite(text.data(), 1, text.size(), stderr);
}

void CLog::AddToLog(const std::string &Text) {
    const std::string entry = GetDateAndTime() + ": " + Text + "\n";
    std::lock_guard<std::mutex> lock(writeMutex);
    if (!Finished) writeLocked(entry);
}

void CLog::FinishLog() {
    std::lock_guard<std::mutex> lock(writeMutex);
    if (Finished) return;
    writeLocked("* Log finished at: " + GetDateAndTime() + "\n");
    if (!useStderr) LogFile.flush();
    else std::fflush(stderr);
    Finished = true;
}

void CLog::SaveBuffer() {
    std::lock_guard<std::mutex> lock(writeMutex);
    if (!useStderr) {
        LogFile.flush();
        if (!LogFile.good()) writeLocked("Log flush failed\n");
    } else {
        std::fflush(stderr);
    }
}
