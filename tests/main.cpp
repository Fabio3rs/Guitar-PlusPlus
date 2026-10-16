#include "CLog.h"
#include <gtest/gtest.h>

int main(int argc, char **argv) {
    // Fixtures can change cwd and delete it in TearDown. The process-wide
    // logger must not keep a file open inside a fixture's temporary directory.
    CLog::configureStderr();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
