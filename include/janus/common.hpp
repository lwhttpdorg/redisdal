#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace janus {
	template<typename K>
	class scan_result {
	public:
		uint64_t cursor;
		std::unordered_set<K> keys;
	};

	/**
	 * @class cmd_result
	 * @brief A class to encapsulate the result of a command execution.
	 */
	class cmd_result {
	public:
		enum class result_type { STRING, ARRAY, INTEGER, NIL, STATUS, ERROR, DOUBLE, BOOL };

		cmd_result() : type(result_type::NIL) {
		}
		explicit cmd_result(const std::string &value) : type(result_type::STRING), str_value(value) {
		}
		explicit cmd_result(const char *value) : type(result_type::STRING), str_value(value) {
		}
		explicit cmd_result(uint64_t value) : type(result_type::INTEGER), int_value(value) {
		}
		explicit cmd_result(double value) : type(result_type::DOUBLE), double_value(value) {
		}
		explicit cmd_result(bool value) : type(result_type::BOOL), bool_value(value) {
		}
		explicit cmd_result(const std::vector<cmd_result> &value) : type(result_type::ARRAY), array_value(value) {
		}
		explicit cmd_result(std::vector<cmd_result> &&value) : type(result_type::ARRAY), array_value(std::move(value)) {
		}

		static cmd_result make_nil() {
			return {};
		}

		static cmd_result make_error(const std::string &message) {
			cmd_result r;
			r.type = result_type::ERROR;
			r.str_value = message;
			return r;
		}
		static cmd_result make_string(const std::string &value) {
			return cmd_result(value);
		}
		static cmd_result make_integer(uint64_t value) {
			return cmd_result(value);
		}
		static cmd_result make_double(double value) {
			return cmd_result(value);
		}
		static cmd_result make_bool(bool value) {
			return cmd_result(value);
		}
		static cmd_result make_array(const std::vector<cmd_result> &value) {
			return cmd_result(value);
		}
		static cmd_result make_array(std::vector<cmd_result> &&value) {
			return cmd_result(std::move(value));
		}
		static cmd_result make_status(const std::string &value) {
			cmd_result r;
			r.type = result_type::STATUS;
			r.str_value = value;
			return r;
		}

		[[nodiscard]] result_type get_type() const {
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
		[[nodiscard]] const std::optional<std::vector<cmd_result>> &get_array() const {
			return array_value;
		}

		[[nodiscard]] bool is_nil() const {
			return type == result_type::NIL;
		}

		[[nodiscard]] bool is_error() const {
			return type == result_type::ERROR;
		}

	private:
		result_type type;

		std::optional<std::string> str_value;

		std::optional<uint64_t> int_value;

		std::optional<double> double_value;

		std::optional<bool> bool_value;

		std::optional<std::vector<cmd_result>> array_value;
	};
}
