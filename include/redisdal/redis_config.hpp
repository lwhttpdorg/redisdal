#pragma once

#include <cstdint>
#include <string>

namespace redisdal {
    /**
     * @brief Defines the connection scheme for a Redis URL.
     */
    enum class redis_scheme : std::uint8_t { TCP, UNIX, REDIS };

    class redis_config {
    public:
        redis_config() : scheme(redis_scheme::TCP), host("127.0.0.1"), port(6379), index(0) {
        }

        redis_scheme scheme;
        std::string host;
        unsigned short port;
        std::string username;
        std::string password;
        unsigned int index;
    };

    /**
     * @brief Holds parsed query parameters from a Redis URL, like authentication and database index.
     */
    struct query_info {
        std::string username;
        std::string password;
        unsigned int index{0};
    };

    /**
     * @brief Parses the query string part of a URL (e.g., "?index=0&auth=secret").
     * @param query The query string to parse.
     * @return A query_info struct containing the parsed data.
     */
    query_info parse_query_params(const std::string &query);

    /**
     * @brief Parse Redis URL
     *
     * Accept a URL of the form:
     * - TCP:   `tcp://[user[:pass]@]host[:port]?index=N`
     * - Redis: `redis://[user[:pass]@]host[:port]?index=N`
     * - Unix:  `unix:///absolute/path/to/socket?index=N&auth=secret`
     * - Unix:  `unix:///absolute/path/to/socket?username=myuser&password=secret&index=0`
     *
     * Supports:
     * - Optional authentication (`user[:pass]@`, `?auth=secret`, or `?username=...&password=...`)
     * - IPv4, IPv6, and hostname formats
     * - Default port 6379 if not specified
     * - Default database index 0 if not specified
     *
     * @param url The Redis connection string.
     * @return redis_config The parsed configuration structure.
     * @throws std::invalid_argument If the scheme or format is invalid.
     */
    redis_config parse_redis_url(const std::string &url);

} // namespace redisdal
