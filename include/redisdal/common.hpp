#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace redisdal {
    template<typename K>
    class scan_result {
    public:
        uint64_t cursor;
        std::unordered_set<K> keys;
    };

    /**
     * @brief The trimming strategy used by XADD and XTRIM.
     */
    enum class stream_trim_strategy : std::uint8_t { MAXLEN, MINID };

    /**
     * @brief Options that describe an XADD/XTRIM trimming clause.
     */
    struct stream_trim_options {
        stream_trim_strategy strategy{stream_trim_strategy::MAXLEN};
        std::string threshold;
        bool approximate{false};
        std::optional<long long> limit;

        static stream_trim_options maxlen(uint64_t max_length, bool approximate = false,
                                          std::optional<long long> limit = std::nullopt) {
            return {stream_trim_strategy::MAXLEN, std::to_string(max_length), approximate, limit};
        }

        static stream_trim_options minid(const std::string &minimum_id, bool approximate = false,
                                         std::optional<long long> limit = std::nullopt) {
            return {stream_trim_strategy::MINID, minimum_id, approximate, limit};
        }
    };

    /**
     * @brief Options for appending a stream entry with XADD.
     */
    struct stream_add_options {
        std::string id{"*"};
        bool nomkstream{false};
        std::optional<stream_trim_options> trim;
    };

    /**
     * @brief Options shared by XREAD calls.
     */
    struct stream_read_options {
        std::optional<long long> count;
        std::optional<long long> block_ms;
    };

    /**
     * @brief Options for XREADGROUP calls.
     */
    struct stream_read_group_options {
        std::optional<long long> count;
        std::optional<long long> block_ms;
        bool noack{false};
    };

    /**
     * @brief A field-value entry stored in a Redis Stream.
     *
     * A vector is intentionally used for fields because Redis preserves field order
     * and permits repeated field names inside an entry.
     */
    template<typename K, typename V>
    struct stream_entry {
        std::string id;
        std::vector<std::pair<K, V>> fields;
    };

    /**
     * @brief A stream key and the entries returned for it by XREAD/XREADGROUP.
     */
    template<typename K, typename V>
    struct stream_batch {
        K key;
        std::vector<stream_entry<K, V>> entries;
    };

    /**
     * @brief A stream key and the ID from which it should be read.
     */
    template<typename K>
    struct stream_read_request {
        K key;
        std::string id;
    };

    using string_stream_entry = stream_entry<std::string, std::string>;
    using string_stream_batch = stream_batch<std::string, std::string>;
    using string_stream_read_request = stream_read_request<std::string>;

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
        static cmd_reply make_signed_integer(int64_t value) {
            return cmd_reply(static_cast<uint64_t>(value));
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
        /** Signed Redis integer view; get_integer() retains its legacy unsigned representation. */
        [[nodiscard]] std::optional<int64_t> get_signed_integer() const {
            if (!int_value) {
                return std::nullopt;
            }
            if (*int_value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                return static_cast<int64_t>(*int_value);
            }
            return -1 - static_cast<int64_t>(std::numeric_limits<uint64_t>::max() - *int_value);
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
        friend class redis_cmd_ops;
        friend class redis_client;

        [[nodiscard]] const char *type_name() const;
        [[nodiscard]] const std::string &as_text(const std::string &command) const;
        [[nodiscard]] const std::string &as_string(const std::string &command) const;
        [[nodiscard]] const std::string &as_status(const std::string &command) const;
        [[nodiscard]] const std::vector<cmd_reply> &as_array(const std::string &command) const;
        [[nodiscard]] int64_t as_integer(const std::string &command) const;
        [[nodiscard]] bool status_is_ok(const std::string &command) const;
        [[nodiscard]] std::optional<std::string> optional_string(const std::string &command) const;
        [[nodiscard]] std::vector<std::string> strings(const std::string &command) const;
        [[nodiscard]] std::unordered_map<std::string, std::string> hash(const std::string &command) const;
        [[nodiscard]] std::optional<double> optional_score(const std::string &command) const;
        [[nodiscard]] std::vector<std::pair<std::string, double>> scores(const std::string &command) const;
        [[nodiscard]] uint64_t scan_cursor(const std::string &command) const;
        [[nodiscard]] std::vector<string_stream_entry> stream_entries(const std::string &command) const;
        [[nodiscard]] std::vector<string_stream_batch> stream_batches(const std::string &command) const;
        [[nodiscard]] cmd_reply or_throw() &&;
        void throw_if_error() const;

        reply_type type;

        std::optional<std::string> str_value;

        std::optional<uint64_t> int_value;

        std::optional<double> double_value;

        std::optional<bool> bool_value;

        std::optional<std::vector<cmd_reply>> array_value;
    };
} // namespace redisdal
