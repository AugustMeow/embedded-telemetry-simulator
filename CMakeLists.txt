cmake_minimum_required(VERSION 3.16)

project(embedded_telemetry_simulator
    VERSION 0.1.0
    DESCRIPTION "A telemetry simulator for embedded Linux"
    LANGUAGES CXX
)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(telemetry_sim
    main.cpp
    src/sensor.cpp
	src/validator.cpp
	src/logger.cpp
    src/application.cpp
    src/config.cpp
    src/tcpserver.cpp
    src/protocol.cpp
    src/protocol_parser.cpp
)

# Independent tests
add_executable(telemetry_test
    tests/test.cpp
    src/sensor.cpp
    src/validator.cpp
	src/logger.cpp
    src/application.cpp
    src/config.cpp
)


add_executable(protocol_test
    tests/protocol_test.cpp
	src/protocol.cpp
    src/protocol_parser.cpp
)

target_link_libraries(telemetry_sim PRIVATE Threads::Threads)
target_link_libraries(telemetry_test PRIVATE Threads::Threads)

target_include_directories(telemetry_sim
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_include_directories(telemetry_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_include_directories(protocol_test
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(telemetry_sim
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(logger_concurrency_test
    tests/logger_concurrency_test.cpp
    src/logger.cpp
)

target_include_directories(logger_concurrency_test
    PRIVATE include
)

target_link_libraries(logger_concurrency_test
    PRIVATE Threads::Threads
)

add_executable(logger_shutdown_test
    tests/logger_shutdown_test.cpp
    src/logger.cpp
)

target_include_directories(logger_shutdown_test
    PRIVATE include
)

target_link_libraries(logger_shutdown_test
    PRIVATE Threads::Threads
)

# Compiler warnings
foreach(target telemetry_sim telemetry_test)
    target_compile_options(${target}
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endforeach()

enable_testing()

add_test(
    NAME validator_tests
    COMMAND telemetry_test
)

install(TARGETS telemetry_sim
    RUNTIME DESTINATION bin
)

