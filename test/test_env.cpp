#include <stdexcept>

#include "test_env.hpp"

std::string get_redis_connection_url() {
	std::string url;

	const char *env_host = std::getenv("REDIS_HOST");
	if (env_host) {
		url = std::string(env_host);
	}
	else {
		// default to tcp URL with default port
		url = DEFAULT_REDIS_HOST;
	}
	return url;
}
