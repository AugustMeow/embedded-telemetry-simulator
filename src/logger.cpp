#include "telemetry/logger.h"
#include <ctime>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <cstdlib>
#include <chrono>
#include <csignal>
#include <thread>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <algorithm>
#include <atomic>

#   define COL(x)  "\033[;" #x "m"
#   define RED     COL(31)
#   define YELLOW  COL(33)
#   define GRAY    "\033[0m"

FILE *fp = NULL;
pthread_t m_thread;
std::string log_level = "";
const int log_size = 5*1024*1024;
static constexpr std::size_t MAX_QUEUE_SIZE = 10000;
std::size_t max_queue_size{0};
namespace fs = std::filesystem;
std::atomic<bool> _fail_next_write_for_test{false};
std::string Logger::now_string()
{
    std::time_t t = std::time(nullptr);
    std::tm tm_buf;
    localtime_r(&t, &tm_buf);        // Linux 下线程安全版本
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

Logger::Logger()
{

}

#if 0
Logger::Logger(const std::string &filename, LogLevel level, std::size_t max_size, int max_files) : _filename(filename), _level(level), _max_size(max_size), _max_files(max_files)
{
    #if 0
    if(_max_size == 0 || _max_files < 1){
        throw std::invalid_argument("Invalid log rotation settings");
    }
    
    std::error_code ec;
    if(fs::exists(_filename, ec) && !ec){
        _current_size = fs::file_size(_filename, ec);
        if(ec){
            throw fs::filesystem_error(
                "Failed to get log file size", _filename, ec);
        }
    }
    if (!openFile()) {
        throw std::runtime_error("Failed to open log file");
    }

     // 文件准备好后，再启动工作线程
    _worker = std::thread(&Logger::worker_loop, this);
    #endif
}
#endif

void Logger::fail_next_write_for_test()
{
    _fail_next_write_for_test.store(true);
}

Logger::~Logger()
{
    info("~Logger() already close File");
    close();
   // pthread_join(m_thread,NULL);
    //pthread_exit(*run);
}

const char* Logger::level_name(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    }

    return "UNKNOWN";
}

bool Logger::init(const LoggerConfig &config)
{
	if(inited){
		warn("have been inited!!!");
		return false;
	}
	_filename = config.file_path;
	_level = config.level;
	_max_size = config.max_size;
	_max_files = config.max_files;


	if(_max_size == 0 || _max_files < 1){
        return false;
		throw std::invalid_argument("Invalid log rotation settings");
	}

	std::error_code ec;
	if(fs::exists(_filename, ec) && !ec){
		_current_size = fs::file_size(_filename, ec);
		if(ec){
            return false;
			throw fs::filesystem_error(
					"Failed to get log file size", _filename, ec);
		}
	}
	if (!openFile()) {
        return false;
		throw std::runtime_error("Failed to open log file");
	}
	// 文件准备好后，再启动工作线程
	_worker = std::thread(&Logger::worker_loop, this);
	inited = true;
    return true;
}

bool Logger::log(
    LogLevel level,
    const std::string &message)
{
    std::string name(fs::path(__FILE__).filename());
    // 格式化时间戳和日志内容
    #if 0
    std::string line =
        "[" + std::string(level_name(level)) + "] " +
        now_string() + " " + message + "\n";
        #endif
        std::string line = now_string() + " "  + "(" + name + ":" + __FUNCTION__ + ":" 
        + std::to_string(__LINE__) + ")" + "[" 
        + std::string(level_name(level)) + "] "+ " "+ message + "\n";

    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (_stopping) {
            return false;
        }

        // 日志等级过滤
        if (static_cast<int>(level) <
            static_cast<int>(_level)) {
            return true;
        }
        if(_queue.size() >= MAX_QUEUE_SIZE){
            ++_dropped_count;
            return false;
        }
        _queue.push_back(std::move(line));
        ++_accepted_count;
        //max_queue_size =
        //std::max(max_queue_size, _queue.size());
    }
    

    // 通知工作线程有新日志
    _cv.notify_one();
    return true;
}
#if 0
bool Logger::log(LogLevel level, const std::string& message)
{
     // 过滤低于最低输出等级的日志
    if (static_cast<int>(level) <
        static_cast<int>(_level)) {
        return true;
    }

    if (!_file.is_open() || !_file.good()) {
        return false;
    }
     const std::string line =
        "[" + std::string(level_name(level)) + "] " +
        now_string() + " " + message + "\n";
#if 0
    std::string pre;
    // 将换行符也计入日志大小
    if(_level == LogLevel::Info){
        pre = "[INFO] " + now_string() + " ";
    }
     if(_level == LogLevel::Debug){
        pre = "[DEBUG] " + now_string() + " ";
    }
     if(_level == LogLevel::Warn){
        pre = "[WARN] " + now_string() + " ";
    }
     if(_level == LogLevel::Error){
        pre = "[ERROR] " + now_string() + " ";
    }
    const std::string line = pre + message + '\n';
    #endif

    // 如果当前文件非空，且写入后会超过限制，则切割
    if (_current_size > 0 &&
        line.size() > _max_size -
            (_current_size < _max_size ? _current_size : _max_size)) {
        if (!rotate()) {
            return false;
        }
    }

    _file.write(
        line.data(),
        static_cast<std::streamsize>(line.size()));
    std::cout << line.data() << std::endl;
    if (!_file) {
        return false;
    }

    _current_size += line.size();

     // 暂时保留控制台输出，方便调试
    std::cout << line;
    return true;
}
#endif

