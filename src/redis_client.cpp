#include "redisdal/redis_client.hpp"

#include <stdexcept>
#include <utility>

#include "redisdal/exception.hpp"
#include "redisdal/sync_connection.hpp"

namespace redisdal {
    redis_client::redis_client(const std::string &url) : redis_client(std::make_unique<sync_connection>(url)) {
    }

    redis_client::redis_client(std::unique_ptr<redis_connection> connection) : connection(std::move(connection)) {
        if (!this->connection) {
            throw std::invalid_argument("redis_client requires a connection");
        }
    }

    bool redis_client::exists(const std::string &key) {
        const auto reply = connection->exists(key).or_throw();
        return reply.as_integer("EXISTS") == 1;
    }

    void redis_client::keys(const std::string &pattern, std::unordered_set<std::string> &keys) {
        const auto values = connection->keys(pattern).or_throw().strings("KEYS");
        keys.insert(values.begin(), values.end());
    }

    string_scan_result redis_client::scan(uint64_t cursor, const std::string &pattern, unsigned int count) {
        const auto reply = connection->scan(cursor, pattern, count).or_throw();
        string_scan_result result;
        result.cursor = reply.scan_cursor("SCAN");
        const auto values = reply.as_array("SCAN")[1].strings("SCAN");
        result.keys.insert(values.begin(), values.end());
        return result;
    }

    std::string redis_client::type(const std::string &key) {
        const auto reply = connection->type(key).or_throw();
        return reply.as_status("TYPE");
    }

    bool redis_client::expire(const std::string &key, long long seconds) {
        const auto reply = connection->expire(key, seconds).or_throw();
        return reply.as_integer("EXPIRE") == 1;
    }

    bool redis_client::pexpire(const std::string &key, long long milliseconds) {
        const auto reply = connection->pexpire(key, milliseconds).or_throw();
        return reply.as_integer("PEXPIRE") == 1;
    }

    int64_t redis_client::ttl(const std::string &key) {
        return connection->ttl(key).or_throw().as_integer("TTL");
    }

    int64_t redis_client::pttl(const std::string &key) {
        return connection->pttl(key).or_throw().as_integer("PTTL");
    }

    bool redis_client::persist(const std::string &key) {
        return connection->persist(key).or_throw().as_integer("PERSIST") == 1;
    }

    std::string redis_client::ping() {
        const auto reply = connection->ping().or_throw();
        return reply.as_text("PING");
    }

    std::string redis_client::ping(const std::string &message) {
        const auto reply = connection->ping(message).or_throw();
        return reply.as_text("PING");
    }

    long long redis_client::del(const std::string &key) {
        return connection->del(key).or_throw().as_integer("DEL");
    }

    long long redis_client::del(const std::vector<std::string> &keys) {
        if (keys.empty()) {
            return 0;
        }
        return connection->del(keys).or_throw().as_integer("DEL");
    }

    bool redis_client::set(const std::string &key, const std::string &value) {
        const auto reply = connection->set(key, value).or_throw();
        return reply.get_type() == reply_type::STATUS && reply.status_is_ok("SET");
    }

    bool redis_client::set_not_exists(const std::string &key, const std::string &value) {
        const auto reply = connection->set_not_exists(key, value).or_throw();
        return reply.get_type() == reply_type::STATUS && reply.status_is_ok("SET");
    }

    bool redis_client::set_ex(const std::string &key, const std::string &value, long long seconds) {
        const auto reply = connection->set_ex(key, value, seconds).or_throw();
        return reply.get_type() == reply_type::STATUS && reply.status_is_ok("SET");
    }

    bool redis_client::set_px(const std::string &key, const std::string &value, long long milliseconds) {
        const auto reply = connection->set_px(key, value, milliseconds).or_throw();
        return reply.get_type() == reply_type::STATUS && reply.status_is_ok("SET");
    }

    std::optional<std::string> redis_client::get(const std::string &key) {
        return connection->get(key).or_throw().optional_string("GET");
    }

    std::optional<std::string> redis_client::getset(const std::string &key, const std::string &new_value) {
        return connection->getset(key, new_value).or_throw().optional_string("GETSET");
    }

