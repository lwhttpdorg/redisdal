#pragma once

#include <hiredis/hiredis.h>
#include <memory>

// Legacy hiredis ownership aliases, kept for source compatibility.
namespace redisdal {
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

} // namespace redisdal
