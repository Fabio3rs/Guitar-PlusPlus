/*****************************************************************************************************
 *		GUITAR++
 *		PROGRAMADO POR FÁBIO
 *		BMS - Brazilian Modding Studio -
 *http://brmodstudio.forumeiros.com
 *****************************************************************************************************/
#pragma once
#include <cstddef>
#include <array>
#include <mutex>
#ifndef LOGGING_SYSTEM_CLOG_H
#define LOGGING_SYSTEM_CLOG_H
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <exception>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

class CLog {
    std::string FileName;
    std::ofstream LogFile;
    std::mutex writeMutex;
    bool useStderr = false;

    // Caller holds writeMutex (or is constructing this instance).
    void writeLocked(const std::string &text);

    class argToString {
        const std::string str;

      public:
        const std::string &getStr() const { return str; }

        argToString(bool value) : str(value ? "true" : "false") {}

        argToString(const char *str) : str(str ? str : "(null)") {}

        argToString(const std::exception &e) : str(e.what()) {}

        argToString(const std::string &str) : str(str) {}

        template <class T,
                  typename = std::enable_if_t<std::is_arithmetic<T>::value>>
        argToString(T value) : str(std::to_string(value)) {}
    };

    bool Finished;

  public:
    ~CLog() noexcept;
    void AddToLog(const std::string &Text);

    template <class... Types>
    std::string multiRegister(const std::string &format, Types &&... args) {
        const std::array<argToString, sizeof...(Types)> a = {std::forward<Types>(args)...};
        std::string printbuf;
        printbuf.reserve(format.size());
        for (size_t i = 0; i < format.size();) {
            if (format[i] == '\\' && i + 1 < format.size() &&
                (format[i + 1] == '%' || format[i + 1] == '\\')) {
                printbuf += format[i + 1];
                i += 2;
            } else if (format[i] == '%' && i + 1 < format.size() &&
                       format[i + 1] >= '0' && format[i + 1] <= '9') {
                const size_t start = i++;
                size_t argument = 0;
                // Saturate to an invalid index; arbitrarily long tokens never throw.
                while (i < format.size() && format[i] >= '0' && format[i] <= '9') {
                    const size_t digit = static_cast<size_t>(format[i++] - '0');
                    if (argument <= a.size() / 10) {
                        argument = argument * 10 + digit;
                    } else {
                        argument = a.size();
                    }
                    if (argument >= a.size()) argument = a.size();
                }
                if (argument < a.size()) printbuf += a[argument].getStr();
                else printbuf.append(format, start, i - start);
            } else {
                printbuf += format[i++];
            }
        }

        AddToLog(printbuf);
        return printbuf;
    }

    void FinishLog();
    void SaveBuffer();
    void operator<<(const std::string &Text) { AddToLog(Text); }
    static std::string GetDateAndTime();

    static CLog &log();
    // Startup-only tool hook: call before the first log(), on the main thread.
    static void configureStderr();

    CLog(const CLog &) = delete;

    explicit CLog(const std::string &NameOfFile);
};
#endif
