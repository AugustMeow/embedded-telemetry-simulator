#ifndef VALIDATOR_H
#define VALIDATOR_H
#include "sensor.h"
namespace telemetry {
enum class validStatus{
	Valid,
	OutOfRange,
	ReadError,
	InvalidRange
};

validStatus validate(const SensorReading &read, double min_value, double max_value);

}
#endif
