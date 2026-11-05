#ifndef   CONFIG_H
#define   CONFIG_H
#include "telemetry/logger.h"
#include <string>
#include <cstddef>
#include <cstdint>

namespace telemetry{
// 传感器配置
struct SensorConfig {
    double temperature_min = 20.0;
    double temperature_max = 80.0;

    double voltage_min = 3.0;
    double voltage_max = 3.6;

    std::uint32_t sample_interval_ms = 1000;
};


// 全局配置
struct Config {
    SensorConfig sensor;
    LoggerConfig logger;
};

bool load_config(const std::string & file_path, Config &config, std::string &error);

bool check_validate(const Config &config, std::string &error);
#if 0
class Config{
public:
    Config(const std::string & filename);
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    ~Config();
    bool readConfig();
    std::string ReadInfoFromFile(std::string filePath);
    SensorConfig GetSensorConfig();
    LoggerConfig GetLoggerConfig();
private:
    std::string _filename;
    SensorConfig _senconfig;
    LoggerConfig _logconfig;
};
#endif
}// end of namespace
#endif