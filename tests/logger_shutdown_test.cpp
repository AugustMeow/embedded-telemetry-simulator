
#include "telemetry/logger.h"

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main()
{
    constexpr int thread_count = 8;
    constexpr int logs_per_thread = 1000;
    
    // 按你当前 logger.h 中的构造函数签名调整参数

    LoggerConfig config;
    Logger logger;

    config.file_path = "shutdown_test.log";
    config.max_files = 1;
    config.max_size = 100 * 1024*1024;
    
    logger.init(config);
    std::atomic<int> accepted{0};
    std::vector<std::thread> producers;

    // 1. 多个生产者线程并发提交日志。
    for (int t = 0; t < thread_count; ++t) {
        producers.emplace_back([t, &logger, &accepted]() {
            for (int i = 0; i < logs_per_thread; ++i) {
                std::string message =
                    "thread=" + std::to_string(t) +
                    " sequence=" + std::to_string(i);

                // 假设 log() 返回 bool：
                // true 表示成功进入队列。
                if (logger.log(LogLevel::Info, message)) 
                    ++accepted;
            }
        });
    }

    // 2. 等待所有生产者结束。
    for (auto& producer : producers) {
        producer.join();
    }

    const int expected = accepted.load();

    // 3. 立即关闭，不额外等待后台线程。
    // close() 应负责排空队列并 join 后台线程。
    logger.close();

    // 4. close() 返回后检查日志文件。
    std::ifstream file("shutdown_test.log");
    if (!file.is_open()) {
        std::cerr << "FAIL: cannot open log file\n";
        return 1;
    }

    int actual = 0;
    std::string line;

    while (std::getline(file, line)) {
        ++actual;
    }

    std::cout << "Accepted: " << expected << '\n';
    std::cout << "Written:  " << actual << '\n';

    if (actual == expected) {
        std::cout << "PASS: all accepted logs were written\n";
        return 0;
    }

    std::cerr << "FAIL: some logs may be missing\n";
    return 1;
}
