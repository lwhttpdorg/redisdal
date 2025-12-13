#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace janus {
	template<typename T>
	class serializer {
	public:
		// Convert object of type T into a string
		virtual std::string serialize(const T &t) const = 0;

		// Convert string back into object of type T
		virtual T deserialize(const std::string &data) const = 0;

		virtual ~serializer() = default;
	};

	// Primary template declaration (specializations will follow)
	template<typename T, typename Enable = void>
	class string_serializable;

	// Specialization for arithmetic types (int, float, double, etc.)
	template<typename T>
	class string_serializable<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
	public:
		// Use std::to_string for serialization
		static std::string to_string(const T &t) {
			if constexpr (std::is_same_v<T, bool>) {
				return t ? "true" : "false";
			}
			return std::to_string(t);
		}

		// Use standard library functions for deserialization
		static T from_string(const std::string &s) {
			if constexpr (std::is_same_v<T, bool>) {
				std::string lower_s;
				lower_s.resize(s.size());
				std::transform(s.begin(), s.end(), lower_s.begin(), [](unsigned char c) { return std::tolower(c); });

				if (lower_s == "true" || lower_s == "1") {
					return true;
				}
				if (lower_s == "false" || lower_s == "0") {
					return false;
				}
				throw std::invalid_argument(
					"Invalid string format for boolean type. Expected 'true', 'false', '1', or '0'.");
			}
			if constexpr (std::is_integral_v<T>) {
				if constexpr (std::is_signed_v<T>) {
					if constexpr (sizeof(T) <= sizeof(int))
						return static_cast<T>(std::stoi(s));
					else if constexpr (sizeof(T) <= sizeof(long))
						return static_cast<T>(std::stol(s));
					else if constexpr (sizeof(T) <= sizeof(long long))
						return static_cast<T>(std::stoll(s));
				}
				else {
					if constexpr (sizeof(T) <= sizeof(unsigned long))
						return static_cast<T>(std::stoul(s));
					else if constexpr (sizeof(T) <= sizeof(unsigned long long))
						return static_cast<T>(std::stoull(s));
				}
			}
			else if constexpr (std::is_floating_point_v<T>) {
				if constexpr (std::is_same_v<T, float>)
					return std::stof(s);
				else if constexpr (std::is_same_v<T, double>)
					return std::stod(s);
				else if constexpr (std::is_same_v<T, long double>)
					return std::stold(s);
			}
			throw std::invalid_argument("Unsupported arithmetic type for from_string");
		}
	};

	// Specialization for std::string
	template<>
	class string_serializable<std::string> {
	public:
		// For std::string, serialization is just identity
		static std::string to_string(const std::string &t) {
			return t;
		}

		// For std::string, deserialization is also identity
		static std::string from_string(const std::string &s) {
			return s;
		}
	};

	// Specialization for non-arithmetic types
	// Users must provide their own implementation of to_string and from_string
	template<typename T>
	class string_serializable<T, std::enable_if_t<!std::is_arithmetic_v<T>>> {
	public:
		static std::string to_string(const T &) {
			static_assert(
				sizeof(T) == 0,
				"Non-arithmetic types must provide their own specialization of string_serializable<T>::to_string");
			return {};
		}

		static T from_string(const std::string &) {
			static_assert(
				sizeof(T) == 0,
				"Non-arithmetic types must provide their own specialization of string_serializable<T>::from_string");
			return {};
		}
	};

	// Serializer wrapper that delegates to string_serializable<T>
	template<typename T>
	class string_serializer: public serializer<T> {
	public:
		std::string serialize(const T &obj) const override {
			return string_serializable<T>::to_string(obj);
		}

		T deserialize(const std::string &bytes) const override {
			return string_serializable<T>::from_string(bytes);
		}
	};
}
