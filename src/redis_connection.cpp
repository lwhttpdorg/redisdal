#include <regex>

#include "redisdal/redis_connection.hpp"

namespace redisdal {

    query_info parse_query_params(const std::string &query) {
        query_info info;
        std::regex param_regex("([^=&]+)=([^&]*)");
        auto params_begin = std::sregex_iterator(query.begin(), query.end(), param_regex);
        auto params_end = std::sregex_iterator();

        for (std::sregex_iterator i = params_begin; i != params_end; ++i) {
            const std::smatch &match = *i;
            std::string key = match[1].str();
            std::string value = match[2].str();

            if (key == "db") {
                info.db = std::stoul(value);
            }
            else if (key == "password" || key == "auth") {
                info.password = value;
            }
            else if (key == "username") {
                info.username = value;
            }
        }
        return info;
    }

    redis_config parse_redis_url(const std::string &url) {
        redis_config config;
        std::string remaining_url = url;

        // Scheme
        if (url.rfind("tcp://", 0) == 0) {
            config.scheme = redis_scheme::TCP;
            remaining_url = url.substr(6);
        }
        else if (url.rfind("redis://", 0) == 0) {
            config.scheme = redis_scheme::REDIS;
            remaining_url = url.substr(8);
        }
        else if (url.rfind("unix://", 0) == 0) {
            config.scheme = redis_scheme::UNIX;
            remaining_url = url.substr(7);
            config.port = 0;
        }
        else {
            throw std::invalid_argument("Invalid Redis URL scheme.");
        }

        // Query parameters
        size_t query_pos = remaining_url.find('?');
        if (query_pos != std::string::npos) {
            std::string query_str = remaining_url.substr(query_pos + 1);
            query_info q_info = parse_query_params(query_str);
            config.db = q_info.db;
            if (!q_info.username.empty()) {
                config.username = q_info.username;
            }
            if (!q_info.password.empty()) {
                config.password = q_info.password;
            }
            remaining_url = remaining_url.substr(0, query_pos);
        }

        if (config.scheme == redis_scheme::UNIX) {
            if (remaining_url.empty() || remaining_url[0] != '/') {
                throw std::invalid_argument("Unix socket path must be absolute.");
            }
            config.host = remaining_url;
            return config;
        }

        // TCP/Redis specific parsing
        // User:Pass
        size_t at_pos = remaining_url.find('@');
        if (at_pos != std::string::npos) {
            std::string user_pass = remaining_url.substr(0, at_pos);
            remaining_url = remaining_url.substr(at_pos + 1);
            size_t colon_pos = user_pass.find(':');
            if (colon_pos != std::string::npos) {
                config.username = user_pass.substr(0, colon_pos);
                config.password = user_pass.substr(colon_pos + 1);
            }
            else {
                config.username = user_pass;
            }
        }

        if (remaining_url.empty()) {
            throw std::invalid_argument("Missing host in Redis URL.");
        }

        // Host and Port
        if (remaining_url[0] == '[') { // IPv6
            size_t bracket_end = remaining_url.find(']');
            if (bracket_end == std::string::npos) {
                throw std::invalid_argument("Invalid IPv6 address format in URL.");
            }
            config.host = remaining_url.substr(1, bracket_end - 1);
            if (remaining_url.length() > bracket_end + 1 && remaining_url[bracket_end + 1] == ':') {
                config.port = std::stoul(remaining_url.substr(bracket_end + 2));
            }
        }
        else { // IPv4 or hostname
            size_t colon_pos = remaining_url.find(':');
            if (colon_pos != std::string::npos) {
                config.host = remaining_url.substr(0, colon_pos);
                config.port = std::stoul(remaining_url.substr(colon_pos + 1));
            }
            else {
                config.host = remaining_url;
            }
        }

        return config;
    }
} // namespace redisdal
