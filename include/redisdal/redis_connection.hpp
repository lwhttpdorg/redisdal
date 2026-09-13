#pragma once

#include <cstddef>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

#include <hiredis/hiredis.h>

#include "exception.hpp"
#include "kv_connection.hpp"
#include "serialization.hpp"

namespace redisdal {

    static constexpr size_t NUM_KEYS_BUF_SIZE = 32;
    // Maximum number of keys allowed to be passed to EVAL/EVALSHA to avoid
    // excessively large argv arrays and potential misuse. Adjust as needed.
    static constexpr size_t MAX_SCRIPT_KEYS = 64;

    /**
     * @brief Defines the connection scheme for a Redis URL.
     */
    enum class redis_scheme : std::uint8_t { TCP, UNIX, REDIS };

    class redis_config {
    public:
        redis_config() : scheme(redis_scheme::TCP), host("127.0.0.1"), port(6379), db(0) {
        }

        redis_scheme scheme;
        std::string host;
        unsigned short port;
        std::string username;
        std::string password;
        unsigned int db;
    };

    /**
     * @brief Holds parsed query parameters from a Redis URL, like authentication and database index.
     */
    struct query_info {
        std::string username;
        std::string password;
        unsigned int db{0};
    };

    /**
     * @brief Parses the query string part of a URL (e.g., "?db=0&auth=secret").
     * @param query The query string to parse.
     * @return A query_info struct containing the parsed data.
     */
    query_info parse_query_params(const std::string &query);

    /**
     * @brief Parse Redis URL
     *
     * Accept a URL of the form:
     * - TCP:   `tcp://[user[:pass]@]host[:port]?db=N`
     * - Redis: `redis://[user[:pass]@]host[:port]?db=N`
     * - Unix:  `unix:///absolute/path/to/socket?db=N&auth=secret`
     * - Unix:  `unix:///absolute/path/to/socket?username=myuser&password=secret&db=0`
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

    struct redis_context_deleter {
        void operator()(redisContext *context) const noexcept {
            if (nullptr != context) {
                redisFree(context);
            }
        }
    };

    using redis_context_ptr = std::unique_ptr<redisContext, redis_context_deleter>;

    struct reply_deleter {
        void operator()(redisReply *reply) const noexcept {
            if (nullptr != reply) {
                freeReplyObject(reply);
            }
        }
    };

    using redis_reply_ptr = std::unique_ptr<redisReply, reply_deleter>;

    /**
     * @brief Concrete kv_connection backed by hiredis.
     *
     * @par Design pattern
     * Adapter: translates the string-based kv_connection contract to hiredis commands
     * and converts hiredis reply objects into the library's return types.
     */
    class redis_connection: public kv_connection {
    public:
        /**
         * @brief Constructs a redis_connection and connects to the server using the provided URL.
         *
         * @param url The Redis connection URL.
         * @throws std::runtime_error if the connection fails, authentication fails,
         * or the URL is invalid.
         */
        explicit redis_connection(const std::string &url) {
            const redis_config config = parse_redis_url(url);

            switch (config.scheme) {
                case redis_scheme::TCP:
                case redis_scheme::REDIS:
                    context = redis_context_ptr(redisConnect(config.host.c_str(), config.port));
                    break;
                case redis_scheme::UNIX:
                    context = redis_context_ptr(redisConnectUnix(config.host.c_str()));
                    break;
            }
            if (!context || context->err) {
                throw std::runtime_error(std::string("Redis connect failed: ")
                                         + (context ? context->errstr : "could not allocate context"));
            }

            // If authentication info provided, attempt AUTH
            if (!config.password.empty()) {
                redis_reply_ptr auth_r = nullptr;
                if (!config.username.empty()) {
                    auth_r = redis_reply_ptr{static_cast<redisReply *>(
                        redisCommand(context.get(), "AUTH %s %s", config.username.c_str(), config.password.c_str()))};
                }
                else {
                    auth_r = redis_reply_ptr{
                        static_cast<redisReply *>(redisCommand(context.get(), "AUTH %s", config.password.c_str()))};
                }
                if (!auth_r) {
                    throw std::runtime_error("AUTH command failed");
                }
                if (auth_r->type == REDIS_REPLY_ERROR) {
                    const std::string err(auth_r->str, auth_r->len);
                    throw std::runtime_error("AUTH failed: " + err);
                }
            }
            if (config.db != 0 && !expect_ok(exec_args({"SELECT", std::to_string(config.db)}), "SELECT")) {
                throw std::runtime_error("SELECT failed: expected OK");
            }
        }

        bool exists(const std::string &key) override {
            const auto reply = exec("EXISTS %s", key.c_str());
            return reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
        }

        /**
         * @brief Retrieves all keys matching the given pattern.
         * @attention `KEYS` can be a performance risk on production servers. Consider using `SCAN` for iterative key
         * retrieval.
         * @attention `KEYS` can be a performance risk on production servers. Consider using `SCAN` for iterative key
         * retrieval.
         * @param pattern The pattern to match keys against (e.g., "*", "user:*").
         * @param keys An unordered_set to store the matching keys.
         */
        void keys(const std::string &pattern, std::unordered_set<std::string> &keys) override {
            const auto reply = exec("KEYS %s", pattern.c_str());
            if (reply->type == REDIS_REPLY_ARRAY) {
                for (size_t i = 0; i < reply->elements; ++i) {
                    if (reply->element[i]->type == REDIS_REPLY_STRING) {
                        keys.emplace(reply->element[i]->str, reply->element[i]->len);
                    }
                }
            }
        }

        /**
         * @brief Incrementally iterates the keys in the database.
         * @param cursor The scan cursor.
         * @param pattern The pattern to match keys against.
         * @param count The number of elements to return. This is a hint to the server, not an upper limit.
         * @return A scan_result containing the new cursor and the set of matching keys. (The cursor > 0 indicates more
         * keys to scan.)
         * @attention
         *
         * - the count is just a hint to the server, not an upper limit.
         * - the returned cursor > 0 indicates more keys to scan, not that is an index of offset.
         */
        string_scan_result scan(uint64_t cursor, const std::string &pattern, unsigned int count) override {
            string_scan_result result{0, {}};
            const std::string cursor_str = std::to_string(cursor);
            const auto reply = exec("SCAN %s MATCH %s COUNT %u", cursor_str.c_str(), pattern.c_str(), count);
            if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
                // First element is the new cursor
                if (reply->element[0]->type == REDIS_REPLY_STRING) {
                    try {
                        result.cursor = std::stoull(std::string(reply->element[0]->str, reply->element[0]->len));
                    }
                    catch (const std::exception &e) {
                        throw std::runtime_error("SCAN: failed to convert cursor to uint64_t: "
                                                 + std::string(e.what()));
                    }
                }
                else {
                    throw std::runtime_error("SCAN: unexpected cursor reply type or missing cursor.");
                }
                // Second element is the array of keys
                if (reply->element[1]->type == REDIS_REPLY_ARRAY) {
                    for (size_t i = 0; i < reply->element[1]->elements; ++i) {
                        if (reply->element[1]->element[i]->type == REDIS_REPLY_STRING) {
                            std::string key(reply->element[1]->element[i]->str, reply->element[1]->element[i]->len);
                            result.keys.insert(std::move(key));
                        }
                    }
                }
            }
            return result;
        }

