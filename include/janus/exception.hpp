#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace janus {

    /**
     * Exception thrown when a script is invoked with more keys than allowed.
     * Message contains the provided count and the configured limit.
     */
    class too_many_script_keys_error: public std::runtime_error {
    public:
        explicit too_many_script_keys_error(const std::string &cmd, size_t provided, size_t limit) :
            std::runtime_error(cmd + ": too many script keys: provided=" + std::to_string(provided)
                               + " limit=" + std::to_string(limit)) {
        }
    };

    class unexpected_reply_type_error: public std::runtime_error {
    public:
        explicit unexpected_reply_type_error(const std::string &cmd, const std::string &expected,
                                             const std::string &actual) :
            std::runtime_error(cmd + ": unexpected reply type: expected=" + expected + " actual=" + actual) {
        }
    };

    /**
     * Exception thrown when `EVALSHA` is called but the script is not loaded on the server.
     * Message contains the command and the SHA1 of the missing script.
     */
    class no_script_error: public std::runtime_error {
    public:
        explicit no_script_error(const std::string &cmd, const std::string &sha1) :
            std::runtime_error(cmd + ": script not loaded: " + sha1) {
        }
    };

    /**
     * Generic Redis error wrapper.
     * Stores the original Redis error message prefixed with "Redis error: ".
     */
    class redis_error: public std::runtime_error {
    public:
        explicit redis_error(const std::string &msg) : std::runtime_error(std::string("Redis error: ") + msg) {
        }
    };

} // namespace janus
