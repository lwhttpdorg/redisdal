#include "redisdal/redis_cmd_ops.hpp"

#include <stdexcept>

#include "redisdal/exception.hpp"
#include "redisdal/serialization.hpp"

namespace redisdal {
    namespace {
        std::vector<std::string> append_arguments(std::vector<std::string> prefix,
                                                  const std::vector<std::string> &args) {
            prefix.insert(prefix.end(), args.begin(), args.end());
            return prefix;
        }

        std::vector<std::string> script_arguments(const std::string &command, const std::string &script,
                                                  const std::vector<std::string> &keys,
                                                  const std::vector<std::string> &args) {
            if (keys.size() > MAX_SCRIPT_KEYS) {
                throw too_many_script_keys_error(command, keys.size(), MAX_SCRIPT_KEYS);
            }
            auto result = append_arguments({command, script, std::to_string(keys.size())}, keys);
            result.insert(result.end(), args.begin(), args.end());
            return result;
        }

        void append_stream_count(std::vector<std::string> &args, std::optional<long long> count,
                                 const std::string &command) {
            if (!count) {
                return;
            }
            if (*count <= 0) {
                throw std::invalid_argument(command + ": count must be greater than zero");
            }
            args.emplace_back("COUNT");
            args.push_back(std::to_string(*count));
        }

        void append_stream_block(std::vector<std::string> &args, std::optional<long long> block_ms,
                                 const std::string &command) {
            if (!block_ms) {
                return;
            }
            if (*block_ms < 0) {
                throw std::invalid_argument(command + ": block_ms must not be negative");
            }
            args.emplace_back("BLOCK");
            args.push_back(std::to_string(*block_ms));
        }

