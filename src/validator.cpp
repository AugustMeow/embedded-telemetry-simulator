#include "telemetry/validator.h"
#include "telemetry/logger.h"
namespace telemetry {
validStatus validate(const SensorReading &_read, double min_value, double max_value)
{

	if(min_value > max_value)
		return validStatus::InvalidRange;
	if(!_read.ok)
		return validStatus::ReadError;
	if (!std::isfinite(_read.value))
        return validStatus::OutOfRange;
	if(_read.value < min_value || _read.value > max_value)
		return validStatus::OutOfRange;

	return validStatus::Valid;
}


} // namespace telemetry
