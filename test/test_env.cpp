#include <stdexcept>

#include "test_env.hpp"

redis_connection_params get_redis_connection_params() {
	redis_connection_params params;

	const char *env_host = std::getenv("REDIS_HOST");
	if (env_host) {
		params.host = std::string(env_host);
	}
	else {
		params.host = DEFAULT_REDIS_HOST;
	}

	const char *env_port = std::getenv("REDIS_PORT");
	if (env_port) {
		int port_int = std::stoi(env_port);
		if (port_int > 0 && port_int < 65536) {
			params.port = static_cast<unsigned short>(port_int);
		}
		else {
			throw std::runtime_error("Port out of range.");
		}
	}
	else {
		params.port = DEFAULT_REDIS_PORT;
	}
	return params;
}
