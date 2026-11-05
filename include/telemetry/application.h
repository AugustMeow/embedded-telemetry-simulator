#ifndef   APPLICATION_H
#define   APPLICATION_H
#include "telemetry/sensor.h"
#include "telemetry/logger.h"
namespace telemetry {
class TelemetryApplication {
public:
    TelemetryApplication(Logger &logger);
    ~TelemetryApplication();

    int init();

    void process_reading(
        const Sensor& sensor,
        const SensorReading& reading,
        double min_value,
        double max_value,
        const std::string& unit
    );



private:
    void receive();

    Logger& _logger;
    std::thread tcpserver;
    int _listenfd;
    int _connfd;
};

} // namespace telemetry

#endif