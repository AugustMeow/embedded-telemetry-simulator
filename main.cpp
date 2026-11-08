#include "telemetry/sensor.h"
#include "telemetry/validator.h"
#include "telemetry/logger.h"
#include "telemetry/application.h"
#include "telemetry/config.h"
#include "telemetry/tcpserver.h"
#include "telemetry/protocol.h"
#include <cstdlib>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

volatile std::sig_atomic_t running = 1;
void handle_signal(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        running = 0;
    }
}

telemetry::FaultMode parse_fault(int argc, char* argv[])
{
    using telemetry::FaultMode;

    if (argc == 1) {
        return FaultMode::None;
    }

    if (argc != 3 || std::string(argv[1]) != "--fault") {
        std::cerr
            << "Usage: telemetry_sim [--fault "
            << "temp-high|voltage-high|temp-error|voltage-error]\n";
        std::exit(1);
    }

    const std::string fault = argv[2];

    if (fault == "temp-high") {
        return FaultMode::TemperatureHigh;
    }

    if (fault == "voltage-high") {
        return FaultMode::VoltageHigh;
    }

    if (fault == "temp-error") {
        return FaultMode::TemperatureError;
    }

    if (fault == "voltage-error") {
        return FaultMode::VoltageError;
    }

    std::cerr << "Unknown fault mode: " << fault << '\n';
    std::exit(1);
}
#if 0
int main(int argc, char* argv[])
{
    
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

#if 0
// 注入一次性写入故障
logger.fail_next_write_for_test();

// 提交三条日志
logger.info("Test message 1");
logger.info("Test message 2");
logger.info("Test message 3");

// 等待 Worker 处理完所有消息
logger.close();

// 查看统计结果
std::cout << "Write success: "
          << logger.write_count() << '\n';

std::cout << "Write failed: "
          << logger.write_failed_count() << '\n';
#endif

    const auto fault = parse_fault(argc, argv);

    telemetry::TemperatureSensor temperature(fault);
    telemetry::VoltageSensor voltage(fault);
	
    telemetry::TelemetryApplication app(logger);

    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    constexpr auto interval = 1s;
    auto next_tick = Clock::now();

	logger.info("Embedded Telemetry Simulator started");
	std::cout << "Embedded Telemetry Simulator started\n";

	while (running) {
		const auto temperature_reading = temperature.read();
		const auto voltage_reading = voltage.read();
		app.process_reading(temperature, temperature_reading,
				20.0, 80.0, "C");
		app.process_reading(voltage, voltage_reading,
				3.0, 3.6, "V");
		next_tick += interval;
		std::this_thread::sleep_until(next_tick);
	}
	logger.info("Simulator stopped");
	logger.close();
	return 0;
}
#endif

int main(int argc, char* argv[])
{   
	std::signal(SIGINT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	telemetry::Config config;
	std::string error;
	// 1. 加载配置
	if (!telemetry::load_config("config.ini", config, error)) {
		std::cerr << "[ERROR] Config loading failed: "
			<< error << '\n';
		return EXIT_FAILURE;
	}

	// 2. 校验配置
	if (!telemetry::check_validate(config, error)) {
		std::cerr << "[ERROR] Config validation failed: "
			<< error << '\n';
		return EXIT_FAILURE;
	}

	#if 1
	// 3. 初始化 Logger
	Logger logger;

	try {
		logger.init(config.logger);
        //logger.init(config.logger);
	}
	catch (const std::exception& e) {
		std::cerr << "[ERROR] Logger initialization failed: "
			<< e.what() << '\n';
		return EXIT_FAILURE;
	}

	// 4. 创建 Application
	telemetry::TelemetryApplication app(logger);
	#endif
	//app.init();
	telemetry::TCPServer server(logger);
	server.init(10000);

#if 0
	//压缩测试
	ClientState state;

	state.send_buf = std::string(16384, 'A');
	state.send_offset = 8192;

	std::cout << "Before compact:"
          << " size=" << state.send_buf.size()
          << " offset=" << state.send_offset
          << std::endl;

	server.compact_send_buffer(state);

	std::cout << "After compact:"
          << " size=" << state.send_buf.size()
          << " offset=" << state.send_offset
          << std::endl;
#endif
	std::cout << "Temperature min: "
		<< config.sensor.temperature_min << '\n';

	std::cout << "Temperature max: "
		<< config.sensor.temperature_max << '\n';

	std::cout << "Sample interval: "
		<< config.sensor.sample_interval_ms << '\n';

	std::cout << "Log file: "
		<< config.logger.file_path << '\n';

	std::cout << "Max size: "
		<< config.logger.max_size << '\n';

	std::cout << "max files: "
		<< config.logger.max_files << '\n';

	const auto fault = parse_fault(argc, argv);

	telemetry::TemperatureSensor temperature(fault);
	telemetry::VoltageSensor voltage(fault);


	using Clock = std::chrono::steady_clock;
	using namespace std::chrono_literals;

	//const auto interval =
	//	std::chrono::milliseconds(config.sensor.sample_interval_ms);

    const auto interval = 1s;
	auto next_tick = Clock::now();
	logger.info("Embedded Telemetry Simulator started");
	std::cout << "Embedded Telemetry Simulator started\n";
#if 0
	//telemetry::MessageType type;
	uint8_t type = 1;
	uint8_t *buffer = (uint8_t*)malloc(8);
	uint32_t length = 4096;
	telemetry::Protocol proto;
	if(proto.encoder_header(buffer,type, length)){
		for(int i = 0; i < 8; i++){
			printf("%.2x ",buffer[i]);
		}
		std::cout << std::endl;
	}
	else{
		std::cout << " false" << std::endl;
	}
	//buffer = "544C010100000005";
	type = 0;
	length = 0;
	if(proto.decoder_header(buffer,type,length)){
		printf(" %d %d \n",type, length);
	}
	else{
		std::cout << " decoder false" << std::endl;
	}

	if(buffer) free(buffer);
#endif

	while (running) {
		const auto temperature_reading = temperature.read();
		const auto voltage_reading = voltage.read();

		#if 0
			app.process_reading(temperature, temperature_reading,
					config.sensor.temperature_min,config.sensor.temperature_max, "C");
			app.process_reading(voltage, voltage_reading,
					config.sensor.voltage_min,config.sensor.voltage_max, "V");
		#endif
					next_tick += interval;
		std::this_thread::sleep_until(next_tick);
	}
	logger.info("Simulator stopped");
	logger.close();

	return 0;
}
