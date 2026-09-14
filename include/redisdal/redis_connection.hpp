#pragma once

#include "redis_cmd_ops.hpp"

namespace redisdal {
    /**
     * Command-capable Redis connection contract.
     * Concrete synchronous and future Pipeline connections implement the
     * inherited execution primitives and expose their underlying descriptor.
     */
    class redis_connection: public redis_cmd_ops {
    public:
        ~redis_connection() override = default;

        [[nodiscard]] virtual int get_fd() const = 0;
    };
} // namespace redisdal