    long long redis_client::incr(const std::string &key, long long delta) {
        return connection->incr(key, delta).or_throw().as_integer("INCRBY");
    }

    long long redis_client::decr(const std::string &key, long long delta) {
        return connection->decr(key, delta).or_throw().as_integer("DECRBY");
    }

    long long redis_client::append(const std::string &key, const std::string &value) {
        return connection->append(key, value).or_throw().as_integer("APPEND");
    }

    std::optional<std::string> redis_client::hget(const std::string &key, const std::string &hash_key) {
        return connection->hget(key, hash_key).or_throw().optional_string("HGET");
    }

    void redis_client::hget(const std::string &key,
                            std::unordered_map<std::string, std::optional<std::string>> &hash_map) {
        if (hash_map.empty()) {
            return;
        }
        std::vector<std::string> fields;
        fields.reserve(hash_map.size());
        for (const auto &field: hash_map) {
            fields.push_back(field.first);
        }
        const auto reply = connection->hget(key, fields);
        reply.throw_if_error();
        const auto &values = reply.as_array("HMGET");
        if (values.size() != hash_map.size()) {
            throw std::runtime_error("HMGET: unexpected value count");
        }
        // Decode before modifying the caller's map, including on malformed replies.
        std::vector<std::optional<std::string>> decoded;
        decoded.reserve(values.size());
        for (const auto &value: values) {
            decoded.push_back(value.optional_string("HMGET"));
        }
        for (size_t i = 0; i < decoded.size(); ++i) {
            hash_map.at(fields[i]) = std::move(decoded[i]);
        }
    }

    bool redis_client::hset(const std::string &key, const std::string &field, const std::string &value) {
        const auto reply = connection->hset(key, field, value).or_throw();
        return reply.get_type() == reply_type::INTEGER && reply.as_integer("HSET") >= 0;
    }

    bool redis_client::hset(const std::string &key, const std::unordered_map<std::string, std::string> &hash_map) {
        if (hash_map.empty()) {
            return false;
        }
        const auto reply = connection->hset(key, hash_map).or_throw();
        return reply.get_type() == reply_type::INTEGER && reply.as_integer("HSET") >= 0;
    }

    std::unordered_map<std::string, std::string> redis_client::hgetall(const std::string &key) {
        return connection->hgetall(key).or_throw().hash("HGETALL");
    }

    std::vector<std::string> redis_client::hkeys(const std::string &key) {
        return connection->hkeys(key).or_throw().strings("HKEYS");
    }

    std::vector<std::string> redis_client::hvals(const std::string &key) {
        return connection->hvals(key).or_throw().strings("HVALS");
    }

    uint64_t redis_client::hscan(const std::string &key, uint64_t cursor, const std::string &pattern,
                                 unsigned int count, std::unordered_map<std::string, std::string> &hash_map) {
        const auto reply = connection->hscan(key, cursor, pattern, count).or_throw();
        const auto next = reply.scan_cursor("HSCAN");
        const auto fields = reply.as_array("HSCAN")[1].hash("HSCAN");
        hash_map.insert(fields.begin(), fields.end());
        return next;
    }

    long long redis_client::hdel(const std::string &key, const std::string &hash_key) {
        return connection->hdel(key, hash_key).or_throw().as_integer("HDEL");
    }

    long long redis_client::hdel(const std::string &key, const std::vector<std::string> &hash_keys) {
        if (hash_keys.empty()) {
            return 0;
        }
        return connection->hdel(key, hash_keys).or_throw().as_integer("HDEL");
    }

    long long redis_client::lpush(const std::string &key, const std::vector<std::string> &values) {
        if (values.empty()) {
            return llen(key);
        }
        return connection->lpush(key, values).or_throw().as_integer("LPUSH");
    }

    long long redis_client::lpush(const std::string &key, const std::string &value) {
        return connection->lpush(key, value).or_throw().as_integer("LPUSH");
    }

    long long redis_client::rpush(const std::string &key, const std::string &value) {
        return connection->rpush(key, value).or_throw().as_integer("RPUSH");
    }

