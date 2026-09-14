#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "redis_executor.hpp"

namespace redisdal {
    static constexpr size_t MAX_SCRIPT_KEYS = 64;

    /**
     * Reusable Redis command construction layer.
     *
     * Operations build argv and delegate to cmd_exec()/cmd_execv(). Value-returning
     * commands expose cmd_reply; script helpers retain the reference project's
     * public behavior. The layer is shared by synchronous, asynchronous, and
     * Pipeline connection types.
     */
    // Paired with redis_async_executor's virtual inheritance so a future
    // Pipeline can reuse this command layer without duplicating redis_executor.
    class redis_cmd_ops: public virtual redis_executor {
    public:
        redis_cmd_ops() = default;
        ~redis_cmd_ops() override = default;

        std::string script_load(const std::string &script);
        cmd_reply eval_sha1(const std::string &sha1, const std::vector<std::string> &keys,
                            const std::vector<std::string> &args);
        cmd_reply eval(const std::string &script, const std::vector<std::string> &keys,
                       const std::vector<std::string> &args);
        cmd_reply command(const std::string &cmd, const std::vector<std::string> &args);

        cmd_reply exists(const std::string &key);
        cmd_reply keys(const std::string &pattern);
        cmd_reply scan(uint64_t cursor, const std::string &pattern, unsigned int count);
        cmd_reply type(const std::string &key);
        cmd_reply del(const std::string &key);
        cmd_reply del(const std::vector<std::string> &keys);
        cmd_reply expire(const std::string &key, long long seconds);
        cmd_reply pexpire(const std::string &key, long long milliseconds);
        cmd_reply ttl(const std::string &key);
        cmd_reply pttl(const std::string &key);
        cmd_reply persist(const std::string &key);
        cmd_reply ping();
        cmd_reply ping(const std::string &message);

        cmd_reply set(const std::string &key, const std::string &value);
        cmd_reply set_not_exists(const std::string &key, const std::string &value);
        cmd_reply set_ex(const std::string &key, const std::string &value, long long seconds);
        cmd_reply set_px(const std::string &key, const std::string &value, long long milliseconds);
        cmd_reply get(const std::string &key);
        cmd_reply getset(const std::string &key, const std::string &new_value);
        cmd_reply append(const std::string &key, const std::string &value);
        cmd_reply incr(const std::string &key, int64_t delta);
        cmd_reply decr(const std::string &key, int64_t delta);

        cmd_reply hget(const std::string &key, const std::string &field);
        cmd_reply hget(const std::string &key, const std::vector<std::string> &fields);
        cmd_reply hset(const std::string &key, const std::string &field, const std::string &value);
        cmd_reply hset(const std::string &key, const std::unordered_map<std::string, std::string> &values);
        cmd_reply hgetall(const std::string &key);
        cmd_reply hkeys(const std::string &key);
        cmd_reply hvals(const std::string &key);
        cmd_reply hscan(const std::string &key, uint64_t cursor, const std::string &pattern, unsigned int count);
        cmd_reply hdel(const std::string &key, const std::string &field);
        cmd_reply hdel(const std::string &key, const std::vector<std::string> &fields);

        cmd_reply lpush(const std::string &key, const std::string &value);
        cmd_reply lpush(const std::string &key, const std::vector<std::string> &values);
        cmd_reply rpush(const std::string &key, const std::string &value);
        cmd_reply rpush(const std::string &key, const std::vector<std::string> &values);
        cmd_reply lpop(const std::string &key);
        cmd_reply lpop(const std::string &key, int count);
        cmd_reply rpop(const std::string &key);
        cmd_reply rpop(const std::string &key, int count);
        cmd_reply lrange(const std::string &key, long long start, long long stop);
        cmd_reply llen(const std::string &key);
        cmd_reply lindex(const std::string &key, long long index);

        cmd_reply sadd(const std::string &key, const std::vector<std::string> &members);
        cmd_reply srem(const std::string &key, const std::vector<std::string> &members);
        cmd_reply smembers(const std::string &key);
        cmd_reply scard(const std::string &key);
        cmd_reply sismember(const std::string &key, const std::string &member);
        cmd_reply spop(const std::string &key);
        cmd_reply sinter(const std::vector<std::string> &keys);

        cmd_reply zadd(const std::string &key, const std::unordered_map<std::string, double> &members);
        cmd_reply zrem(const std::string &key, const std::vector<std::string> &members);
        cmd_reply zscore(const std::string &key, const std::string &member);
        cmd_reply zrange(const std::string &key, long long start, long long stop);
        cmd_reply zrevrange(const std::string &key, long long start, long long stop);
        cmd_reply zrange_withscores(const std::string &key, long long start, long long stop);
        cmd_reply zrevrange_withscores(const std::string &key, long long start, long long stop);
        cmd_reply zincrby(const std::string &key, double increment, const std::string &member);

        cmd_reply xadd(const std::string &key, const std::vector<std::pair<std::string, std::string>> &fields,
                       const stream_add_options &options);
        cmd_reply xlen(const std::string &key);
        cmd_reply xrange(const std::string &key, const std::string &start, const std::string &end,
                         std::optional<long long> count);
        cmd_reply xrevrange(const std::string &key, const std::string &end, const std::string &start,
                            std::optional<long long> count);
        cmd_reply xread(const std::vector<string_stream_read_request> &streams, const stream_read_options &options);
        cmd_reply xdel(const std::string &key, const std::vector<std::string> &ids);
        cmd_reply xtrim(const std::string &key, const stream_trim_options &options);
        cmd_reply xgroup_create(const std::string &key, const std::string &group, const std::string &id, bool mkstream);
        cmd_reply xgroup_setid(const std::string &key, const std::string &group, const std::string &id);
        cmd_reply xgroup_destroy(const std::string &key, const std::string &group);
        cmd_reply xgroup_createconsumer(const std::string &key, const std::string &group, const std::string &consumer);
        cmd_reply xgroup_delconsumer(const std::string &key, const std::string &group, const std::string &consumer);
        cmd_reply xreadgroup(const std::string &group, const std::string &consumer,
                             const std::vector<string_stream_read_request> &streams,
                             const stream_read_group_options &options);
        cmd_reply xack(const std::string &key, const std::string &group, const std::vector<std::string> &ids);
    };
} // namespace redisdal
