#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

#include <gtest/gtest.h>

#include "test_utils.h"

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);

    bool verbose;
    ParseCommandLineParams(argc, argv, verbose);

    plog::ConsoleAppender<plog::TxtFormatter> appender;
    plog::init(verbose ? plog::Severity::debug : plog::Severity::info);

    return RUN_ALL_TESTS();
}