    long long redis_client::rpush(const std::string &key, const std::vector<std::string> &values) {
        if (values.empty()) {
            return llen(key);
        }
        return connection->rpush(key, values).or_throw().as_integer("RPUSH");
    }

    std::optional<std::string> redis_client::lpop(const std::string &key) {
        return connection->lpop(key).or_throw().optional_string("LPOP");
    }

    std::optional<std::string> redis_client::rpop(const std::string &key) {
        return connection->rpop(key).or_throw().optional_string("RPOP");
    }

    std::vector<std::string> redis_client::lpop(const std::string &key, int count) {
        return connection->lpop(key, count).or_throw().strings("LPOP");
    }

    std::vector<std::string> redis_client::rpop(const std::string &key, int count) {
        return connection->rpop(key, count).or_throw().strings("RPOP");
    }

    std::vector<std::string> redis_client::lrange(const std::string &key, long long start, long long stop) {
        return connection->lrange(key, start, stop).or_throw().strings("LRANGE");
    }

    long long redis_client::llen(const std::string &key) {
        return connection->llen(key).or_throw().as_integer("LLEN");
    }

    std::optional<std::string> redis_client::lindex(const std::string &key, long long index) {
        return connection->lindex(key, index).or_throw().optional_string("LINDEX");
    }

    long long redis_client::sadd(const std::string &key, const std::vector<std::string> &members) {
        if (members.empty()) {
            return 0;
        }
        return connection->sadd(key, members).or_throw().as_integer("SADD");
    }

    long long redis_client::srem(const std::string &key, const std::vector<std::string> &members) {
        if (members.empty()) {
            return 0;
        }
        return connection->srem(key, members).or_throw().as_integer("SREM");
    }

    std::vector<std::string> redis_client::smembers(const std::string &key) {
        return connection->smembers(key).or_throw().strings("SMEMBERS");
    }

    long long redis_client::scard(const std::string &key) {
        return connection->scard(key).or_throw().as_integer("SCARD");
    }

    bool redis_client::sismember(const std::string &key, const std::string &member) {
        return connection->sismember(key, member).or_throw().as_integer("SISMEMBER") == 1;
    }

    std::optional<std::string> redis_client::spop(const std::string &key) {
        return connection->spop(key).or_throw().optional_string("SPOP");
    }

    std::vector<std::string> redis_client::sinter(const std::vector<std::string> &keys) {
        if (keys.empty()) {
            return {};
        }
        return connection->sinter(keys).or_throw().strings("SINTER");
    }

    long long redis_client::zadd(const std::string &key, const std::unordered_map<std::string, double> &members) {
        if (members.empty()) {
            return 0;
        }
        return connection->zadd(key, members).or_throw().as_integer("ZADD");
    }

    long long redis_client::zrem(const std::string &key, const std::vector<std::string> &members) {
        if (members.empty()) {
            return 0;
        }
        return connection->zrem(key, members).or_throw().as_integer("ZREM");
    }

    std::optional<double> redis_client::zscore(const std::string &key, const std::string &member) {
        return connection->zscore(key, member).or_throw().optional_score("ZSCORE");
    }

    std::vector<std::string> redis_client::zrange(const std::string &key, long long start, long long stop) {
        return connection->zrange(key, start, stop).or_throw().strings("ZRANGE");
    }

    std::vector<std::string> redis_client::zrevrange(const std::string &key, long long start, long long stop) {
        return connection->zrevrange(key, start, stop).or_throw().strings("ZREVRANGE");
    }

    std::vector<std::pair<std::string, double>> redis_client::zrange_withscores(const std::string &key, long long start,
                                                                                long long stop) {
        return connection->zrange_withscores(key, start, stop).or_throw().scores("ZRANGE");
    }

    std::vector<std::pair<std::string, double>> redis_client::zrevrange_withscores(const std::string &key,
                                                                                   long long start, long long stop) {
        return connection->zrevrange_withscores(key, start, stop).or_throw().scores("ZREVRANGE");
    }

    double redis_client::zincrby(const std::string &key, double increment, const std::string &member) {
        const auto reply = connection->zincrby(key, increment, member).or_throw();
        const auto score = reply.optional_score("ZINCRBY");
        if (!score) {
            throw unexpected_reply_type_error("ZINCRBY", "string", "nil");
        }
        return *score;
    }

