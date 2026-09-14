#include "redisdal/common.hpp"

#include <stdexcept>
#include <utility>

#include "redisdal/exception.hpp"
#include "redisdal/serialization.hpp"

namespace redisdal {
    const char *cmd_reply::type_name() const {
        switch (type) {
            case reply_type::STRING:
                return "string";
            case reply_type::ARRAY:
                return "array";
            case reply_type::INTEGER:
                return "integer";
            case reply_type::NIL:
                return "nil";
            case reply_type::STATUS:
                return "status";
            case reply_type::ERROR:
                return "error";
            case reply_type::DOUBLE:
                return "double";
            case reply_type::BOOL:
                return "bool";
        }
        return "unknown";
    }

    const std::string &cmd_reply::as_text(const std::string &command) const {
        if (!str_value) {
            throw unexpected_reply_type_error(command, "text", type_name());
        }
        return *str_value;
    }

    const std::string &cmd_reply::as_string(const std::string &command) const {
        if (type != reply_type::STRING) {
            throw unexpected_reply_type_error(command, "string", type_name());
        }
        return as_text(command);
    }

    const std::string &cmd_reply::as_status(const std::string &command) const {
        if (type != reply_type::STATUS) {
            throw unexpected_reply_type_error(command, "status", type_name());
        }
        return as_text(command);
    }

    const std::vector<cmd_reply> &cmd_reply::as_array(const std::string &command) const {
        if (type != reply_type::ARRAY) {
            throw unexpected_reply_type_error(command, "array", type_name());
        }
        return *array_value;
    }

    int64_t cmd_reply::as_integer(const std::string &command) const {
        if (type != reply_type::INTEGER) {
            throw unexpected_reply_type_error(command, "integer", type_name());
        }
        return *get_signed_integer();
    }

    bool cmd_reply::status_is_ok(const std::string &command) const {
        return as_status(command) == "OK";
    }

    std::optional<std::string> cmd_reply::optional_string(const std::string &command) const {
        if (is_nil()) {
            return std::nullopt;
        }
        return as_string(command);
    }

    std::vector<std::string> cmd_reply::strings(const std::string &command) const {
        if (is_nil()) {
            return {};
        }
        const auto &items = as_array(command);
        std::vector<std::string> values;
        values.reserve(items.size());
        for (const auto &item: items) {
            values.push_back(item.as_string(command));
        }
        return values;
    }

    std::unordered_map<std::string, std::string> cmd_reply::hash(const std::string &command) const {
        const auto &items = as_array(command);
        if (items.size() % 2 != 0) {
            throw std::runtime_error(command + ": expected an even number of field-value elements");
        }
        std::unordered_map<std::string, std::string> fields;
        fields.reserve(items.size() / 2);
        for (size_t i = 0; i < items.size(); i += 2) {
            fields.emplace(items[i].as_string(command), items[i + 1].as_string(command));
        }
        return fields;
    }

    std::optional<double> cmd_reply::optional_score(const std::string &command) const {
        const auto text = optional_string(command);
        if (!text) {
            return std::nullopt;
        }
        try {
            return string_serializable<double>::from_string(*text);
        }
        catch (const std::exception &) {
            throw std::runtime_error(command + ": failed to convert score to double");
        }
    }

    std::vector<std::pair<std::string, double>> cmd_reply::scores(const std::string &command) const {
        if (is_nil()) {
            return {};
        }
        const auto &items = as_array(command);
        if (items.size() % 2 != 0) {
            throw std::runtime_error(command + ": expected an even number of member-score elements");
        }
        std::vector<std::pair<std::string, double>> result;
        result.reserve(items.size() / 2);
        for (size_t i = 0; i < items.size(); i += 2) {
            result.emplace_back(items[i].as_string(command), items[i + 1].optional_score(command).value());
        }
        return result;
    }

    uint64_t cmd_reply::scan_cursor(const std::string &command) const {
        const auto &parts = as_array(command);
        if (parts.size() != 2) {
            throw std::runtime_error(command + ": expected a cursor and result array");
        }
        [[maybe_unused]] const auto &results = parts[1].as_array(command);
        return string_serializable<uint64_t>::from_string(parts[0].as_string(command));
    }

    std::vector<string_stream_entry> cmd_reply::stream_entries(const std::string &command) const {
        if (is_nil()) {
            return {};
        }
        const auto &entries = as_array(command);
        std::vector<string_stream_entry> result;
        result.reserve(entries.size());
        for (const auto &entry: entries) {
            const auto &parts = entry.as_array(command);
            if (parts.size() != 2) {
                throw std::runtime_error(command + ": expected a two-element stream entry");
            }
            string_stream_entry parsed;
            parsed.id = parts[0].as_string(command);
            if (!parts[1].is_nil()) {
                const auto &fields = parts[1].as_array(command);
                if (fields.size() % 2 != 0) {
                    throw std::runtime_error(command + ": expected an even number of field-value elements");
                }
                parsed.fields.reserve(fields.size() / 2);
                for (size_t i = 0; i < fields.size(); i += 2) {
                    parsed.fields.emplace_back(fields[i].as_string(command), fields[i + 1].as_string(command));
                }
            }
            result.push_back(std::move(parsed));
        }
        return result;
    }

    std::vector<string_stream_batch> cmd_reply::stream_batches(const std::string &command) const {
        if (is_nil()) {
            return {};
        }
        const auto &batches = as_array(command);
        std::vector<string_stream_batch> result;
        result.reserve(batches.size());
        for (const auto &batch: batches) {
            const auto &parts = batch.as_array(command);
            if (parts.size() != 2) {
                throw std::runtime_error(command + ": expected a two-element stream batch");
            }
            result.push_back({parts[0].as_string(command), parts[1].stream_entries(command)});
        }
        return result;
    }

    cmd_reply cmd_reply::or_throw() && {
        throw_if_error();
        return std::move(*this);
    }

    void cmd_reply::throw_if_error() const {
        if (is_error()) {
            throw redis_error(as_text("Redis command"));
        }
    }
} // namespace redisdal