        void append_stream_trim(std::vector<std::string> &args, const stream_trim_options &options,
                                const std::string &command) {
            if (options.threshold.empty()) {
                throw std::invalid_argument(command + ": trim threshold must not be empty");
            }
            if (options.limit && *options.limit < 0) {
                throw std::invalid_argument(command + ": trim limit must not be negative");
            }
            if (options.limit && !options.approximate) {
                throw std::invalid_argument(command + ": trim limit requires approximate trimming");
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

        void append_streams(std::vector<std::string> &args, const std::vector<string_stream_read_request> &streams,
                            const std::string &command) {
            if (streams.empty()) {
                throw std::invalid_argument(command + ": streams must not be empty");
            }
            for (const auto &stream: streams) {
                if (stream.id.empty()) {
                    throw std::invalid_argument(command + ": stream id must not be empty");
                }
            }
            args.emplace_back("STREAMS");
            for (const auto &stream: streams) {
                args.push_back(stream.key);
            }
            for (const auto &stream: streams) {
                args.push_back(stream.id);
            }
        }
    } // namespace

    std::string redis_cmd_ops::script_load(const std::string &script) {
        const auto reply = cmd_execv({"SCRIPT", "LOAD", script});
        if (reply.is_error()) {
            throw redis_error(reply.get_string().value_or("Unknown Redis error"));
        }
        if (reply.get_type() != reply_type::STRING || !reply.get_string()) {
            throw unexpected_reply_type_error("SCRIPT LOAD", "string", "non-string");
        }
        return *reply.get_string();
    }

    cmd_reply redis_cmd_ops::eval_sha1(const std::string &sha1, const std::vector<std::string> &keys,
                                       const std::vector<std::string> &args) {
        auto reply = cmd_execv(script_arguments("EVALSHA", sha1, keys, args));
        if (reply.is_error() && reply.get_string().value_or("").find("NOSCRIPT") != std::string::npos) {
            throw no_script_error("EVALSHA", sha1);
        }
        return reply;
    }

    cmd_reply redis_cmd_ops::eval(const std::string &script, const std::vector<std::string> &keys,
                                  const std::vector<std::string> &args) {
        return cmd_execv(script_arguments("EVAL", script, keys, args));
    }

    cmd_reply redis_cmd_ops::command(const std::string &cmd, const std::vector<std::string> &args) {
        return cmd_execv(append_arguments({cmd}, args));
    }

    cmd_reply redis_cmd_ops::exists(const std::string &key) {
        return cmd_execv({"EXISTS", key});
    }

    cmd_reply redis_cmd_ops::keys(const std::string &pattern) {
        return cmd_execv({"KEYS", pattern});
    }

    cmd_reply redis_cmd_ops::scan(uint64_t cursor, const std::string &pattern, unsigned int count) {
        return cmd_execv({"SCAN", std::to_string(cursor), "MATCH", pattern, "COUNT", std::to_string(count)});
    }

    cmd_reply redis_cmd_ops::type(const std::string &key) {
        return cmd_execv({"TYPE", key});
    }

    cmd_reply redis_cmd_ops::set(const std::string &key, const std::string &value) {
        return cmd_execv({"SET", key, value});
    }

    cmd_reply redis_cmd_ops::set_not_exists(const std::string &key, const std::string &value) {
        return cmd_execv({"SET", key, value, "NX"});
    }

    cmd_reply redis_cmd_ops::set_ex(const std::string &key, const std::string &value, long long seconds) {
        return cmd_execv({"SET", key, value, "EX", std::to_string(seconds)});
    }

    cmd_reply redis_cmd_ops::set_px(const std::string &key, const std::string &value, long long milliseconds) {
        return cmd_execv({"SET", key, value, "PX", std::to_string(milliseconds)});
    }

    cmd_reply redis_cmd_ops::get(const std::string &key) {
        return cmd_execv({"GET", key});
    }

    cmd_reply redis_cmd_ops::getset(const std::string &key, const std::string &new_value) {
        return cmd_execv({"GETSET", key, new_value});
    }

    cmd_reply redis_cmd_ops::append(const std::string &key, const std::string &value) {
        return cmd_execv({"APPEND", key, value});
    }

    cmd_reply redis_cmd_ops::incr(const std::string &key, int64_t delta) {
        return cmd_execv({"INCRBY", key, std::to_string(delta)});
    }

    cmd_reply redis_cmd_ops::decr(const std::string &key, int64_t delta) {
        return cmd_execv({"DECRBY", key, std::to_string(delta)});
    }

    cmd_reply redis_cmd_ops::hget(const std::string &key, const std::string &field) {
        return cmd_execv({"HGET", key, field});
    }

    cmd_reply redis_cmd_ops::hget(const std::string &key, const std::vector<std::string> &fields) {
        return cmd_execv(append_arguments({"HMGET", key}, fields));
    }

    cmd_reply redis_cmd_ops::hset(const std::string &key, const std::string &field, const std::string &value) {
        return cmd_execv({"HSET", key, field, value});
    }

    cmd_reply redis_cmd_ops::hset(const std::string &key, const std::unordered_map<std::string, std::string> &values) {
        if (values.empty()) {
            return {};
        }
        auto args = std::vector<std::string>{"HSET", key};
        for (const auto &value: values) {
            args.push_back(value.first);
            args.push_back(value.second);
        }
        return cmd_execv(args);
    }

    cmd_reply redis_cmd_ops::hgetall(const std::string &key) {
        return cmd_execv({"HGETALL", key});
    }

    cmd_reply redis_cmd_ops::hkeys(const std::string &key) {
        return cmd_execv({"HKEYS", key});
    }

    cmd_reply redis_cmd_ops::hvals(const std::string &key) {
        return cmd_execv({"HVALS", key});
    }

    cmd_reply redis_cmd_ops::hscan(const std::string &key, uint64_t cursor, const std::string &pattern,
                                   unsigned int count) {
        return cmd_execv({"HSCAN", key, std::to_string(cursor), "MATCH", pattern, "COUNT", std::to_string(count)});
    }

    cmd_reply redis_cmd_ops::hdel(const std::string &key, const std::string &field) {
        return cmd_execv({"HDEL", key, field});
    }

    cmd_reply redis_cmd_ops::hdel(const std::string &key, const std::vector<std::string> &fields) {
        if (fields.empty()) {
            return {};
        }
        return cmd_execv(append_arguments({"HDEL", key}, fields));
    }

    cmd_reply redis_cmd_ops::lpush(const std::string &key, const std::string &value) {
        return cmd_execv({"LPUSH", key, value});
    }

    cmd_reply redis_cmd_ops::lpush(const std::string &key, const std::vector<std::string> &values) {
        if (values.empty()) {
            return {};
        }
        return cmd_execv(append_arguments({"LPUSH", key}, values));
    }

    cmd_reply redis_cmd_ops::rpush(const std::string &key, const std::string &value) {
        return cmd_execv({"RPUSH", key, value});
    }

    cmd_reply redis_cmd_ops::rpush(const std::string &key, const std::vector<std::string> &values) {
        if (values.empty()) {
            return {};
        }
        return cmd_execv(append_arguments({"RPUSH", key}, values));
    }

    cmd_reply redis_cmd_ops::lpop(const std::string &key) {
        return cmd_execv({"LPOP", key});
    }

    cmd_reply redis_cmd_ops::lpop(const std::string &key, int count) {
        return cmd_execv({"LPOP", key, std::to_string(count)});
    }

    cmd_reply redis_cmd_ops::rpop(const std::string &key) {
        return cmd_execv({"RPOP", key});
    }

    cmd_reply redis_cmd_ops::rpop(const std::string &key, int count) {
        return cmd_execv({"RPOP", key, std::to_string(count)});
    }

    cmd_reply redis_cmd_ops::lrange(const std::string &key, long long start, long long stop) {
        return cmd_execv({"LRANGE", key, std::to_string(start), std::to_string(stop)});
    }

    cmd_reply redis_cmd_ops::llen(const std::string &key) {
        return cmd_execv({"LLEN", key});
    }

    cmd_reply redis_cmd_ops::lindex(const std::string &key, long long index) {
        return cmd_execv({"LINDEX", key, std::to_string(index)});
    }

    cmd_reply redis_cmd_ops::del(const std::string &key) {
        return cmd_execv({"DEL", key});
    }

    cmd_reply redis_cmd_ops::del(const std::vector<std::string> &keys) {
        return cmd_execv(append_arguments({"DEL"}, keys));
    }

    cmd_reply redis_cmd_ops::expire(const std::string &key, long long seconds) {
        return cmd_execv({"EXPIRE", key, std::to_string(seconds)});
    }

    cmd_reply redis_cmd_ops::pexpire(const std::string &key, long long milliseconds) {
        return cmd_execv({"PEXPIRE", key, std::to_string(milliseconds)});
    }

    cmd_reply redis_cmd_ops::ttl(const std::string &key) {
        return cmd_execv({"TTL", key});
    }

    cmd_reply redis_cmd_ops::pttl(const std::string &key) {
        return cmd_execv({"PTTL", key});
    }

    cmd_reply redis_cmd_ops::persist(const std::string &key) {
        return cmd_execv({"PERSIST", key});
    }

    cmd_reply redis_cmd_ops::ping() {
        return cmd_execv({"PING"});
    }

    cmd_reply redis_cmd_ops::ping(const std::string &message) {
        return cmd_execv({"PING", message});
    }

    cmd_reply redis_cmd_ops::sadd(const std::string &key, const std::vector<std::string> &members) {
        return cmd_execv(append_arguments({"SADD", key}, members));
    }

    cmd_reply redis_cmd_ops::srem(const std::string &key, const std::vector<std::string> &members) {
        return cmd_execv(append_arguments({"SREM", key}, members));
    }

    cmd_reply redis_cmd_ops::smembers(const std::string &key) {
        return cmd_execv({"SMEMBERS", key});
    }

    cmd_reply redis_cmd_ops::scard(const std::string &key) {
        return cmd_execv({"SCARD", key});
    }

    cmd_reply redis_cmd_ops::sismember(const std::string &key, const std::string &member) {
        return cmd_execv({"SISMEMBER", key, member});
    }

    cmd_reply redis_cmd_ops::spop(const std::string &key) {
        return cmd_execv({"SPOP", key});
    }

    cmd_reply redis_cmd_ops::sinter(const std::vector<std::string> &keys) {
        return cmd_execv(append_arguments({"SINTER"}, keys));
    }

    cmd_reply redis_cmd_ops::zadd(const std::string &key, const std::unordered_map<std::string, double> &members) {
        std::vector<std::string> args{"ZADD", key};
        for (const auto &member: members) {
            args.push_back(string_serializable<double>::to_string(member.second));
            args.push_back(member.first);
        }
        return cmd_execv(args);
    }

    cmd_reply redis_cmd_ops::zrem(const std::string &key, const std::vector<std::string> &members) {
        return cmd_execv(append_arguments({"ZREM", key}, members));
    }

    cmd_reply redis_cmd_ops::zscore(const std::string &key, const std::string &member) {
        return cmd_execv({"ZSCORE", key, member});
    }

    cmd_reply redis_cmd_ops::zrange(const std::string &key, long long start, long long stop) {
        return cmd_execv({"ZRANGE", key, std::to_string(start), std::to_string(stop)});
    }

    cmd_reply redis_cmd_ops::zrevrange(const std::string &key, long long start, long long stop) {
        return cmd_execv({"ZREVRANGE", key, std::to_string(start), std::to_string(stop)});
    }

    cmd_reply redis_cmd_ops::zrange_withscores(const std::string &key, long long start, long long stop) {
        return cmd_execv({"ZRANGE", key, std::to_string(start), std::to_string(stop), "WITHSCORES"});
    }

    cmd_reply redis_cmd_ops::zrevrange_withscores(const std::string &key, long long start, long long stop) {
        return cmd_execv({"ZREVRANGE", key, std::to_string(start), std::to_string(stop), "WITHSCORES"});
    }

    cmd_reply redis_cmd_ops::zincrby(const std::string &key, double increment, const std::string &member) {
        return cmd_execv({"ZINCRBY", key, string_serializable<double>::to_string(increment), member});
    }

    cmd_reply redis_cmd_ops::xadd(const std::string &key,
                                  const std::vector<std::pair<std::string, std::string>> &fields,
                                  const stream_add_options &options) {
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
            append_stream_trim(args, *options.trim, "XADD");
        }
        args.push_back(options.id);
        for (const auto &field: fields) {
            args.push_back(field.first);
            args.push_back(field.second);
        }
        return cmd_execv(args);
    }

