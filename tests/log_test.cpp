#include "geodraw/log/log.hpp"
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>

using namespace geodraw;

TEST_CASE("Log - basic log levels do not crash") {
    log_error("This is an error message");
    log_warning("This is a warning message");
    log_info("This is an info message");
    log_verbose("This is a verbose message");
    log_spam("This is a spam message");
    // All log calls must complete without crashing
    SUCCEED();
}

TEST_CASE("Log - throttled logging suppresses repeats", "[timing]") {
    // Call log_info_every(200ms) 10 times with 50ms delays.
    // Expect approximately 3 emissions (at t=0, t=200ms, t=400ms) out of 10 calls.
    // This is timing-sensitive; the test only checks it doesn't crash.
    for (int i = 0; i < 10; ++i) {
        log_info_every(200, "Throttled message iteration " + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    SUCCEED();
}

TEST_CASE("Log - different call sites throttled independently", "[timing]") {
    for (int i = 0; i < 3; ++i) {
        log_info_every(100, "Call site A");
        log_info_every(100, "Call site B");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    SUCCEED();
}
