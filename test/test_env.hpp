#pragma once

#include <string>

#define DEFAULT_REDIS_HOST "127.0.0.1"
#define DEFAULT_REDIS_PORT 6379

struct redis_connection_params {
	std::string host;
	unsigned short port;
};

redis_connection_params get_redis_connection_params();
