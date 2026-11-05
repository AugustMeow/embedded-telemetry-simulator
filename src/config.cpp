#include "telemetry/config.h"

#include <fstream>
#include <stdexcept>
#include <limits>
#include <cmath>
#include <iostream>
namespace telemetry {

static std::string trim(const std::string& str)
{
    const auto first = str.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const auto last = str.find_last_not_of(" \t\r\n");

    return str.substr(first, last - first + 1);
}

static bool parse_double(
    const std::string& value,
    double& result)
{
    try {
        std::size_t pos = 0;

        double temp = std::stod(value, &pos);
        if (pos != value.size()) {
            return false;
        }
        result = temp;
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

static bool parse_size(
    const std::string& value,
    std::size_t& result)
{
    if (value.empty() || value[0] == '-') {
        return false;
    }

    try {
        std::size_t pos = 0;

        unsigned long long temp =
            std::stoull(value, &pos);

        if (pos != value.size() ||
            temp > std::numeric_limits<std::size_t>::max()) {
            return false;
        }

        result = static_cast<std::size_t>(temp);
        return true;
    }
    catch (const std::exception&) {
        return false;
    }
}

bool load_config(
    const std::string& file_path,
    Config& config,
    std::string& error)
{
    std::ifstream file(file_path);

    if (!file.is_open()) {
        error = "Cannot open config file: " + file_path;
        return false;
    }

    Config temp_config;
    std::string section;
    std::string line;

    std::size_t line_number = 0;

    while (std::getline(file, line)) {
        ++line_number;

        line = trim(line);

        // 忽略空行和整行注释
        if (line.empty() ||
            line[0] == '#' ||
            line[0] == ';') {
            continue;
        }

        // 解析 section
        if (line.front() == '[') {
            if (line.back() != ']') {
                error = "Invalid section at line "
                      + std::to_string(line_number);
                return false;
            }

            section = trim(
                line.substr(1, line.size() - 2));

            if (section != "sensor" &&
                section != "logger") {
                error = "Unknown section at line "
                      + std::to_string(line_number);
                return false;
            }

            continue;
        }

        // 解析 key = value
        const auto equal_pos = line.find('=');

        if (equal_pos == std::string::npos) {
            error = "Invalid key-value pair at line "
                  + std::to_string(line_number);
            return false;
        }

        const std::string key =
            trim(line.substr(0, equal_pos));

        std::string value =
            trim(line.substr(equal_pos + 1));

        // 去除行尾注释
        const auto comment_pos = value.find_first_of("#;");

        if (comment_pos != std::string::npos) {
            value = trim(value.substr(0, comment_pos));
        }

        if (key.empty()) {
            error = "Empty key at line "
                  + std::to_string(line_number);
            return false;
        }

        if (section.empty()) {
            error = "Key outside section at line "
                  + std::to_string(line_number);
            return false;
        }

        // sensor 配置
        if (section == "sensor") {

            if (key == "temperature_min") {
                if (!parse_double(value,
                        temp_config.sensor.temperature_min)) {
                    error = "Invalid temperature_min" + std::to_string(line_number);;
                    return false;
                }
            }
            else if (key == "temperature_max") {
                if (!parse_double(value,
                        temp_config.sensor.temperature_max)) {
                    error = "Invalid temperature_max" + std::to_string(line_number);;
                    return false;
                }
            }
            else if (key == "voltage_min") {
                if (!parse_double(value,
                        temp_config.sensor.voltage_min)) {
                    error = "Invalid voltage_min" + std::to_string(line_number);;
                    return false;
                }
            }
            else if (key == "voltage_max") {
                if (!parse_double(value,
                        temp_config.sensor.voltage_max)) {
                    error = "Invalid voltage_max" + std::to_string(line_number);;
                    return false;
                }
            }
            else if (key == "sample_interval_ms") {
                std::size_t temp = 0;

                if (!parse_size(value, temp) ||
                    temp > std::numeric_limits<
                        std::uint32_t>::max()) {
                    error = "Invalid sample_interval_ms" + std::to_string(line_number);;
                    return false;
                }

                temp_config.sensor.sample_interval_ms =
                    static_cast<std::uint32_t>(temp);
            }
            else {
                error = "Unknown sensor key: " + key + std::to_string(line_number);;
                return false;
            }
        }

        // logger 配置
        else if (section == "logger") {

            if (key == "file_path") {
                temp_config.logger.file_path = value;
            }
            else if (key == "max_size") {
                if (!parse_size(value,
                        temp_config.logger.max_size)) {
                    error = "Invalid max_size" + std::to_string(line_number);;
                    return false;
                }
            }
            else if (key == "max_files") {
                if (!parse_size(value,
                        temp_config.logger.max_files)) {
                    error = "Invalid max_files"+ std::to_string(line_number);;
                    return false;
                }
            }
            else if (key == "queue_capacity") {
                if (!parse_size(value,
                        temp_config.logger.queue_capacity)) {
                    error = "Invalid queue_capacity"+ std::to_string(line_number);;
                    return false;
                }
            }
            else {
                error = "Unknown logger key: " + key+ std::to_string(line_number);;
                return false;
            }
        }
    }

    if (file.bad()) {
        error = "Error reading config file";
        return false;
    }

    config = temp_config;
    error.clear();

    return true;
}

bool check_validate(const Config &config, std::string& error)
{
    // Temperature range
    if (!std::isfinite(config.sensor.temperature_min) ||
        !std::isfinite(config.sensor.temperature_max) ||
        config.sensor.temperature_min >
            config.sensor.temperature_max) {

        error = "temperature InvalidRange";
        return false;
    }

    // Voltage range
    if (!std::isfinite(config.sensor.voltage_min) ||
        !std::isfinite(config.sensor.voltage_max) ||
        config.sensor.voltage_min >
            config.sensor.voltage_max) {

        error = "voltage InvalidRange";
        return false;
    }

    // Sampling interval
    if (config.sensor.sample_interval_ms == 0) {
        error = "sample_interval should not be zero";
        return false;
    }

    // Logger path
    if (config.logger.file_path.empty()) {
        error = "empty log file path";
        return false;
    }

    // Logger size
    if (config.logger.max_size == 0) {
        error = "max_size should not be zero";
        return false;
    }

    // Logger rotation count
    if (config.logger.max_files == 0) {
        error = "max_files should not be zero";
        return false;
    }

    // Logger queue capacity
    if (config.logger.queue_capacity == 0) {
        error = "queue_capacity should not be zero";
        return false;
    }

    error.clear();
    return true;
}

} // namespace telemetry

#if 0
#include "telemetry/config.h"
#include <fstream>
#include <iostream>

Config::Config(const std::string & filename):_filename(filename)
{
    bool ret = readConfig();
    if(!ret)
        std::cerr << "read file failed!" << std::endl;
}

Config::~Config()
{

}

bool Config::readConfig()
{   
    std::ifstream file(_filename);
    if(!file.is_open()){
        std::cerr << "Open file failed!" << std::endl;
    }
    std::string line;
    std::string sensorConfig("[sensor]");
    std::string loggerConfig("[logger]");
    while(getline(file,line)){
        if(sensorConfig.compare(line)){
            std::cout << line << std::endl;
            continue;
        }
        if(loggerConfig.compare(line)){
            std::cout << line << std::endl;
            continue;
        }
    }
    if(file.is_open()){
        file.close();
        return true;
    }
    return false;
}

std::string Config::ReadInfoFromFile(std::string filePath)
{
    size_t pos;
    std::ifstream file(filePath.c_str(), std::ios::ate);
    int infoLen = file.tellg();
    file.seekg(0);
    return std::string();
}
#endif