#include "redisdal/sync_connection.hpp"

#include <cstdarg>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "redisdal/exception.hpp"
#include "redisdal/redis_config.hpp"

namespace redisdal {
    namespace {
        cmd_reply convert_reply(const redisReply &reply) {
            if ((reply.type == REDIS_REPLY_STRING || reply.type == REDIS_REPLY_STATUS
                 || reply.type == REDIS_REPLY_ERROR)
                && !reply.str && reply.len != 0) {
                throw std::runtime_error("Malformed Redis text reply");
            }
            switch (reply.type) {
                case REDIS_REPLY_STRING:
                    return cmd_reply::make_string(std::string(reply.str ? reply.str : "", reply.len));
                case REDIS_REPLY_STATUS:
                    return cmd_reply::make_status(std::string(reply.str ? reply.str : "", reply.len));
                case REDIS_REPLY_ERROR:
                    return cmd_reply::make_error(std::string(reply.str ? reply.str : "", reply.len));
                case REDIS_REPLY_INTEGER:
                    return cmd_reply::make_signed_integer(reply.integer);
                case REDIS_REPLY_NIL:
                    return cmd_reply::make_nil();
                case REDIS_REPLY_ARRAY: {
                    if (!reply.element && reply.elements != 0) {
                        throw std::runtime_error("Malformed Redis array reply");
                    }
                    std::vector<cmd_reply> elements;
                    elements.reserve(reply.elements);
                    for (size_t i = 0; i < reply.elements; ++i) {
                        if (!reply.element[i]) {
                            throw std::runtime_error("Null Redis array element");
                        }
                        elements.push_back(convert_reply(*reply.element[i]));
                    }
                    return cmd_reply::make_array(std::move(elements));
                }
                default:
                    return cmd_reply::make_error("Unknown reply type");
            }
        }

        void validate_ok_reply(const cmd_reply &reply, const std::string &command) {
            if (reply.is_error()) {
                throw redis_error(reply.get_string().value_or("Unknown Redis error"));
            }
            if (reply.get_type() != reply_type::STATUS || reply.get_string() != "OK") {
                throw std::runtime_error(command + ": expected OK");
            }
        }
    } // namespace

    sync_connection::sync_connection(const std::string &url) {
        const auto config = parse_redis_url(url);
        switch (config.scheme) {
            case redis_scheme::TCP:
            case redis_scheme::REDIS:
                context.reset(redisConnect(config.host.c_str(), config.port));
                break;
            case redis_scheme::UNIX:
                context.reset(redisConnectUnix(config.host.c_str()));
                break;
        }
        if (!context || context->err) {
            throw std::runtime_error(std::string("Redis connect failed: ")
                                     + (context ? context->errstr : "could not allocate context"));
        }
        if (!config.password.empty()) {
            if (!config.username.empty()) {
                validate_ok_reply(convert_reply(*exec_args({"AUTH", config.username, config.password})), "AUTH");
            }
            else {
                validate_ok_reply(convert_reply(*exec_args({"AUTH", config.password})), "AUTH");
            }
        }
        if (config.index != 0) {
            validate_ok_reply(convert_reply(*exec_args({"SELECT", std::to_string(config.index)})), "SELECT");
        }
    }

    cmd_reply sync_connection::cmd_exec(const char *format, ...) {
        try {
            va_list args;
            va_start(args, format);
            redis_reply_ptr reply;
            try {
                reply = exec_format(format, args);
            }
            catch (...) {
                va_end(args);
                throw;
            }
            va_end(args);
            return convert_reply(*reply);
        }
        catch (const redis_error &error) {
            return cmd_reply::make_error(error.redis_message());
        }
    }

    cmd_reply sync_connection::cmd_execv(const std::vector<std::string> &args) {
        try {
            return convert_reply(*exec_args(args));
        }
        catch (const redis_error &error) {
            return cmd_reply::make_error(error.redis_message());
        }
    }

    int sync_connection::get_fd() const {
        if (!context) {
            throw std::logic_error("Redis connection is not available");
        }
        return context->fd;
    }

    redis_reply_ptr sync_connection::exec_format(const char *format, va_list args) const {
        if (!context) {
            throw std::logic_error("Redis connection is not available");
        }
        redis_reply_ptr reply(static_cast<redisReply *>(redisvCommand(context.get(), format, args)));
        if (!reply) {
            throw std::runtime_error(std::string("Redis command failed: ") + context->errstr);
        }
        if (reply->type == REDIS_REPLY_ERROR) {
            throw redis_error(std::string(reply->str, reply->len));
        }
        return reply;
    }

    redis_reply_ptr sync_connection::exec(const char *format, ...) const {
        va_list args;
        va_start(args, format);
        redis_reply_ptr reply;
        try {
            reply = exec_format(format, args);
        }
        catch (...) {
            va_end(args);
            throw;
        }
        va_end(args);
        return reply;
    }

    redis_reply_ptr sync_connection::exec_args(const std::vector<std::string> &args) const {
        if (args.empty()) {
            throw std::invalid_argument("Redis command must not be empty");
        }
        if (args.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            throw std::length_error("Too many Redis command arguments");
        }
        std::vector<const char *> argv;
        std::vector<size_t> lengths;
        argv.reserve(args.size());
        lengths.reserve(args.size());
        for (const auto &arg: args) {
            argv.push_back(arg.data());
            lengths.push_back(arg.size());
        }
        return execv(argv, lengths);
    }

    redis_reply_ptr sync_connection::execv(const std::vector<const char *> &argv,
                                           const std::vector<size_t> &argv_len) const {
        if (argv.empty() || argv.size() != argv_len.size()) {
            throw std::invalid_argument("Invalid Redis command argument arrays");
        }
        if (argv.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            throw std::length_error("Too many Redis command arguments");
        }
        if (!context) {
            throw std::logic_error("Redis connection is not available");
        }
        redis_reply_ptr reply(static_cast<redisReply *>(redisCommandArgv(
            context.get(), static_cast<int>(argv.size()), const_cast<const char **>(argv.data()), argv_len.data())));
        if (!reply) {
            throw std::runtime_error(std::string("Redis command failed: ") + context->errstr);
        }
        if (reply->type == REDIS_REPLY_ERROR) {
            throw redis_error(std::string(reply->str, reply->len));
        }
        return reply;
    }
} // namespace redisdal
