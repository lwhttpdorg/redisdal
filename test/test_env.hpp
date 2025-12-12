#pragma once

#include <string>

constexpr const char *DEFAULT_REDIS_HOST = "tcp://127.0.0.1:6379";

std::string get_redis_connection_url();
