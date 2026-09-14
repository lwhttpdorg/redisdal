#pragma once

#include <string>
#include <vector>

#include "common.hpp"

namespace redisdal {
    /**
     * Primitive command execution boundary used by redis_cmd_ops.
     *
     * Synchronous connections return the command reply directly. Implementations of
     * redis_async_executor may queue the command and expose replies through
     * fetch_reply(). Transport failures throw and arguments are borrowed only for
     * the duration of each call.
     */
    class redis_executor {
    public:
        virtual ~redis_executor() = default;

    protected:
        /**
         * Submit one Redis command.
         *
         * Redis server ERROR replies remain cmd_reply::ERROR values. Transport
         * and protocol failures throw. Arguments are borrowed only for the call.
         */
        virtual cmd_reply cmd_exec(const char *format, ...) = 0;
        virtual cmd_reply cmd_execv(const std::vector<std::string> &args) = 0;
    };

    // Virtual inheritance keeps a future redis_pipeline, which also inherits
    // redis_connection -> redis_cmd_ops, convertible to one redis_executor base.
    class redis_async_executor: public virtual redis_executor {
    public:
        ~redis_async_executor() override = default;
        virtual std::vector<cmd_reply> fetch_reply() = 0;
    };
} // namespace redisdal
