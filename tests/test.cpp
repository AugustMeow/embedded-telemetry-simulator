#include "telemetry/validator.h"
#include "telemetry/sensor.h"
#include "telemetry/logger.h"
#include <cstdlib>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
volatile std::sig_atomic_t running = 1;
void handle_signal(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        running = 0;
    }
}

void test_log(const std::string path){
    #if 0
    Logger log;
    bool ret = log.open(path);
    if(ret){
        log.set_level(LogLevel::Info);
        log.info("recive message from info");
        log.warn("recive message from warn");
        log.error("recive message from error");
    }
    #endif
}

int main()
{
    int failed = 0;
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    //const std::string path = "log/telemetry.log";
    //test_log(path);
    // 最大 1 MB，保留 3 个归档文件
    
    LoggerConfig config;
    Logger logger;

    config.file_path = "shutdown_test.log";
    config.max_files = 1;
    config.max_size = 100 * 1024*1024;
    
    logger.init(config);

    for (int i = 0; i < 100000; ++i) {
        if (!logger.log(LogLevel::Info,"[INFO] Sensor reading: 25.6")) {
            std::cerr << "Failed to write log\n";
            return 1;
        }
    }

    auto check = [&](const std::string& name,
                     telemetry::validStatus actual,
                     telemetry::validStatus expected) {
        if (actual == expected) {
            std::cout << "[PASS] " << name << '\n';
        } else {
            std::cout << "[FAIL] " << name << '\n';
            ++failed;
        }
    };

    check(
        "Valid reading",
        telemetry::validate({true, 25.0, ""}, 20.0, 80.0),
        telemetry::validStatus::Valid
    );

    check(
        "Out of range",
        telemetry::validate({true, 100.0, ""}, 20.0, 80.0),
        telemetry::validStatus::OutOfRange
    );

    check(
        "Read error",
        telemetry::validate(
            {false, 0.0, "simulated read failure"},
            20.0, 80.0
        ),
        telemetry::validStatus::ReadError
    );

    check(
        "Invalid range",
        telemetry::validate({true, 25.0, ""}, 80.0, 20.0),
        telemetry::validStatus::InvalidRange
    );
    check(
        "Valid edge",
        telemetry::validate({true, 20.0, ""}, 20.0, 80.0),
        telemetry::validStatus::Valid
    );
    check(
        "Valid edge",
        telemetry::validate({true, 80.0, ""}, 20.0, 80.0),
        telemetry::validStatus::Valid
    );

    while (running){

    }
    std::cout << "\nFailed: " << failed << '\n';
    return failed == 0 ? 0 : 1;
}