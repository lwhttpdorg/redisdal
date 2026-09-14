#pragma once

#include <cstddef>
#include <memory>

#include "kv_connection.hpp"
#include "redis_connection.hpp"

namespace redisdal {
    /**
     * String client layer. It owns a redis_connection, implements kv_connection,
     * and interprets owning command replies.
     */
    class redis_client: public kv_connection {
    public:
        explicit redis_client(const std::string &url);
        explicit redis_client(std::unique_ptr<redis_connection> connection);
        ~redis_client() override = default;
        redis_client(const redis_client &) = delete;
        redis_client &operator=(const redis_client &) = delete;
        redis_client(redis_client &&) noexcept = default;
        redis_client &operator=(redis_client &&) noexcept = default;

        /** @copydoc kv_connection::exists */
        bool exists(const std::string &key) override;

        /** @copydoc kv_connection::keys */
        void keys(const std::string &pattern, std::unordered_set<std::string> &keys) override;

        /** @copydoc kv_connection::scan */
        string_scan_result scan(uint64_t cursor, const std::string &pattern, unsigned int count) override;

        /** @copydoc kv_connection::type */
        std::string type(const std::string &key) override;

        /** @copydoc kv_connection::expire */
        bool expire(const std::string &key, long long seconds) override;

        /** @copydoc kv_connection::pexpire */
        bool pexpire(const std::string &key, long long milliseconds) override;

        /** @copydoc kv_connection::ttl */
        int64_t ttl(const std::string &key) override;

        /** @copydoc kv_connection::pttl */
        int64_t pttl(const std::string &key) override;

        /** @copydoc kv_connection::persist */
        bool persist(const std::string &key) override;

        /** @copydoc kv_connection::ping */
        std::string ping() override;

        /** @copydoc kv_connection::ping */
        std::string ping(const std::string &message) override;

        /** @copydoc kv_connection::del */
        long long del(const std::string &key) override;

        /** @copydoc kv_connection::del */
        long long del(const std::vector<std::string> &keys) override;

        /** @copydoc kv_connection::set */
        bool set(const std::string &key, const std::string &value) override;

        /** @copydoc kv_connection::set_not_exists */
        bool set_not_exists(const std::string &key, const std::string &value) override;

        /** @copydoc kv_connection::set_ex */
        bool set_ex(const std::string &key, const std::string &value, long long seconds) override;

        /** @copydoc kv_connection::set_px */
        bool set_px(const std::string &key, const std::string &value, long long milliseconds) override;

        /** @copydoc kv_connection::get */
        std::optional<std::string> get(const std::string &key) override;

        /** @copydoc kv_connection::getset */
        std::optional<std::string> getset(const std::string &key, const std::string &new_value) override;

        /** @copydoc kv_connection::incr */
        long long incr(const std::string &key, long long delta) override;

        /** @copydoc kv_connection::decr */
        long long decr(const std::string &key, long long delta) override;

        /** @copydoc kv_connection::append */
        long long append(const std::string &key, const std::string &value) override;

        /** @copydoc kv_connection::hget */
        std::optional<std::string> hget(const std::string &key, const std::string &hash_key) override;

        /** @copydoc kv_connection::hget */
        void hget(const std::string &key,
                  std::unordered_map<std::string, std::optional<std::string>> &hash_map) override;

        /** @copydoc kv_connection::hset */
        bool hset(const std::string &key, const std::string &field, const std::string &value) override;

        /** @copydoc kv_connection::hset */
        bool hset(const std::string &key, const std::unordered_map<std::string, std::string> &hash_map) override;

        /** @copydoc kv_connection::hgetall */
        std::unordered_map<std::string, std::string> hgetall(const std::string &key) override;

        /** @copydoc kv_connection::hkeys */
        std::vector<std::string> hkeys(const std::string &key) override;

        /** @copydoc kv_connection::hvals */
        std::vector<std::string> hvals(const std::string &key) override;

        /** @copydoc kv_connection::hscan */
        uint64_t hscan(const std::string &key, uint64_t cursor, const std::string &pattern, unsigned int count,
                       std::unordered_map<std::string, std::string> &hash_map) override;

        /** @copydoc kv_connection::hdel */
        long long hdel(const std::string &key, const std::string &hash_key) override;

        /** @copydoc kv_connection::hdel */
        long long hdel(const std::string &key, const std::vector<std::string> &hash_keys) override;

        /** @copydoc kv_connection::lpush */
        long long lpush(const std::string &key, const std::vector<std::string> &values) override;

        /** @copydoc kv_connection::lpush */
        long long lpush(const std::string &key, const std::string &value) override;

        /** @copydoc kv_connection::rpush */
        long long rpush(const std::string &key, const std::string &value) override;

        /** @copydoc kv_connection::rpush */
        long long rpush(const std::string &key, const std::vector<std::string> &values) override;

        /** @copydoc kv_connection::lpop */
        std::optional<std::string> lpop(const std::string &key) override;

        /** @copydoc kv_connection::rpop */
        std::optional<std::string> rpop(const std::string &key) override;

        /** @copydoc kv_connection::lpop */
        std::vector<std::string> lpop(const std::string &key, int count) override;

        /** @copydoc kv_connection::rpop */
        std::vector<std::string> rpop(const std::string &key, int count) override;