void Logger::set_level(LogLevel level)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _level = level;
}


void Logger::info(const std::string& message)
{
    log(LogLevel::Info, message);
}

void Logger::warn(const std::string& message)
{
    log(LogLevel::Warn, message);
}

void Logger::error(const std::string& message)
{
    log(LogLevel::Error, message);
}


void Logger::worker_loop()
{

    std::size_t write_failed_count = 0;
    std::size_t write_count = 0;
    while (true) {
        std::string line;

        {
            std::unique_lock<std::mutex> lock(_mutex);

            _cv.wait(lock, [this] {
                return _stopping || !_queue.empty();
            });

            // 停止且队列已排空，退出线程
            if (_stopping && _queue.empty()) {
                break;
            }

            line = std::move(_queue.front());
            //line = _queue.front();
            _queue.pop_front();
        }

        // 文件操作不持有队列锁
        if (!write_line(line)) {
            ++write_failed_count;
             std::cerr << "[LOGGER ERROR] "
                      << "Failed to write log file \n";
        }else{
            ++write_count;
        }
    }
    fprintf(stdout, RED);
    std::cout << __FUNCTION__ << ":"<< __LINE__ <<" already write:" << write_count << "  failed: " << write_failed_count << std::endl;
}

bool Logger::write_line(const std::string& line)
{
    if (!_file.is_open() || !_file.good()) {
        return false;
    }

    // 防止减法下溢
   if (_current_size > 0 &&
    (_current_size >= _max_size ||
     line.size() > _max_size - _current_size)) {

        if (!rotate()) {
            return false;
        }
    }
    #if 0
    if (_fail_next_write_for_test.exchange(false)) {
        return false;
    }
    #endif

    _file.write(
        line.data(),
        static_cast<std::streamsize>(line.size()));
    if(_level == LogLevel::Debug)
        fprintf(stdout, YELLOW);
    fprintf(stdout, "%s", line.c_str());
    
    fflush(stdout);
    if (!_file) {
        return false;
    }
    
    _current_size += line.size();
    return true;
}

#if 0
void Logger::close()
{
    if (_file.is_open()) {
        _file.close();
    }
}
#endif

LoggerStats Logger::get_stats()
{
    std::lock_guard<std::mutex> lock(_mutex);

    return {
        _accepted_count,
        _dropped_count
    };
}

void Logger::close()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _stopping = true;
    }

    _cv.notify_all();

    if (_worker.joinable()) {
        _worker.join();
    }

    if (_file.is_open()) {
        _file.close();
    }
}

bool Logger::openFile()
{
    _file.open(_filename, std::ios::out | std::ios::app);
    return _file.is_open() && _file.good();
}

bool Logger::rotate()
{
    _file.close();

    if (_file.fail()) {
        return false;
    }

    std::error_code ec;

    // 删除最旧的归档
    fs::remove(_filename + "." +
               std::to_string(_max_files), ec);

    if (ec) {
        openFile();
        return false;
    }

    // 归档文件依次向后移动
    for (int i = _max_files - 1; i >= 1; --i) {   //2->3,1->2,->1
        const std::string old_name =
            _filename + "." + std::to_string(i);
        const std::string new_name =
            _filename + "." + std::to_string(i + 1);

        ec.clear();

        if (fs::exists(old_name, ec) && !ec) {
            fs::rename(old_name, new_name, ec);

            if (ec) {
                openFile();
                return false;
            }
        } else if (ec) {
            openFile();
            return false;
        }
    }

    // 当前日志改名为 .1
    ec.clear();

    if (fs::exists(_filename, ec) && !ec) {
        fs::rename(_filename, _filename + ".1", ec);

        if (ec) {
            openFile();
            return false;
        }
    }

    // 创建新的当前日志文件
    _current_size = 0;

    if (!openFile()) {
        return false;
    }

    return true;
}
