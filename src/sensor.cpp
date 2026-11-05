#include "telemetry/sensor.h"
#include "telemetry/logger.h"
#include <utility>

namespace telemetry {


Sensor::Sensor(std::string name)
    : name_(std::move(name))
{
    
}

const std::string& Sensor::name() const
{
    return name_;
}

TemperatureSensor::TemperatureSensor(FaultMode fault)
    : Sensor("temperature"),
      fault_(fault),
      generator_(std::random_device{}()),
      distribution_(20.0, 80.0)
{
}

SensorReading TemperatureSensor::read()
{
    if (fault_ == FaultMode::TemperatureError) {
        return {false, 0.0, "simulated read failure"};
    }

    if (fault_ == FaultMode::TemperatureHigh) {
        return {true, 100.0, ""};
    }

    //return {true,-INFINITY,""};
    return {true, distribution_(generator_), ""};
}

VoltageSensor::VoltageSensor(FaultMode fault)
    : Sensor("voltage"),
      fault_(fault),
      generator_(std::random_device{}()),
      distribution_(3.0, 3.6)
{
}

SensorReading VoltageSensor::read()
{
    if (fault_ == FaultMode::VoltageError) {
        return {false, 0.0, "simulated read failure"};
    }

    if (fault_ == FaultMode::VoltageHigh) {
        return {true, 4.2, ""};
    }

    return {true, distribution_(generator_), ""};
}

} // namespace telemetry
