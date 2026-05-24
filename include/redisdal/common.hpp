#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace redisdal {
    template<typename K>
    class scan_result {
    public:
        uint64_t cursor;
        std::unordered_set<K> keys;
    };

    enum class reply_type : std::uint8_t { STRING, ARRAY, INTEGER, NIL, STATUS, ERROR, DOUBLE, BOOL };

    /**
     * @class cmd_reply
     * @brief A class to encapsulate the reply of a command execution.
     */
    class cmd_reply {
    public:
        cmd_reply() : type(reply_type::NIL) {
        }
        explicit cmd_reply(const std::string &value) : type(reply_type::STRING), str_value(value) {
        }
        explicit cmd_reply(const char *value) : type(reply_type::STRING), str_value(value) {
        }
        explicit cmd_reply(uint64_t value) : type(reply_type::INTEGER), int_value(value) {
        }
        explicit cmd_reply(double value) : type(reply_type::DOUBLE), double_value(value) {
        }
        explicit cmd_reply(bool value) : type(reply_type::BOOL), bool_value(value) {
        }
        explicit cmd_reply(const std::vector<cmd_reply> &value) : type(reply_type::ARRAY), array_value(value) {
        }
        explicit cmd_reply(std::vector<cmd_reply> &&value) : type(reply_type::ARRAY), array_value(std::move(value)) {
        }

        static cmd_reply make_nil() {
            return {};
        }

        static cmd_reply make_error(const std::string &message) {
            cmd_reply r;
            r.type = reply_type::ERROR;
            r.str_value = message;
            return r;
        }
        static cmd_reply make_string(const std::string &value) {
            return cmd_reply(value);
        }
        static cmd_reply make_integer(uint64_t value) {
            return cmd_reply(value);
        }
        static cmd_reply make_double(double value) {
            return cmd_reply(value);
        }
        static cmd_reply make_bool(bool value) {
            return cmd_reply(value);
        }
        static cmd_reply make_array(const std::vector<cmd_reply> &value) {
            return cmd_reply(value);
        }
        static cmd_reply make_array(std::vector<cmd_reply> &&value) {
            return cmd_reply(std::move(value));
        }
        static cmd_reply make_status(const std::string &value) {
            cmd_reply r;
            r.type = reply_type::STATUS;
            r.str_value = value;
            return r;
        }

        [[nodiscard]] reply_type get_type() const {
            return type;
        }

        [[nodiscard]] const std::optional<std::string> &get_string() const {
            return str_value;
        }
        [[nodiscard]] const std::optional<uint64_t> &get_integer() const {
            return int_value;
        }
        [[nodiscard]] const std::optional<double> &get_double() const {
            return double_value;
        }
        [[nodiscard]] const std::optional<bool> &get_bool() const {
            return bool_value;
        }
        [[nodiscard]] const std::optional<std::vector<cmd_reply>> &get_array() const {
            return array_value;
        }

        [[nodiscard]] bool is_nil() const {
            return type == reply_type::NIL;
        }

        [[nodiscard]] bool is_error() const {
            return type == reply_type::ERROR;
        }

    private:
        reply_type type;

        std::optional<std::string> str_value;

        std::optional<uint64_t> int_value;

        std::optional<double> double_value;

        std::optional<bool> bool_value;

        std::optional<std::vector<cmd_reply>> array_value;
    };
} // namespace redisdal