        /** @copydoc kv_connection::lrange */
        std::vector<std::string> lrange(const std::string &key, long long start, long long stop) override;

        /** @copydoc kv_connection::llen */
        long long llen(const std::string &key) override;

        /** @copydoc kv_connection::lindex */
        std::optional<std::string> lindex(const std::string &key, long long index) override;

        /** @copydoc kv_connection::sadd */
        long long sadd(const std::string &key, const std::vector<std::string> &members) override;

        /** @copydoc kv_connection::srem */
        long long srem(const std::string &key, const std::vector<std::string> &members) override;

        /** @copydoc kv_connection::smembers */
        std::vector<std::string> smembers(const std::string &key) override;

        /** @copydoc kv_connection::scard */
        long long scard(const std::string &key) override;

        /** @copydoc kv_connection::sismember */
        bool sismember(const std::string &key, const std::string &member) override;

        /** @copydoc kv_connection::spop */
        std::optional<std::string> spop(const std::string &key) override;

        /** @copydoc kv_connection::sinter */
        std::vector<std::string> sinter(const std::vector<std::string> &keys) override;

        /** @copydoc kv_connection::zadd */
        long long zadd(const std::string &key, const std::unordered_map<std::string, double> &members) override;

        /** @copydoc kv_connection::zrem */
        long long zrem(const std::string &key, const std::vector<std::string> &members) override;

        /** @copydoc kv_connection::zscore */
        std::optional<double> zscore(const std::string &key, const std::string &member) override;

        /** @copydoc kv_connection::zrange */
        std::vector<std::string> zrange(const std::string &key, long long start, long long stop) override;

        /** @copydoc kv_connection::zrevrange */
        std::vector<std::string> zrevrange(const std::string &key, long long start, long long stop) override;

        /** @copydoc kv_connection::zrange_withscores */
        std::vector<std::pair<std::string, double>> zrange_withscores(const std::string &key, long long start,
                                                                      long long stop) override;

        /** @copydoc kv_connection::zrevrange_withscores */
        std::vector<std::pair<std::string, double>> zrevrange_withscores(const std::string &key, long long start,
                                                                         long long stop) override;

        /** @copydoc kv_connection::zincrby */
        double zincrby(const std::string &key, double increment, const std::string &member) override;

        /** @copydoc kv_connection::xadd */
        std::optional<std::string> xadd(const std::string &key,
                                        const std::vector<std::pair<std::string, std::string>> &fields,
                                        const stream_add_options &options) override;

        /** @copydoc kv_connection::xlen */
        long long xlen(const std::string &key) override;

        /** @copydoc kv_connection::xrange */
        std::vector<string_stream_entry> xrange(const std::string &key, const std::string &start,
                                                const std::string &end, std::optional<long long> count) override;

        /** @copydoc kv_connection::xrevrange */
        std::vector<string_stream_entry> xrevrange(const std::string &key, const std::string &end,
                                                   const std::string &start, std::optional<long long> count) override;

        /** @copydoc kv_connection::xread */
        std::vector<string_stream_batch> xread(const std::vector<string_stream_read_request> &streams,
                                               const stream_read_options &options) override;

        /** @copydoc kv_connection::xdel */
        long long xdel(const std::string &key, const std::vector<std::string> &ids) override;

        /** @copydoc kv_connection::xtrim */
        long long xtrim(const std::string &key, const stream_trim_options &options) override;

        /** @copydoc kv_connection::xgroup_create */
        bool xgroup_create(const std::string &key, const std::string &group, const std::string &id,
                           bool mkstream) override;

        /** @copydoc kv_connection::xgroup_setid */
        bool xgroup_setid(const std::string &key, const std::string &group, const std::string &id) override;

        /** @copydoc kv_connection::xgroup_destroy */
        bool xgroup_destroy(const std::string &key, const std::string &group) override;

        /** @copydoc kv_connection::xgroup_createconsumer */
        bool xgroup_createconsumer(const std::string &key, const std::string &group,
                                   const std::string &consumer) override;

        /** @copydoc kv_connection::xgroup_delconsumer */
        long long xgroup_delconsumer(const std::string &key, const std::string &group,
                                     const std::string &consumer) override;

        /** @copydoc kv_connection::xreadgroup */
        std::vector<string_stream_batch> xreadgroup(const std::string &group, const std::string &consumer,
                                                    const std::vector<string_stream_read_request> &streams,
                                                    const stream_read_group_options &options) override;

        /** @copydoc kv_connection::xack */
        long long xack(const std::string &key, const std::string &group, const std::vector<std::string> &ids) override;

        /** @copydoc kv_connection::script_load */
        std::string script_load(const std::string &script) override;

        /** @copydoc kv_connection::eval_sha1 */
        cmd_reply eval_sha1(const std::string &sha1, const std::vector<std::string> &keys,
                            const std::vector<std::string> &args) override;

        /** @copydoc kv_connection::eval */
        cmd_reply eval(const std::string &script, const std::vector<std::string> &keys,
                       const std::vector<std::string> &args) override;

        /** @copydoc kv_connection::command */
        cmd_reply command(const std::string &cmd, const std::vector<std::string> &args) override;

    private:
        std::unique_ptr<redis_connection> connection;
    };
} // namespace redisdal
