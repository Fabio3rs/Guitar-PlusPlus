#include "CLog.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <thread>
#include <vector>

namespace {
class Logging : public ::testing::Test {
  protected:
    std::filesystem::path directory;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("gpp-log-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(directory);
    }
    void TearDown() override { std::filesystem::remove_all(directory); }
};
}

TEST_F(Logging, FormatsPlaceholdersWithoutLosingLiteralText) {
    CLog log((directory / "test.log").string());
    EXPECT_EQ(log.multiRegister("%0 %1 %2", "loading", true, 42), "loading true 42");
    EXPECT_EQ(log.multiRegister("100% done %x % %9", "unused"), "100% done %x % %9");
    EXPECT_EQ(log.multiRegister("\\%0 \\\\ %0", "value"), "%0 \\ value");
    EXPECT_EQ(log.multiRegister("%9999999999999999999999999999", 1),
              "%9999999999999999999999999999");
    EXPECT_EQ(log.multiRegister("%0"), "%0");
    EXPECT_EQ(log.multiRegister("%0", static_cast<const char *>(nullptr)), "(null)");
}

TEST_F(Logging, ConcurrentWritersProduceCompleteRecords) {
    const auto filename = directory / "test.log";
    {
        CLog log(filename.string());
        std::vector<std::thread> writers;
        for (int worker = 0; worker < 4; ++worker) {
            writers.emplace_back([&log, worker]() {
                for (int entry = 0; entry < 100; ++entry)
                    log.multiRegister("record:%0:%1:end", worker, entry);
            });
        }
        for (auto &writer : writers) writer.join();
        log.SaveBuffer();
        log.FinishLog();
        log.FinishLog();
    }
    std::ifstream input(filename);
    std::set<std::string> records;
    int finishes = 0;
    for (std::string line; std::getline(input, line);) {
        const auto start = line.find("record:");
        if (start != std::string::npos) {
            const auto record = line.substr(start);
            EXPECT_EQ(record.substr(record.size() - 4), ":end");
            EXPECT_TRUE(records.insert(record).second);
        }
        if (line.find("Log finished at:") != std::string::npos) ++finishes;
    }
    EXPECT_EQ(finishes, 1);
    for (int worker = 0; worker < 4; ++worker)
        for (int entry = 0; entry < 100; ++entry)
            EXPECT_EQ(records.count("record:" + std::to_string(worker) + ":" +
                                    std::to_string(entry) + ":end"), 1u);
    EXPECT_EQ(records.size(), 400u);
}

TEST_F(Logging, UnwritableFileFallsBackToStderr) {
    ::testing::internal::CaptureStderr();
    {
        // Opening a directory as a file fails even with elevated privileges.
        CLog log(directory.string());
        log << "loading failure remains visible";
        log.SaveBuffer();
    }
    const auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("using stderr"), std::string::npos);
    EXPECT_NE(output.find("loading failure remains visible"), std::string::npos);
}
