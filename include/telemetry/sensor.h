#ifndef TELEMETRY_SENSOR_H
#define TELEMETRY_SENSOR_H

#include <random>
#include <string>

namespace telemetry {

enum class FaultMode {
    None,
    TemperatureHigh,
    VoltageHigh,
    TemperatureError,
    VoltageError
};

struct SensorReading {
    bool ok;
    double value;
    std::string error;
};

class Sensor {
public:
    explicit Sensor(std::string name);
    virtual ~Sensor() = default;

    virtual SensorReading read() = 0;

    const std::string& name() const;

private:
    std::string name_;
};

class TemperatureSensor : public Sensor {
public:
    explicit TemperatureSensor(
        FaultMode fault = FaultMode::None);

    SensorReading read() override;

private:
    FaultMode fault_;
    std::mt19937 generator_;
    std::uniform_real_distribution<double> distribution_;
};

class VoltageSensor : public Sensor {
public:
    explicit VoltageSensor(
        FaultMode fault = FaultMode::None);

    SensorReading read() override;

private:
    FaultMode fault_;
    std::mt19937 generator_;
    std::uniform_real_distribution<double> distribution_;
};

} // namespace telemetry

#endif
