
#include "telemetry/logger.h"

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>
int main()
{
    constexpr int thread_count = 8;
    constexpr int logs_per_thread = 1000;
    constexpr int expected = thread_count * logs_per_thread;
    // 按你当前 Logger 的构造函数签名调整参数。
    // 测试时关闭日志轮转，或把文件大小限制设得足够大，
    // 否则归档删除旧文件会影响最终统计。

    std::filesystem::remove("concurrency_test.log");
   
    LoggerConfig config;
    Logger logger;

    config.file_path = "concurrency_test.log";
    config.max_files = 1;
    config.max_size = 100 * 1024*1024;
    
    logger.init(config);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    std::atomic<int> attempted{0};
    std::atomic<int> accepted{0};
    std::atomic<int> dropped{0};
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([t, &logger, &attempted, &accepted, &dropped]() {
            for (int i = 0; i < logs_per_thread; ++i) {
                std::string message =
                "thread=" + std::to_string(t) +
                " sequence=" + std::to_string(i);

            ++attempted;

        if (logger.log(LogLevel::Info, message)) {
            ++accepted;
        } else {
            ++dropped;
        }
}
        });
    }

    // 等待所有生产者线程结束。
    for (auto& thread : threads) {
        thread.join();
    }

    // 等待后台线程写完队列中的日志。
    logger.close();
    
    auto stats = logger.get_stats();

    std::cout << "Accepted: " << stats.accepted << '\n';
    std::cout << "Dropped: " << stats.dropped << '\n';

    std::ifstream file("concurrency_test.log");
    if (!file.is_open()) {
        std::cerr << "Failed to open log file\n";
        return 1;
    }

    int actual = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++actual;
    }

    std::cout << "Attempted: " << attempted << '\n';
    std::cout << "Accepted:  " << accepted << '\n';
    std::cout << "Dropped:   " << dropped << '\n';
    std::cout << "Actual:    " << actual << '\n';

    if (attempted == accepted + dropped &&
        accepted == actual) {
        std::cout << "PASS\n";
    } else {
        std::cout << "FAIL\n";
    }

    return 1;
}