        std::string type(const std::string &key) override {
            const auto reply = exec("TYPE %s", key.c_str());
            if (reply->type == REDIS_REPLY_STATUS) {
                return {reply->str, reply->len};
            }
            throw unexpected_reply_type_error("TYPE", "status", reply_type_name(reply->type));
        }

        bool expire(const std::string &key, int seconds) override {
            const auto reply = exec("EXPIRE %s %d", key.c_str(), seconds);
            return reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
        }

        bool pexpire(const std::string &key, int milliseconds) override {
            const auto reply = exec("PEXPIRE %s %d", key.c_str(), milliseconds);
            return reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
        }

        int64_t ttl(const std::string &key) override {
            const auto reply = exec("TTL %s", key.c_str());

            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("TTL", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        int64_t pttl(const std::string &key) override {
            const auto reply = exec("PTTL %s", key.c_str());

            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("PTTL", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        bool persist(const std::string &key) override {
            const auto reply = exec("PERSIST %s", key.c_str());
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("PERSIST", "integer", reply_type_name(reply->type));
            }
            return reply->integer == 1;
        }

        std::string ping() override {
            const auto reply = exec("PING");
            if (reply->type == REDIS_REPLY_STATUS || reply->type == REDIS_REPLY_STRING) {
                return {reply->str, reply->len};
            }
            throw unexpected_reply_type_error("PING", "status or string", reply_type_name(reply->type));
        }

        std::string ping(const std::string &message) override {
            const auto reply = exec("PING %s", message.c_str());
            if (reply->type == REDIS_REPLY_STATUS || reply->type == REDIS_REPLY_STRING) {
                return {reply->str, reply->len};
            }
            throw unexpected_reply_type_error("PING", "status or string", reply_type_name(reply->type));
        }

        long long del(const std::string &key) override {
            const auto reply = exec("DEL %s", key.c_str());
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("DEL", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long del(const std::vector<std::string> &keys) override {
            if (keys.empty()) {
                return 0;
            }
            std::vector<const char *> argv;
            std::vector<size_t> argv_len;
            argv.push_back("DEL");
            argv_len.push_back(3);
            for (auto &k: keys) {
                argv.push_back(k.c_str());
                argv_len.push_back(k.size());
            }
            const auto reply = execv(argv, argv_len);
            return reply->type == REDIS_REPLY_INTEGER ? reply->integer : 0;
        }

        // ============================================================================
        // For String
        // ============================================================================

        bool set(const std::string &key, const std::string &value) override {
            const auto reply = exec("SET %s %s", key.c_str(), value.c_str());
            return reply->type == REDIS_REPLY_STATUS && std::string(reply->str, reply->len) == "OK";
        }

        /* SET if Not eXists, Only set the key if it does not already exist */
        bool set_not_exists(const std::string &key, const std::string &value) override {
            const auto reply = exec("SET %s %s NX", key.c_str(), value.c_str());

            if (reply->type == REDIS_REPLY_STATUS) {
                return std::string(reply->str, reply->len) == "OK";
            }
            if (reply->type == REDIS_REPLY_NIL) {
                return false;
            }
            return false;
        }

        /* Set the specified expire time, in seconds */
        bool set_ex(const std::string &key, const std::string &value, int seconds) override {
            const auto reply = exec("SET %s %s EX %d", key.c_str(), value.c_str(), seconds);
            return reply->type == REDIS_REPLY_STATUS && std::string(reply->str, reply->len) == "OK";
        }

        /* Set the specified expire time, in milliseconds */
        bool set_px(const std::string &key, const std::string &value, int milliseconds) override {
            const auto reply = exec("SET %s %s PX %d", key.c_str(), value.c_str(), milliseconds);
            return reply->type == REDIS_REPLY_STATUS && std::string(reply->str, reply->len) == "OK";
        }

        std::optional<std::string> get(const std::string &key) override {
            const auto reply = exec("GET %s", key.c_str());
            if (reply->type == REDIS_REPLY_STRING) {
                return std::string(reply->str, reply->len);
            }
            if (reply->type == REDIS_REPLY_NIL) {
                return std::nullopt;
            }
            throw unexpected_reply_type_error("GET", "string or nil", reply_type_name(reply->type));
        }

        std::optional<std::string> getset(const std::string &key, const std::string &new_value) override {
            const auto reply = exec("GETSET %s %s", key.c_str(), new_value.c_str());
            if (reply->type == REDIS_REPLY_STRING) {
                return std::string(reply->str, reply->len);
            }
            if (reply->type == REDIS_REPLY_NIL) {
                return std::nullopt;
            }
            throw unexpected_reply_type_error("GETSET", "string or nil", reply_type_name(reply->type));
        }

        long long incr(const std::string &key, long long delta) override {
            const auto reply = exec("INCRBY %s %lld", key.c_str(), delta);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("INCRBY", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long decr(const std::string &key, long long delta) override {
            const auto reply = exec("DECRBY %s %lld", key.c_str(), delta);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("DECRBY", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long append(const std::string &key, const std::string &value) override {
            const auto reply = exec("APPEND %s %s", key.c_str(), value.c_str());
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("APPEND", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        // ============================================================================
        // For Hash
        // ============================================================================

        std::optional<std::string> hget(const std::string &key, const std::string &hash_key) override {
            const auto reply = exec("HGET %s %s", key.c_str(), hash_key.c_str());
            if (reply->type == REDIS_REPLY_NIL) {
                return std::nullopt;
            }
            if (reply->type == REDIS_REPLY_STRING) {
                return std::string(reply->str, reply->len);
            }
            throw unexpected_reply_type_error("HGET", "string or nil", reply_type_name(reply->type));
        }

        void hget(const std::string &key,
                  std::unordered_map<std::string, std::optional<std::string>> &hash_map) override {
            if (hash_map.empty()) {
                return;
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("HMGET");
            argv_len.push_back(5);
            argv.push_back(key.c_str());
            argv_len.push_back(key.size());

            std::vector<std::string> fields;
            fields.reserve(hash_map.size());
            for (auto &kv: hash_map) {
                argv.push_back(kv.first.c_str());
                argv_len.push_back(kv.first.size());
                fields.push_back(kv.first);
            }

            const auto reply = execv(argv, argv_len);
            if (reply->type != REDIS_REPLY_ARRAY) {
                return;
            }

            for (size_t i = 0; i < reply->elements && i < fields.size(); ++i) {
                const redisReply *elem = reply->element[i];
                if (elem->type == REDIS_REPLY_STRING) {
                    hash_map[fields[i]] = std::string(elem->str, elem->len);
                }
                else if (elem->type == REDIS_REPLY_NIL) {
                    hash_map[fields[i]] = std::nullopt;
                }
                else {
                    throw std::runtime_error("HMGET: unexpected element type");
                }
            }
        }

        bool hset(const std::string &key, const std::string &field, const std::string &value) override {
            const auto reply = exec("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
            return reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0;
        }

        bool hset(const std::string &key, const std::unordered_map<std::string, std::string> &hash_map) override {
            if (hash_map.empty()) {
                return false;
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("HSET");
            argv_len.push_back(4);
            argv.push_back(key.c_str());
            argv_len.push_back(key.size());

            for (auto &kv: hash_map) {
                argv.push_back(kv.first.c_str());
                argv_len.push_back(kv.first.size());
                argv.push_back(kv.second.c_str());
                argv_len.push_back(kv.second.size());
            }
            const redis_reply_ptr reply = execv(argv, argv_len);
            return reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0;
        }

        /**
         * @brief Gets all the fields and values in a hash.
         * @attention `HGETALL` can be a performance risk on production servers for large hashes. Consider using
         * `HSCAN`.
         * @attention `HGETALL` can be a performance risk on production servers for large hashes. Consider using
         * `HSCAN`.
         * @param key The hash key.
         * @return A map containing all fields and their values.
         */
        std::unordered_map<std::string, std::string> hgetall(const std::string &key) override {
            std::unordered_map<std::string, std::string> result;
            const auto reply = exec("HGETALL %s", key.c_str());
            if (reply->type == REDIS_REPLY_ARRAY) {
                for (size_t i = 0; i + 1 < reply->elements; i += 2) {
                    result.emplace(std::string(reply->element[i]->str, reply->element[i]->len),
                                   std::string(reply->element[i + 1]->str, reply->element[i + 1]->len));
                }
            }
            return result;
        }

        /**
         * @brief Gets all the fields in a hash.
         * @attention `HKEYS` can be a performance risk on production servers for large hashes. Consider using `HSCAN`.
         * @param key The hash key.
         * @return A vector of field names.
         */
        std::vector<std::string> hkeys(const std::string &key) override {
            std::vector<std::string> result;
            const auto reply = exec("HKEYS %s", key.c_str());
            if (reply->type == REDIS_REPLY_ARRAY) {
                for (size_t i = 0; i < reply->elements; ++i) {
                    result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                }
            }
            return result;
        }

        /**
         * @brief Gets all the values in a hash.
         * @attention `HVALS` can be a performance risk on production servers for large hashes. Consider using `HSCAN`.
         * @param key The hash key.
         * @return A vector of values.
         */
        std::vector<std::string> hvals(const std::string &key) override {
            std::vector<std::string> result;
            const auto reply = exec("HVALS %s", key.c_str());
            if (reply->type == REDIS_REPLY_ARRAY) {
                for (size_t i = 0; i < reply->elements; ++i) {
                    result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                }
            }
            return result;
        }

        /* @brief Incrementally iterates the fields and values of a hash stored at key.
         * @param key The hash key.
         * @param cursor The scan cursor.
         * @param pattern The pattern to match fields against.
         * @param count The number of elements to return. This is a hint to the server, not a limit.
         * @param hash_map An unordered_map to store the matching field-value pairs.
         * @return The new cursor. (The cursor > 0 indicates more fields to scan.)
         * @attention
         *
         * - the count is just a hint to the server, not an upper limit.
         * - the returned cursor > 0 indicates more keys to scan, not that is an index of offset.
         */
        uint64_t hscan(const std::string &key, uint64_t cursor, const std::string &pattern, unsigned int count,
                       std::unordered_map<std::string, std::string> &hash_map) override {
            uint64_t next_cursor = 0;
            const std::string cursor_str = std::to_string(cursor);
            const auto reply =
                exec("HSCAN %s %s MATCH %s COUNT %u", key.c_str(), cursor_str.c_str(), pattern.c_str(), count);
            if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
                // First element is the new cursor
                if (reply->element[0]->type == REDIS_REPLY_STRING) {
                    try {
                        std::string cursor_reply(reply->element[0]->str, reply->element[0]->len);
                        next_cursor = std::stoull(cursor_reply);
                    }
                    catch (const std::exception &e) {
                        throw std::runtime_error("HSCAN: failed to convert cursor to uint64_t: "
                                                 + std::string(e.what()));
                    }
                }
                else {
                    throw std::runtime_error("HSCAN: unexpected cursor reply type or missing cursor.");
                }
                // Second element is the array of field-value pairs
                if (reply->element[1]->type == REDIS_REPLY_ARRAY) {
                    for (size_t i = 0; i + 1 < reply->element[1]->elements; i += 2) {
                        if (reply->element[1]->element[i]->type == REDIS_REPLY_STRING
                            && reply->element[1]->element[i + 1]->type == REDIS_REPLY_STRING) {
                            std::string field(reply->element[1]->element[i]->str, reply->element[1]->element[i]->len);
                            std::string value(reply->element[1]->element[i + 1]->str,
                                              reply->element[1]->element[i + 1]->len);
                            hash_map.emplace(std::move(field), std::move(value));
                        }
                    }
                }
            }
            return next_cursor;
        }

        long long hdel(const std::string &key, const std::string &hash_key) override {
            const auto reply = exec("HDEL %s %s", key.c_str(), hash_key.c_str());
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("HDEL", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long hdel(const std::string &key, const std::vector<std::string> &hash_keys) override {
            if (hash_keys.empty()) {
                return 0;
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("HDEL");
            argv_len.push_back(4);
            argv.push_back(key.c_str());
            argv_len.push_back(key.size());

            for (auto &f: hash_keys) {
                argv.push_back(f.c_str());
                argv_len.push_back(f.size());
            }

            const auto reply = execv(argv, argv_len);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("HDEL", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        // ============================================================================
        // For list
        // ============================================================================

        long long lpush(const std::string &key, const std::vector<std::string> &values) override {
            if (values.empty()) {
                return llen(key);
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("LPUSH");
            argv_len.push_back(5);
            argv.push_back(key.c_str());
            argv_len.push_back(key.size());

            for (const auto &v: values) {
                argv.push_back(v.c_str());
                argv_len.push_back(v.size());
            }

            const auto reply = execv(argv, argv_len);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("LPUSH", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long lpush(const std::string &key, const std::string &value) override {
            const auto reply = exec("LPUSH %s %s", key.c_str(), value.c_str());
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("LPUSH", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long rpush(const std::string &key, const std::string &value) override {
            const auto reply = exec("RPUSH %s %s", key.c_str(), value.c_str());
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("RPUSH", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long rpush(const std::string &key, const std::vector<std::string> &values) override {
            if (values.empty()) {
                return llen(key);
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("RPUSH");
            argv_len.push_back(5);
            argv.push_back(key.c_str());
            argv_len.push_back(key.size());

            for (const auto &v: values) {
                argv.push_back(v.c_str());
                argv_len.push_back(v.size());
            }

            const auto reply = execv(argv, argv_len);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("RPUSH", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        std::optional<std::string> lpop(const std::string &key) override {
            const auto reply = exec("LPOP %s", key.c_str());
            if (reply->type == REDIS_REPLY_NIL) {
                return std::nullopt;
            }
            if (reply->type == REDIS_REPLY_STRING) {
                return std::string(reply->str, reply->len);
            }
            throw unexpected_reply_type_error("LPOP", "string or nil", reply_type_name(reply->type));
        }

        std::optional<std::string> rpop(const std::string &key) override {
            const auto reply = exec("RPOP %s", key.c_str());
            if (reply->type == REDIS_REPLY_NIL) {
                return std::nullopt;
            }
            if (reply->type == REDIS_REPLY_STRING) {
                return std::string(reply->str, reply->len);
            }
            throw unexpected_reply_type_error("RPOP", "string or nil", reply_type_name(reply->type));
        }

        std::vector<std::string> lpop(const std::string &key, int count) override {
            const auto reply = exec("LPOP %s %d", key.c_str(), count);
            if (reply->type == REDIS_REPLY_NIL) {
                return {};
            }
            if (reply->type == REDIS_REPLY_ARRAY) {
                std::vector<std::string> result;
                result.reserve(reply->elements);
                for (size_t i = 0; i < reply->elements; ++i) {
                    result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                }
                return result;
            }
            throw unexpected_reply_type_error("LPOP", "array or nil", reply_type_name(reply->type));
        }

        std::vector<std::string> rpop(const std::string &key, int count) override {
            const auto reply = exec("RPOP %s %d", key.c_str(), count);
            if (reply->type == REDIS_REPLY_NIL) {
                return {};
            }
            if (reply->type == REDIS_REPLY_ARRAY) {
                std::vector<std::string> result;
                result.reserve(reply->elements);
                for (size_t i = 0; i < reply->elements; ++i) {
                    result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                }
                return result;
            }
            throw unexpected_reply_type_error("RPOP", "array or nil", reply_type_name(reply->type));
        }

        /**
         * @brief Gets a range of elements from a list.
         * @attention Requesting a large range with `LRANGE` can consume significant memory and bandwidth. Be cautious
         * with large `start` and `stop` values.
         * @attention Requesting a large range with `LRANGE` can consume significant memory and bandwidth. Be cautious
         * with large `start` and `stop` values.
         * @param key The list key.
         * @param start The starting index (0 is the head).
         * @param stop The stopping index (-1 is the last element).
         * @return A vector of elements in the specified range.
         */
        std::vector<std::string> lrange(const std::string &key, long long start, long long stop) override {
            std::vector<std::string> result;
            const auto reply = exec("LRANGE %s %lld %lld", key.c_str(), start, stop);
            if (reply->type == REDIS_REPLY_ARRAY) {
                result.reserve(reply->elements);
                for (size_t i = 0; i < reply->elements; ++i) {
                    if (reply->element[i]->type == REDIS_REPLY_STRING) {
                        result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                    }
                }
            }
            else if (reply->type != REDIS_REPLY_NIL) {
                throw unexpected_reply_type_error("LRANGE", "array or nil", reply_type_name(reply->type));
            }
            return result;
        }

        long long llen(const std::string &key) override {
            const auto reply = exec("LLEN %s", key.c_str());
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("LLEN", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        std::optional<std::string> lindex(const std::string &key, long long index) override {
            const auto reply = exec("LINDEX %s %lld", key.c_str(), index);
            if (reply->type == REDIS_REPLY_NIL) {
                return std::nullopt;
            }
            if (reply->type == REDIS_REPLY_STRING) {
                return std::string(reply->str, reply->len);
            }
            throw unexpected_reply_type_error("LINDEX", "string or nil", reply_type_name(reply->type));
        }

        // ============================================================================
        // For Set
        // ============================================================================

        long long sadd(const std::string &key, const std::vector<std::string> &members) override {
            if (members.empty()) {
                return 0;
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("SADD");
            argv_len.push_back(4);
            argv.push_back(key.c_str());
            argv_len.push_back(key.size());

            for (const auto &m: members) {
                argv.push_back(m.c_str());
                argv_len.push_back(m.size());
            }

            const auto reply = execv(argv, argv_len);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("SADD", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long srem(const std::string &key, const std::vector<std::string> &members) override {
            if (members.empty()) {
                return 0;
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("SREM");
            argv_len.push_back(4);
            argv.push_back(key.c_str());
            argv_len.push_back(key.size());

            for (const auto &m: members) {
                argv.push_back(m.c_str());
                argv_len.push_back(m.size());
            }

            const auto reply = execv(argv, argv_len);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("SREM", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        /**
         * @brief Returns all the members of the set.
         * @attention `SMEMBERS` can be a performance risk on production servers for large sets. Consider using `SSCAN`.
         * @param key The set key.
         * @return A vector containing all members of the set. Returns an empty vector if the key does not exist.
         */
        std::vector<std::string> smembers(const std::string &key) override {
            std::vector<std::string> result;
            const auto reply = exec("SMEMBERS %s", key.c_str());
            if (reply->type == REDIS_REPLY_ARRAY) {
                result.reserve(reply->elements);
                for (size_t i = 0; i < reply->elements; ++i) {
                    if (reply->element[i]->type == REDIS_REPLY_STRING) {
                        result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                    }
                }
            }
            else if (reply->type != REDIS_REPLY_NIL) {
                throw unexpected_reply_type_error("SMEMBERS", "array or nil", reply_type_name(reply->type));
            }
            return result;
        }

        long long scard(const std::string &key) override {
            const auto reply = exec("SCARD %s", key.c_str());
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("SCARD", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        bool sismember(const std::string &key, const std::string &member) override {
            const auto reply = exec("SISMEMBER %s %s", key.c_str(), member.c_str());
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("SISMEMBER", "integer", reply_type_name(reply->type));
            }
            return reply->integer == 1;
        }

        std::optional<std::string> spop(const std::string &key) override {
            const auto reply = exec("SPOP %s", key.c_str());
            if (reply->type == REDIS_REPLY_NIL) {
                return std::nullopt;
            }
            if (reply->type == REDIS_REPLY_STRING) {
                return std::string(reply->str, reply->len);
            }
            throw unexpected_reply_type_error("SPOP", "string or nil", reply_type_name(reply->type));
        }

        /**
         * @brief Returns the members resulting from the intersection of all the given sets.
         * @attention `SINTER` can be slow if the sets involved are large.
         * @param keys The keys of the sets to intersect.
         * @return A vector of members resulting from the intersection.
         */
        std::vector<std::string> sinter(const std::vector<std::string> &keys) override {
            if (keys.empty()) {
                return {};
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("SINTER");
            argv_len.push_back(6);
            for (const auto &k: keys) {
                argv.push_back(k.c_str());
                argv_len.push_back(k.size());
            }

            std::vector<std::string> result;
            const auto reply = execv(argv, argv_len);

            if (reply->type == REDIS_REPLY_ARRAY) {
                result.reserve(reply->elements);
                for (size_t i = 0; i < reply->elements; ++i) {
                    if (reply->element[i]->type == REDIS_REPLY_STRING) {
                        result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                    }
                }
            }
            else if (reply->type != REDIS_REPLY_NIL) {
                throw unexpected_reply_type_error("SINTER", "array or nil", reply_type_name(reply->type));
            }
            return result;
        }

        // ============================================================================
        // For ZSet
        // ============================================================================

        long long zadd(const std::string &key, const std::unordered_map<std::string, double> &members) override {
            if (members.empty()) {
                return 0;
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("ZADD");
            argv_len.push_back(4);
            argv.push_back(key.c_str());
            argv_len.push_back(key.size());

            std::vector<std::string> score_strings;
            score_strings.reserve(members.size());

            for (const auto &member: members) {
                score_strings.emplace_back(string_serializable<double>::to_string(member.second));
                argv.push_back(score_strings.back().c_str());
                argv_len.push_back(score_strings.back().size());
                argv.push_back(member.first.c_str());
                argv_len.push_back(member.first.size());
            }

            const auto reply = execv(argv, argv_len);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("ZADD", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long zrem(const std::string &key, const std::vector<std::string> &members) override {
            if (members.empty()) {
                return 0;
            }

            std::vector<const char *> argv;
            std::vector<size_t> argv_len;

            argv.push_back("ZREM");
            argv_len.push_back(4);
            argv.push_back(key.c_str());
            argv_len.push_back(key.size());

            for (const auto &m: members) {
                argv.push_back(m.c_str());
                argv_len.push_back(m.size());
            }

            const auto reply = execv(argv, argv_len);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("ZREM", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        std::optional<double> zscore(const std::string &key, const std::string &member) override {
            const auto reply = exec("ZSCORE %s %s", key.c_str(), member.c_str());
            if (reply->type == REDIS_REPLY_NIL) {
                return std::nullopt;
            }

            if (reply->type == REDIS_REPLY_STRING) {
                try {
                    return string_serializable<double>::from_string(std::string(reply->str, reply->len));
                }
                catch (const std::exception &) {
                    throw std::runtime_error("ZSCORE: failed to convert score to double");
                }
            }
            throw unexpected_reply_type_error("ZSCORE", "string", reply_type_name(reply->type));
        }

        std::vector<std::string> zrange(const std::string &key, long long start, long long stop) override {
            // ZRANGE key start stop
            std::vector<std::string> result;
            const auto reply = exec("ZRANGE %s %lld %lld", key.c_str(), start, stop);

            if (reply->type == REDIS_REPLY_ARRAY) {
                result.reserve(reply->elements);
                for (size_t i = 0; i < reply->elements; ++i) {
                    if (reply->element[i]->type == REDIS_REPLY_STRING) {
                        result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                    }
                }
            }
            else if (reply->type != REDIS_REPLY_NIL) {
                throw unexpected_reply_type_error("ZRANGE", "array or nil", reply_type_name(reply->type));
            }
            return result;
        }

        std::vector<std::string> zrevrange(const std::string &key, long long start, long long stop) override {
            // ZREVRANGE key start stop
            std::vector<std::string> result;
            const auto reply = exec("ZREVRANGE %s %lld %lld", key.c_str(), start, stop);

            if (reply->type == REDIS_REPLY_ARRAY) {
                result.reserve(reply->elements);
                for (size_t i = 0; i < reply->elements; ++i) {
                    if (reply->element[i]->type == REDIS_REPLY_STRING) {
                        result.emplace_back(reply->element[i]->str, reply->element[i]->len);
                    }
                }
            }
            else if (reply->type != REDIS_REPLY_NIL) {
                throw unexpected_reply_type_error("ZREVRANGE", "array or nil", reply_type_name(reply->type));
            }
            return result;
        }

        std::vector<std::pair<std::string, double>> zrange_withscores(const std::string &key, long long start,
                                                                      long long stop) override {
            // ZRANGE key start stop WITHSCORES
            std::vector<std::pair<std::string, double>> result;
            const auto reply = exec("ZRANGE %s %lld %lld WITHSCORES", key.c_str(), start, stop);

            if (reply->type == REDIS_REPLY_ARRAY) {
                if (reply->elements % 2 != 0) {
                    throw std::runtime_error("ZRANGE WITHSCORES: expected even number of elements");
                }
                result.reserve(reply->elements / 2);

                for (size_t i = 0; i + 1 < reply->elements; i += 2) {
                    const redisReply *member_reply = reply->element[i];
                    const redisReply *score_reply = reply->element[i + 1];

                    if (member_reply->type == REDIS_REPLY_STRING && score_reply->type == REDIS_REPLY_STRING) {
                        try {
                            double score = string_serializable<double>::from_string(
                                std::string(score_reply->str, score_reply->len));
                            result.emplace_back(std::string(member_reply->str, member_reply->len), score);
                        }
                        catch (const std::exception &) {
                            throw std::runtime_error("ZRANGE WITHSCORES: score conversion failed");
                        }
                    }
                    else {
                        throw std::runtime_error("ZRANGE WITHSCORES: unexpected element type");
                    }
                }
            }
            else if (reply->type != REDIS_REPLY_NIL) {
                throw unexpected_reply_type_error("ZRANGE WITHSCORES", "array or nil", reply_type_name(reply->type));
            }
            return result;
        }

        std::vector<std::pair<std::string, double>> zrevrange_withscores(const std::string &key, long long start,
                                                                         long long stop) override {
            // ZREVRANGE key start stop WITHSCORES
            std::vector<std::pair<std::string, double>> result;
            const auto reply = exec("ZREVRANGE %s %lld %lld WITHSCORES", key.c_str(), start, stop);

            if (reply->type == REDIS_REPLY_ARRAY) {
                if (reply->elements % 2 != 0) {
                    throw std::runtime_error("ZREVRANGE WITHSCORES: expected even number of elements");
                }
                result.reserve(reply->elements / 2);

                for (size_t i = 0; i + 1 < reply->elements; i += 2) {
                    redisReply *member_reply = reply->element[i];
                    redisReply *score_reply = reply->element[i + 1];

                    if (member_reply->type == REDIS_REPLY_STRING && score_reply->type == REDIS_REPLY_STRING) {
                        try {
                            double score = string_serializable<double>::from_string(
                                std::string(score_reply->str, score_reply->len));
                            result.emplace_back(std::string(member_reply->str, member_reply->len), score);
                        }
                        catch (const std::exception &) {
                            throw std::runtime_error("ZREVRANGE WITHSCORES: score conversion failed");
                        }
                    }
                    else {
                        throw std::runtime_error("ZREVRANGE WITHSCORES: unexpected element type");
                    }
                }
            }
            else if (reply->type != REDIS_REPLY_NIL) {
                throw unexpected_reply_type_error("ZREVRANGE WITHSCORES", "array or nil", reply_type_name(reply->type));
            }
            return result;
        }

        double zincrby(const std::string &key, double increment, const std::string &member) override {
            std::string increment_str = string_serializable<double>::to_string(increment);

            // ZINCRBY key increment member
            const auto reply = exec("ZINCRBY %s %s %s", key.c_str(), increment_str.c_str(), member.c_str());

            if (reply->type == REDIS_REPLY_STRING) {
                try {
                    return string_serializable<double>::from_string(std::string(reply->str, reply->len));
                }
                catch (const std::exception &) {
                    throw std::runtime_error("ZINCRBY: failed to convert returned score to double");
                }
            }
            throw unexpected_reply_type_error("ZINCRBY", "string", reply_type_name(reply->type));
        }

        // ============================================================================
        // For Stream
        // ============================================================================

        std::optional<std::string> xadd(const std::string &key,
                                        const std::vector<std::pair<std::string, std::string>> &fields,
                                        const stream_add_options &options) override {
            if (fields.empty()) {
                throw std::invalid_argument("XADD: fields must not be empty");
            }
            if (options.id.empty()) {
                throw std::invalid_argument("XADD: id must not be empty");
            }

            std::vector<std::string> args{"XADD", key};
            if (options.nomkstream) {
                args.emplace_back("NOMKSTREAM");
            }
            if (options.trim) {
                append_stream_trim_args(args, *options.trim, "XADD");
            }
            args.push_back(options.id);
            for (const auto &field: fields) {
                args.push_back(field.first);
                args.push_back(field.second);
            }

            const auto reply = exec_args(args);
            if (reply->type == REDIS_REPLY_STRING) {
                return std::string(reply->str, reply->len);
            }
            if (reply->type == REDIS_REPLY_NIL) {
                return std::nullopt;
            }
            throw unexpected_reply_type_error("XADD", "string or nil", reply_type_name(reply->type));
        }

        long long xlen(const std::string &key) override {
            const auto reply = exec_args({"XLEN", key});
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("XLEN", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        std::vector<string_stream_entry> xrange(const std::string &key, const std::string &start,
                                                const std::string &end, std::optional<long long> count) override {
            std::vector<std::string> args{"XRANGE", key, start, end};
            append_stream_count_arg(args, count, "XRANGE");
            const auto reply = exec_args(args);
            return parse_stream_entries(reply.get(), "XRANGE");
        }

        std::vector<string_stream_entry> xrevrange(const std::string &key, const std::string &end,
                                                   const std::string &start, std::optional<long long> count) override {
            std::vector<std::string> args{"XREVRANGE", key, end, start};
            append_stream_count_arg(args, count, "XREVRANGE");
            const auto reply = exec_args(args);
            return parse_stream_entries(reply.get(), "XREVRANGE");
        }

        std::vector<string_stream_batch> xread(const std::vector<string_stream_read_request> &streams,
                                               const stream_read_options &options) override {
            validate_stream_requests(streams, "XREAD");
            std::vector<std::string> args{"XREAD"};
            append_stream_count_arg(args, options.count, "XREAD");
            append_stream_block_arg(args, options.block_ms, "XREAD");
            append_stream_requests(args, streams);
            const auto reply = exec_args(args);
            return parse_stream_batches(reply.get(), "XREAD");
        }

        long long xdel(const std::string &key, const std::vector<std::string> &ids) override {
            if (ids.empty()) {
                return 0;
            }
            std::vector<std::string> args{"XDEL", key};
            args.insert(args.end(), ids.begin(), ids.end());
            const auto reply = exec_args(args);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("XDEL", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        long long xtrim(const std::string &key, const stream_trim_options &options) override {
            std::vector<std::string> args{"XTRIM", key};
            append_stream_trim_args(args, options, "XTRIM");
            const auto reply = exec_args(args);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("XTRIM", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        bool xgroup_create(const std::string &key, const std::string &group, const std::string &id,
                           bool mkstream) override {
            std::vector<std::string> args{"XGROUP", "CREATE", key, group, id};
            if (mkstream) {
                args.emplace_back("MKSTREAM");
            }
            return expect_ok(exec_args(args), "XGROUP CREATE");
        }

        bool xgroup_setid(const std::string &key, const std::string &group, const std::string &id) override {
            return expect_ok(exec_args({"XGROUP", "SETID", key, group, id}), "XGROUP SETID");
        }

        bool xgroup_destroy(const std::string &key, const std::string &group) override {
            const auto reply = exec_args({"XGROUP", "DESTROY", key, group});
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("XGROUP DESTROY", "integer", reply_type_name(reply->type));
            }
            return reply->integer == 1;
        }

        bool xgroup_createconsumer(const std::string &key, const std::string &group,
                                   const std::string &consumer) override {
            const auto reply = exec_args({"XGROUP", "CREATECONSUMER", key, group, consumer});
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("XGROUP CREATECONSUMER", "integer", reply_type_name(reply->type));
            }
            return reply->integer == 1;
        }

        long long xgroup_delconsumer(const std::string &key, const std::string &group,
                                     const std::string &consumer) override {
            const auto reply = exec_args({"XGROUP", "DELCONSUMER", key, group, consumer});
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("XGROUP DELCONSUMER", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        std::vector<string_stream_batch> xreadgroup(const std::string &group, const std::string &consumer,
                                                    const std::vector<string_stream_read_request> &streams,
                                                    const stream_read_group_options &options) override {
            validate_stream_requests(streams, "XREADGROUP");
            std::vector<std::string> args{"XREADGROUP", "GROUP", group, consumer};
            append_stream_count_arg(args, options.count, "XREADGROUP");
            append_stream_block_arg(args, options.block_ms, "XREADGROUP");
            if (options.noack) {
                args.emplace_back("NOACK");
            }
            append_stream_requests(args, streams);
            const auto reply = exec_args(args);
            return parse_stream_batches(reply.get(), "XREADGROUP");
        }

        long long xack(const std::string &key, const std::string &group, const std::vector<std::string> &ids) override {
            if (ids.empty()) {
                return 0;
            }
            std::vector<std::string> args{"XACK", key, group};
            args.insert(args.end(), ids.begin(), ids.end());
            const auto reply = exec_args(args);
            if (reply->type != REDIS_REPLY_INTEGER) {
                throw unexpected_reply_type_error("XACK", "integer", reply_type_name(reply->type));
            }
            return reply->integer;
        }

        std::string script_load(const std::string &script) override {
            // SCRIPT LOAD <script> -> returns SHA1 digest of the script
            std::vector<const char *> argv;
            std::vector<size_t> argv_len;
            argv.push_back("SCRIPT");
            argv_len.push_back(6); // legnth of "SCRIPT"
            argv.push_back("LOAD");
            argv_len.push_back(4); // length of "LOAD"
            argv.push_back(script.c_str());
            argv_len.push_back(script.size());
            const auto reply = execv(argv, argv_len);
            if (reply->type == REDIS_REPLY_STRING) {
                return {reply->str, reply->len};
            }
            throw unexpected_reply_type_error("SCRIPT LOAD", "string", reply_type_name(reply->type));
        }

        cmd_reply eval_sha1(const std::string &sha1, const std::vector<std::string> &keys,
                            const std::vector<std::string> &args) override {
            std::vector<const char *> argv;
            std::vector<size_t> argv_len;
            // EVALSHA sha1 numkeys key [key ...] arg [arg ...]
            argv.push_back("EVALSHA");
            argv_len.push_back(7); // length of "EVALSHA"
            argv.push_back(sha1.c_str());
            argv_len.push_back(sha1.size());
            if (keys.size() > MAX_SCRIPT_KEYS) {
                throw too_many_script_keys_error("EVALSHA", keys.size(), MAX_SCRIPT_KEYS);
            }
            char num_keys_buf[NUM_KEYS_BUF_SIZE];
            int num_keys_len = std::snprintf(num_keys_buf, sizeof(num_keys_buf), "%zu", keys.size());
            if (num_keys_len < 0) {
                throw std::runtime_error("snprintf format command failed");
            }
            argv.push_back(num_keys_buf);
            argv_len.push_back(static_cast<size_t>(num_keys_len));
            for (const auto &k: keys) {
                argv.push_back(k.c_str());
                argv_len.push_back(k.size());
            }
            for (const auto &a: args) {
                argv.push_back(a.c_str());
                argv_len.push_back(a.size());
            }

            // Use redisCommandArgv directly so we can inspect error replies (e.g. NOSCRIPT)
            // NOLINTNEXTLINE
            redisReply *r = static_cast<redisReply *>(
                redisCommandArgv(context.get(), static_cast<int>(argv.size()), argv.data(), argv_len.data()));
            if (!r) {
                throw std::runtime_error("CommandArgv failed");
            }
            redis_reply_ptr reply(r);
            if (reply->type == REDIS_REPLY_ERROR) {
                std::string err(reply->str, reply->len);
                if (err.find("NOSCRIPT") != std::string::npos) {
                    throw no_script_error("EVALSHA", sha1);
                }
                throw redis_error(err);
            }
            return parse_reply(reply.get());
        }

        cmd_reply eval(const std::string &script, const std::vector<std::string> &keys,
                       const std::vector<std::string> &args) override {
            std::vector<const char *> argv;
            std::vector<size_t> argv_len;
            // EVAL script numkeys key [key ...] arg [arg ...]
            argv.push_back("EVAL");
            argv_len.push_back(4); // length of "EVAL"
            argv.push_back(script.c_str());
            argv_len.push_back(script.size());
            if (keys.size() > MAX_SCRIPT_KEYS) {
                throw too_many_script_keys_error("EVAL", keys.size(), MAX_SCRIPT_KEYS);
            }
            char num_keys_buf[NUM_KEYS_BUF_SIZE];
            int num_keys_len = std::snprintf(num_keys_buf, sizeof(num_keys_buf), "%zu", keys.size());
            if (num_keys_len < 0) {
                throw std::runtime_error("snprintf format command failed");
            }
            argv.push_back(num_keys_buf);
            argv_len.push_back(static_cast<size_t>(num_keys_len)); // length of num_keys
            for (const auto &k: keys) {
                argv.push_back(k.c_str());
                argv_len.push_back(k.size());
            }
            for (const auto &a: args) {
                argv.push_back(a.c_str());
                argv_len.push_back(a.size());
            }
            const auto reply = execv(argv, argv_len);
            return parse_reply(reply.get());
        }

        cmd_reply command(const std::string &cmd, const std::vector<std::string> &args) override {
            std::vector<const char *> argv;
            std::vector<size_t> argv_len;
            argv.push_back(cmd.c_str());
            argv_len.push_back(cmd.size());
            for (const auto &a: args) {
                argv.push_back(a.c_str());
                argv_len.push_back(a.size());
            }
            const auto reply = execv(argv, argv_len);
            return parse_reply(reply.get());
        }

    protected:
        /**
         * @brief Executes a command represented as binary-safe string arguments.
         */
        [[nodiscard]] redis_reply_ptr exec_args(const std::vector<std::string> &args) const {
            if (args.empty()) {
                throw std::invalid_argument("Redis command arguments must not be empty");
            }
            std::vector<const char *> argv;
            std::vector<size_t> argv_len;
            argv.reserve(args.size());
            argv_len.reserve(args.size());
            for (const auto &arg: args) {
                argv.push_back(arg.data());
                argv_len.push_back(arg.size());
            }
            return execv(argv, argv_len);
        }

        static bool expect_ok(const redis_reply_ptr &reply, const std::string &command_name) {
            if (reply->type != REDIS_REPLY_STATUS) {
                throw unexpected_reply_type_error(command_name, "status", reply_type_name(reply->type));
            }
            return std::string(reply->str, reply->len) == "OK";
        }

        static void append_stream_count_arg(std::vector<std::string> &args, std::optional<long long> count,
                                            const std::string &command_name) {
            if (!count) {
                return;
            }
            if (*count <= 0) {
                throw std::invalid_argument(command_name + ": count must be greater than zero");
            }
            args.emplace_back("COUNT");
            args.push_back(std::to_string(*count));
        }

        static void append_stream_block_arg(std::vector<std::string> &args, std::optional<long long> block_ms,
                                            const std::string &command_name) {
            if (!block_ms) {
                return;
            }
            if (*block_ms < 0) {
                throw std::invalid_argument(command_name + ": block_ms must not be negative");
            }
            args.emplace_back("BLOCK");
            args.push_back(std::to_string(*block_ms));
        }

        static void append_stream_trim_args(std::vector<std::string> &args, const stream_trim_options &options,
                                            const std::string &command_name) {
            if (options.threshold.empty()) {
                throw std::invalid_argument(command_name + ": trim threshold must not be empty");
            }
            if (options.limit && *options.limit < 0) {
                throw std::invalid_argument(command_name + ": trim limit must not be negative");
            }
            if (options.limit && !options.approximate) {
                throw std::invalid_argument(command_name + ": trim limit requires approximate trimming");
            }

            args.emplace_back(options.strategy == stream_trim_strategy::MAXLEN ? "MAXLEN" : "MINID");
            if (options.approximate) {
                args.emplace_back("~");
            }
            args.push_back(options.threshold);
            if (options.limit) {
                args.emplace_back("LIMIT");
                args.push_back(std::to_string(*options.limit));
            }
        }

        static void validate_stream_requests(const std::vector<string_stream_read_request> &streams,
                                             const std::string &command_name) {
            if (streams.empty()) {
                throw std::invalid_argument(command_name + ": streams must not be empty");
            }
            for (const auto &stream: streams) {
                if (stream.id.empty()) {
                    throw std::invalid_argument(command_name + ": stream id must not be empty");
                }
            }
        }

        static void append_stream_requests(std::vector<std::string> &args,
                                           const std::vector<string_stream_read_request> &streams) {
            args.emplace_back("STREAMS");
            for (const auto &stream: streams) {
                args.push_back(stream.key);
            }
            for (const auto &stream: streams) {
                args.push_back(stream.id);
            }
        }

        static string_stream_entry parse_stream_entry(const redisReply *reply, const std::string &command_name) {
            if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements != 2) {
                throw unexpected_reply_type_error(command_name, "two-element stream entry array",
                                                  reply ? reply_type_name(reply->type) : "null");
            }
            if (reply->element[0]->type != REDIS_REPLY_STRING) {
                throw unexpected_reply_type_error(command_name, "string entry id",
                                                  reply_type_name(reply->element[0]->type));
            }

            string_stream_entry result;
            result.id.assign(reply->element[0]->str, reply->element[0]->len);
            const redisReply *fields = reply->element[1];
            // XREADGROUP can return a nil payload when a pending entry was deleted from the stream.
            if (fields->type == REDIS_REPLY_NIL) {
                return result;
            }
            if (fields->type != REDIS_REPLY_ARRAY) {
                throw unexpected_reply_type_error(command_name, "field-value array or nil",
                                                  reply_type_name(fields->type));
            }
            if (fields->elements % 2 != 0) {
                throw std::runtime_error(command_name + ": expected an even number of field-value elements");
            }

            result.fields.reserve(fields->elements / 2);
            for (size_t i = 0; i < fields->elements; i += 2) {
                const redisReply *field = fields->element[i];
                const redisReply *value = fields->element[i + 1];
                if (field->type != REDIS_REPLY_STRING || value->type != REDIS_REPLY_STRING) {
                    throw std::runtime_error(command_name + ": unexpected stream field or value reply type");
                }
                result.fields.emplace_back(std::string(field->str, field->len), std::string(value->str, value->len));
            }
            return result;
        }

        static std::vector<string_stream_entry> parse_stream_entries(const redisReply *reply,
                                                                     const std::string &command_name) {
            std::vector<string_stream_entry> result;
            if (reply->type == REDIS_REPLY_NIL) {
                return result;
            }
            if (reply->type != REDIS_REPLY_ARRAY) {
                throw unexpected_reply_type_error(command_name, "array or nil", reply_type_name(reply->type));
            }
            result.reserve(reply->elements);
            for (size_t i = 0; i < reply->elements; ++i) {
                result.push_back(parse_stream_entry(reply->element[i], command_name));
            }
            return result;
        }

        static std::vector<string_stream_batch> parse_stream_batches(const redisReply *reply,
                                                                     const std::string &command_name) {
            std::vector<string_stream_batch> result;
            if (reply->type == REDIS_REPLY_NIL) {
                return result;
            }
            if (reply->type != REDIS_REPLY_ARRAY) {
                throw unexpected_reply_type_error(command_name, "array or nil", reply_type_name(reply->type));
            }
            result.reserve(reply->elements);
            for (size_t i = 0; i < reply->elements; ++i) {
                const redisReply *batch_reply = reply->element[i];
                if (batch_reply->type != REDIS_REPLY_ARRAY || batch_reply->elements != 2) {
                    throw unexpected_reply_type_error(command_name, "two-element stream batch array",
                                                      reply_type_name(batch_reply->type));
                }
                if (batch_reply->element[0]->type != REDIS_REPLY_STRING) {
                    throw unexpected_reply_type_error(command_name, "string stream key",
                                                      reply_type_name(batch_reply->element[0]->type));
                }

                string_stream_batch batch;
                batch.key.assign(batch_reply->element[0]->str, batch_reply->element[0]->len);
                batch.entries = parse_stream_entries(batch_reply->element[1], command_name);
                result.push_back(std::move(batch));
            }
            return result;
        }

        // parse redis reply to cmd_reply
        // NOLINTNEXTLINE
        /**
         * @brief Recursively parses a hiredis redisReply object into a redisdal::cmd_reply.
         *
         * This function handles the translation between the C-style hiredis reply
         * and the C++ object-oriented cmd_reply, including nested arrays.
         * @param r A pointer to the redisReply to parse.
         * @return A cmd_reply object representing the Redis response.
         */
        static cmd_reply parse_reply(const redisReply *r) {
            if (!r) {
                return cmd_reply::make_error("Null reply");
            }
            switch (r->type) {
                case REDIS_REPLY_STRING:
                    return cmd_reply::make_string(std::string(r->str, r->len));
                case REDIS_REPLY_INTEGER:
                    return cmd_reply::make_integer(r->integer);
                case REDIS_REPLY_ARRAY: {
                    std::vector<cmd_reply> elements;
                    elements.reserve(r->elements);
                    for (size_t i = 0; i < r->elements; ++i) {
                        elements.push_back(parse_reply(r->element[i]));
                    }
                    return cmd_reply::make_array(std::move(elements));
                }
                case REDIS_REPLY_NIL:
                    return cmd_reply::make_nil();
                case REDIS_REPLY_STATUS:
                    return cmd_reply::make_status(std::string(r->str, r->len));
                case REDIS_REPLY_ERROR:
                    return cmd_reply::make_error(std::string(r->str, r->len));
                default:
                    return cmd_reply::make_error("Unknown reply type");
            }
        }

        // Map redisdal::reply_type enum to human-readable name
        /**
         * @brief Maps a redisdal::reply_type enum to its human-readable string name.
         * @param t The reply_type enum value.
         * @return A C-style string representing the name (e.g., "string", "array").
         */
        static const char *reply_type_name(reply_type t) {
            return reply_type_name(static_cast<unsigned int>(t));
        }

        // Accept raw int (hiredis) reply type as well
        /**
         * @brief Maps a raw hiredis reply type integer to its human-readable string name.
         * @param type The integer reply type from a redisReply object.
         * @return A C-style string representing the name (e.g., "string", "array").
         */
        static const char *reply_type_name(unsigned int type) {
            switch (type) {
                case REDIS_REPLY_STRING:
                    return "string";
                case REDIS_REPLY_INTEGER:
                    return "integer";
                case REDIS_REPLY_ARRAY:
                    return "array";
                case REDIS_REPLY_NIL:
                    return "nil";
                case REDIS_REPLY_STATUS:
                    return "status";
                case REDIS_REPLY_ERROR:
                    return "error";
                default:
                    return "unknown";
            }
        }

        /**
         * @brief Executes a Redis command with a printf-style format string.
         * @param fmt The format string for the command.
         * @param ... Arguments for the format string.
         * @return A smart pointer to the Redis reply.
         * @throws std::runtime_error if the command fails at the hiredis level.
         * @throws redis_error if Redis returns an error reply.
         */
        redis_reply_ptr exec(const char *fmt, ...) const {
            va_list ap;
            va_start(ap, fmt);
            // NOLINTNEXTLINE
            redisReply *r = static_cast<redisReply *>(redisvCommand(context.get(), fmt, ap));
            va_end(ap);
            if (!r) {
                throw std::runtime_error("Command failed");
            }
            if (r->type == REDIS_REPLY_ERROR) {
                std::string err(r->str, r->len);
                freeReplyObject(r);
                throw redis_error(err);
            }
            return redis_reply_ptr(r);
        }

        /**
         * @brief Executes a Redis command with an array of arguments.
         * @param argv Vector of C-style strings representing the command and its arguments.
         * @param argv_len Vector of lengths for each string in argv.
         * @return A smart pointer to the Redis reply.
         * @throws std::runtime_error if the command fails at the hiredis level.
         * @throws redis_error if Redis returns an error reply.
         */
        [[nodiscard]] redis_reply_ptr execv(const std::vector<const char *> &argv,
                                            const std::vector<size_t> &argv_len) const {
            // NOLINTNEXTLINE
            redisReply *r = static_cast<redisReply *>(redisCommandArgv(
                context.get(), static_cast<int>(argv.size()), const_cast<const char **>(argv.data()), argv_len.data()));
            if (!r) {
                throw std::runtime_error("CommandArgv failed");
            }
            if (r->type == REDIS_REPLY_ERROR) {
                std::string err(r->str, r->len);
                freeReplyObject(r);
                throw redis_error(err);
            }
            return redis_reply_ptr(r);
        }

    private:
        redis_context_ptr context;
    };
} // namespace redisdal