    cmd_reply redis_cmd_ops::xlen(const std::string &key) {
        return cmd_execv({"XLEN", key});
    }

    cmd_reply redis_cmd_ops::xrange(const std::string &key, const std::string &start, const std::string &end,
                                    std::optional<long long> count) {
        std::vector<std::string> args{"XRANGE", key, start, end};
        append_stream_count(args, count, "XRANGE");
        return cmd_execv(args);
    }

    cmd_reply redis_cmd_ops::xrevrange(const std::string &key, const std::string &end, const std::string &start,
                                       std::optional<long long> count) {
        std::vector<std::string> args{"XREVRANGE", key, end, start};
        append_stream_count(args, count, "XREVRANGE");
        return cmd_execv(args);
    }

    cmd_reply redis_cmd_ops::xread(const std::vector<string_stream_read_request> &streams,
                                   const stream_read_options &options) {
        std::vector<std::string> args{"XREAD"};
        append_stream_count(args, options.count, "XREAD");
        append_stream_block(args, options.block_ms, "XREAD");
        append_streams(args, streams, "XREAD");
        return cmd_execv(args);
    }

    cmd_reply redis_cmd_ops::xdel(const std::string &key, const std::vector<std::string> &ids) {
        if (ids.empty()) {
            return cmd_reply::make_signed_integer(0);
        }
        return cmd_execv(append_arguments({"XDEL", key}, ids));
    }

