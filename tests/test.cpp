#include "telemetry/validator.h"

#include <iostream>

int main()
{
    int failed = 0;

    auto check = [&](const std::string& name,
                     telemetry::validStatus actual,
                     telemetry::validStatus expected) {
        if (actual == expected) {
            std::cout << "[PASS] " << name << '\n';
        } else {
            std::cout << "[FAIL] " << name << '\n';
            ++failed;
        }
    };

    check(
        "Valid reading",
        telemetry::validate({true, 25.0, ""}, 20.0, 80.0),
        telemetry::validStatus::Valid
    );

    check(
        "Out of range",
        telemetry::validate({true, 100.0, ""}, 20.0, 80.0),
        telemetry::validStatus::OutOfRange
    );

    check(
        "Read error",
        telemetry::validate(
            {false, 0.0, "simulated read failure"},
            20.0, 80.0
        ),
        telemetry::validStatus::ReadError
    );

    check(
        "Invalid range",
        telemetry::validate({true, 25.0, ""}, 80.0, 20.0),
        telemetry::validStatus::InvalidRange
    );

    check(
        "Lower boundary",
        telemetry::validate({true, 20.0, ""}, 20.0, 80.0),
        telemetry::validStatus::Valid
    );

    check(
        "Upper boundary",
        telemetry::validate({true, 80.0, ""}, 20.0, 80.0),
        telemetry::validStatus::Valid
    );

    std::cout << "\nFailed: " << failed << '\n';

    return failed == 0 ? 0 : 1;
}