    std::optional<std::string> redis_client::xadd(const std::string &key,
                                                  const std::vector<std::pair<std::string, std::string>> &fields,
                                                  const stream_add_options &options) {
        return connection->xadd(key, fields, options).or_throw().optional_string("XADD");
    }

    long long redis_client::xlen(const std::string &key) {
        return connection->xlen(key).or_throw().as_integer("XLEN");
    }

    std::vector<string_stream_entry> redis_client::xrange(const std::string &key, const std::string &start,
                                                          const std::string &end, std::optional<long long> count) {
        return connection->xrange(key, start, end, count).or_throw().stream_entries("XRANGE");
    }

    std::vector<string_stream_entry> redis_client::xrevrange(const std::string &key, const std::string &end,
                                                             const std::string &start, std::optional<long long> count) {
        return connection->xrevrange(key, end, start, count).or_throw().stream_entries("XREVRANGE");
    }

    std::vector<string_stream_batch> redis_client::xread(const std::vector<string_stream_read_request> &streams,
                                                         const stream_read_options &options) {
        return connection->xread(streams, options).or_throw().stream_batches("XREAD");
    }

    long long redis_client::xdel(const std::string &key, const std::vector<std::string> &ids) {
        if (ids.empty()) {
            return 0;
        }
        return connection->xdel(key, ids).or_throw().as_integer("XDEL");
    }

    long long redis_client::xtrim(const std::string &key, const stream_trim_options &options) {
        return connection->xtrim(key, options).or_throw().as_integer("XTRIM");
    }

    bool redis_client::xgroup_create(const std::string &key, const std::string &group, const std::string &id,
                                     bool mkstream) {
        return connection->xgroup_create(key, group, id, mkstream).or_throw().status_is_ok("XGROUP CREATE");
    }

    bool redis_client::xgroup_setid(const std::string &key, const std::string &group, const std::string &id) {
        return connection->xgroup_setid(key, group, id).or_throw().status_is_ok("XGROUP SETID");
    }

    bool redis_client::xgroup_destroy(const std::string &key, const std::string &group) {
        return connection->xgroup_destroy(key, group).or_throw().as_integer("XGROUP DESTROY") == 1;
    }

    bool redis_client::xgroup_createconsumer(const std::string &key, const std::string &group,
                                             const std::string &consumer) {
        return connection->xgroup_createconsumer(key, group, consumer).or_throw().as_integer("XGROUP CREATECONSUMER")
               == 1;
    }

    long long redis_client::xgroup_delconsumer(const std::string &key, const std::string &group,
                                               const std::string &consumer) {
        return connection->xgroup_delconsumer(key, group, consumer).or_throw().as_integer("XGROUP DELCONSUMER");
    }

    std::vector<string_stream_batch> redis_client::xreadgroup(const std::string &group, const std::string &consumer,
                                                              const std::vector<string_stream_read_request> &streams,
                                                              const stream_read_group_options &options) {
        return connection->xreadgroup(group, consumer, streams, options).or_throw().stream_batches("XREADGROUP");
    }

    long long redis_client::xack(const std::string &key, const std::string &group,
                                 const std::vector<std::string> &ids) {
        if (ids.empty()) {
            return 0;
        }
        return connection->xack(key, group, ids).or_throw().as_integer("XACK");
    }

    std::string redis_client::script_load(const std::string &script) {
        return connection->script_load(script);
    }

    cmd_reply redis_client::eval_sha1(const std::string &sha1, const std::vector<std::string> &keys,
                                      const std::vector<std::string> &args) {
        auto reply = connection->eval_sha1(sha1, keys, args);
        reply.throw_if_error();
        return reply;
    }

    cmd_reply redis_client::eval(const std::string &script, const std::vector<std::string> &keys,
                                 const std::vector<std::string> &args) {
        return connection->eval(script, keys, args).or_throw();
    }

    cmd_reply redis_client::command(const std::string &cmd, const std::vector<std::string> &args) {
        return connection->command(cmd, args).or_throw();
    }
} // namespace redisdal
