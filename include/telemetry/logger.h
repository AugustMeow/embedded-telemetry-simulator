
#ifndef LOGGER_H
#define LOGGER_H
#include <condition_variable>
#include <cstddef>
#include <fstream>
#include <string>
#include <mutex>
#include <queue>
#include <thread>

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};


struct LoggerStats {
    std::size_t accepted;
    std::size_t dropped;
};

// 日志配置
struct LoggerConfig {
    std::string file_path = "telemetry.log";
    LogLevel level = LogLevel::Info;
    std::size_t max_size = 1024 * 1024;
    std::size_t max_files = 3;
    std::size_t queue_capacity = 1000;
};


class Logger {
public:
    Logger();
    #if 0
    Logger(const std::string& filename,
           LogLevel level,
           std::size_t max_size,
           int max_files);
    #endif

    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    bool init(const LoggerConfig& config);
    bool log(LogLevel level, const std::string& message);

    void set_level(LogLevel level);

    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

    void close();
    LoggerStats get_stats();
    void fail_next_write_for_test();
    
private:
    void worker_loop();
    bool write_line(const std::string& line);

    bool rotate();
    bool openFile();

    static std::string now_string();
    static const char* level_name(LogLevel level);

    std::string _filename;
    std::size_t _max_size;
    LogLevel _level;
    int _max_files;
    std::size_t _current_size{0};
    std::ofstream _file;

    // 以下成员用于异步日志
    std::deque<std::string> _queue;
    std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _worker;
    bool _stopping{false};
    std::size_t _accepted_count{0};
    std::size_t _dropped_count{0};

    bool inited{false};
};

#endif
