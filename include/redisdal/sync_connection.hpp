#pragma once

#include <cstdarg>
#include <cstddef>
#include <string>
#include <vector>

#include "hiredis_types.hpp"
#include "redis_connection.hpp"
#include "redis_executor.hpp"

namespace redisdal {
    /**
     * Synchronous hiredis implementation of the command execution contract.
     *
     * This class owns only connection state and command transport. Command
     * construction and typed result handling live above this layer.
     */
    class sync_connection final: public redis_connection {
    public:
        explicit sync_connection(const std::string &url);

        [[nodiscard]] int get_fd() const override;

    protected:
        cmd_reply cmd_exec(const char *format, ...) override;
        cmd_reply cmd_execv(const std::vector<std::string> &args) override;
        redis_reply_ptr exec(const char *format, ...) const;
        [[nodiscard]] redis_reply_ptr exec_args(const std::vector<std::string> &args) const;
        [[nodiscard]] redis_reply_ptr execv(const std::vector<const char *> &argv,
                                            const std::vector<size_t> &argv_len) const;

    private:
        [[nodiscard]] redis_reply_ptr exec_format(const char *format, va_list args) const;

        redis_context_ptr context;
    };
} // namespace redisdal