    cmd_reply redis_cmd_ops::xtrim(const std::string &key, const stream_trim_options &options) {
        std::vector<std::string> args{"XTRIM", key};
        append_stream_trim(args, options, "XTRIM");
        return cmd_execv(args);
    }

    cmd_reply redis_cmd_ops::xgroup_create(const std::string &key, const std::string &group, const std::string &id,
                                           bool mkstream) {
        std::vector<std::string> args{"XGROUP", "CREATE", key, group, id};
        if (mkstream) {
            args.emplace_back("MKSTREAM");
        }
        return cmd_execv(args);
    }

    cmd_reply redis_cmd_ops::xgroup_setid(const std::string &key, const std::string &group, const std::string &id) {
        return cmd_execv({"XGROUP", "SETID", key, group, id});
    }

    cmd_reply redis_cmd_ops::xgroup_destroy(const std::string &key, const std::string &group) {
        return cmd_execv({"XGROUP", "DESTROY", key, group});
    }

    cmd_reply redis_cmd_ops::xgroup_createconsumer(const std::string &key, const std::string &group,
                                                   const std::string &consumer) {
        return cmd_execv({"XGROUP", "CREATECONSUMER", key, group, consumer});
    }

    cmd_reply redis_cmd_ops::xgroup_delconsumer(const std::string &key, const std::string &group,
                                                const std::string &consumer) {
        return cmd_execv({"XGROUP", "DELCONSUMER", key, group, consumer});
    }

    cmd_reply redis_cmd_ops::xreadgroup(const std::string &group, const std::string &consumer,
                                        const std::vector<string_stream_read_request> &streams,
                                        const stream_read_group_options &options) {
        std::vector<std::string> args{"XREADGROUP", "GROUP", group, consumer};
        append_stream_count(args, options.count, "XREADGROUP");
        append_stream_block(args, options.block_ms, "XREADGROUP");
        if (options.noack) {
            args.emplace_back("NOACK");
        }
        append_streams(args, streams, "XREADGROUP");
        return cmd_execv(args);
    }

    cmd_reply redis_cmd_ops::xack(const std::string &key, const std::string &group,
                                  const std::vector<std::string> &ids) {
        if (ids.empty()) {
            return cmd_reply::make_signed_integer(0);
        }
        return cmd_execv(append_arguments({"XACK", key, group}, ids));
    }
} // namespace redisdal
